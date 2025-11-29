#include "search_models.h"
#include "client_handler.h"
#include "inverted_index.h"
#include "thread_pool.h"

#include <iostream>


// зовнішній м’ютекс (оголошений у головному файлі)
extern std::mutex indexMutex;

SearchRequest SearchRequest::parseLine(const std::string& line) {
    SearchRequest req;
    req.raw = line;
    req.tokens = tokenize(line);
    return req;
}

std::string resolvePath(const std::string& shortName) {
    const std::string baseDir = "C:/Users/DEll/Desktop/CW/aclImdb/";

    if (shortName.rfind("train_unsup_", 0) == 0) {
        return baseDir + "train/unsup/" + shortName.substr(std::string("train_unsup_").size());
    } else if (shortName.rfind("train_pos_", 0) == 0) {
        return baseDir + "train/pos/" + shortName.substr(std::string("train_pos_").size());
    } else if (shortName.rfind("train_neg_", 0) == 0) {
        return baseDir + "train/neg/" + shortName.substr(std::string("train_neg_").size());
    } else if (shortName.rfind("test_pos_", 0) == 0) {
        return baseDir + "test/pos/" + shortName.substr(std::string("test_pos_").size());
    } else if (shortName.rfind("test_neg_", 0) == 0) {
        return baseDir + "test/neg/" + shortName.substr(std::string("test_neg_").size());
    } else if (shortName.rfind("added_", 0) == 0) {
        return baseDir + "added/" + shortName.substr(std::string("added_").size());
    }

    return shortName;
}

std::string fileWithCaps(const std::string& filepath, const std::string& phrase, int mode) {
    std::ifstream in(filepath);
    if (!in.is_open()) return "ERROR\n";

    std::ostringstream out;
    std::string line;

    std::string lowerPhrase = phrase;
    std::transform(lowerPhrase.begin(), lowerPhrase.end(), lowerPhrase.begin(), ::tolower);

    bool isPhrase = (phrase.find(' ') != std::string::npos);

    while (std::getline(in, line)) {
        std::string processed;
        if (isPhrase) {
            std::string lowerLine = line;
            std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

            size_t pos = 0;
            while ((pos = lowerLine.find(lowerPhrase, pos)) != std::string::npos) {
                std::string caps = phrase;
                for (char& c : caps) c = std::toupper(c);

                std::string highlight = (mode == 0)
                    ? "\x1b[47m\x1b[30m" + caps + "\x1b[0m"
                    : "<mark>" + caps + "</mark>";

                line.replace(pos, phrase.size(), highlight);
                pos += highlight.size();
                lowerLine.replace(pos - highlight.size(), phrase.size(), std::string(phrase.size(), '*'));
            }
            processed = line;
        } else {
            std::ostringstream tmp;
            std::istringstream iss(line);
            std::string word;
            while (iss >> word) {
                std::string cleaned = word;
                std::transform(cleaned.begin(), cleaned.end(), cleaned.begin(), ::tolower);
                if (cleaned == lowerPhrase) {
                    tmp << ((mode == 0)
                        ? "\x1b[47m\x1b[30m" + word + "\x1b[0m "
                        : "<mark>" + word + "</mark> ");
                } else {
                    tmp << word << " ";
                }
            }
            processed = tmp.str();
        }
        out << processed << "\n";
    }
    return out.str();
}

bool SearchRequest::valid() const {
    return !tokens.empty();
}

std::vector<std::string> SearchRequest::tokenize(const std::string& text) {
    std::istringstream iss(text);
    std::string token;
    std::vector<std::string> tokens;
    while (iss >> token) {
        std::transform(token.begin(), token.end(), token.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        tokens.push_back(token);
    }
    return tokens;
}

std::string SearchResponse::serialize() const {
    std::ostringstream out;
    if (!ok) return "ERROR " + message + "\n";
    return out.str();
}

void handleSearchClient(SOCKET clientSocket, InvertedIndex& index, ThreadPool& pool) {
    char buffer[2048];
    int received = recv(clientSocket, buffer, sizeof(buffer)-1, 0);
    if (received <= 0) { closesocket(clientSocket); return; }
    buffer[received] = '\0';
    std::string line(buffer);

    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();

    SearchRequest req = SearchRequest::parseLine(line);
    ClientHandler handler(index, pool);

    SearchResponse resp;
    {
        std::lock_guard<std::mutex> lock(indexMutex);
        resp = handler.handle(req);
    }

    std::cout << resp.serialize() << std::endl;

    if (!resp.ok || resp.files.empty()) {
        std::string responseText = "ERROR Нічого не знайдено. Спробуйте ще раз.\n";
        send(clientSocket, responseText.c_str(), responseText.size(), 0);
        closesocket(clientSocket);
        return;
    }

    std::ostringstream out;
    size_t limit = std::min<size_t>(resp.files.size(), 10);
    out << "Знайдено " << resp.files.size() << " файлів:\n";
    for (size_t i = 0; i < limit; ++i) {
        out << resp.files[i] << "\n";
    }

    std::string responseText = out.str();
    send(clientSocket, responseText.c_str(), responseText.size(), 0);

    char buf2[2048];
    int rec2 = recv(clientSocket, buf2, sizeof(buf2)-1, 0);
    if (rec2 <= 0) { closesocket(clientSocket); return; }
    buf2[rec2] = '\0';
    std::string chosen(buf2);
    while (!chosen.empty() && (chosen.back() == '\r' || chosen.back() == '\n'))
        chosen.pop_back();

    std::cout << "DEBUG: користувач вибрав файл: " << chosen << std::endl;

    std::string fullPath = resolvePath(chosen);
    std::cout << "DEBUG: fullPath = " << fullPath << std::endl;
    std::cout << "DEBUG: req.raw = \"" << req.raw << "\"" << std::endl;
    std::cout << "DEBUG: req.tokens = [ ";
    for (const auto& t : req.tokens) {
        std::cout << "\"" << t << "\" ";
    }
    std::cout << "]\n" << std::endl;

    std::string fileText = fileWithCaps(fullPath, req.raw);
    std::string finalResponse = "=== Вміст файлу ===\n" + fileText;
    send(clientSocket, finalResponse.c_str(), finalResponse.size(), 0);

    closesocket(clientSocket);
}

std::string decodeQuery(const std::string& q) {
    std::string res = q;
    while (true) {
        size_t pos = res.find("%20");
        if (pos == std::string::npos) break;
        res.replace(pos, 3, " ");
    }
    std::replace(res.begin(), res.end(), '+', ' ');
    while (!res.empty() && std::isspace((unsigned char)res.front())) res.erase(res.begin());
    while (!res.empty() && std::isspace((unsigned char)res.back())) res.pop_back();
    return res;
}

void handleWebClient(SOCKET clientSocket,
                     InvertedIndex& index,
                     ThreadPool& pool,
                     std::vector<std::string>& lastResults,
                     std::string& lastQuery,
                     int pages) {
    char buffer[4096];
    int received = recv(clientSocket, buffer, sizeof(buffer)-1, 0);
    if (received <= 0) { closesocket(clientSocket); return; }
    buffer[received] = '\0';
    std::string request(buffer);

    std::string query;
    std::string chosenFile;
    int page = 0;

    size_t pos = request.find("GET /?q=");
    if (pos != std::string::npos) {
        pos += 8;
        size_t end = request.find(' ', pos);
        std::string params = request.substr(pos, end - pos);

        std::istringstream iss(params);
        std::string token;
        while (std::getline(iss, token, '&')) {
            if (token.rfind("page=",0)==0) {
                page = std::stoi(token.substr(5));
            } else if (token.rfind("file=",0)==0) {
                chosenFile = token.substr(5);
            } else {
                query = token;
            }
        }
    }

    query = decodeQuery(query);
    chosenFile = decodeQuery(chosenFile);

    if (query.empty()) {
        std::string htmlPage =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n\r\n"
            "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Search</title></head>"
            "<body><h2>Search engine</h2>"
            "<form method='GET'>"
            "<input type='text' name='q'/>"
            "<input type='submit' value='Search'/>"
            "</form></body></html>";

        send(clientSocket, htmlPage.c_str(), (int)htmlPage.size(), 0);
    }
    else if (!chosenFile.empty()) {
        std::string fullPath = resolvePath(chosenFile);
        SearchRequest req = SearchRequest::parseLine(query);
        std::string fileText = fileWithCaps(fullPath, query, WEB);

        std::string safeQuery;
        for (char c : query) safeQuery += (c == ' ' ? '+' : c);

        std::ostringstream out;
        out << "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>File</title></head><body>";
        out << "<h3>File: " << chosenFile << "</h3>";
        out << "<pre>" << fileText << "</pre>";
        out << "<br><a href='/?q=" << safeQuery << "'>Back to list</a>";
        out << "</body></html>";

        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n\r\n" + out.str();

        send(clientSocket, response.c_str(), (int)response.size(), 0);
    }
    else {
        if (page == 0 || query != lastQuery) {
            SearchRequest req = SearchRequest::parseLine(query);
            ClientHandler handler(index, pool);
            SearchResponse resp = handler.handle(req);
            if (resp.ok) {
                lastResults = resp.files;
                lastQuery = query;
            } else {
                lastResults.clear();
            }
        }

        std::ostringstream out;
        out << "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Results</title></head><body>";
        if (lastResults.empty()) {
            out << "<p>No results: <b>" << query << "</b></p>";
        } else {
            int total = (int)lastResults.size();
            int start = page * pages;
            int end = std::min(start + pages, total);

            out << "<h3>Found " << total << " files:</h3><ul>";
            for (int i = start; i < end; ++i) {
                out << "<li><a href='/?q=" << query << "&file=" << lastResults[i] << "'>"
                    << lastResults[i] << "</a></li>";
            }
            out << "</ul>";

            int totalPages = (total + pages - 1) / pages;
            out << "<p>Page " << (page+1) << " of " << totalPages << "</p>";

            if (page > 0) {
                out << "<a href='/?q=" << query << "&page=" << (page-1) << "'>Previous</a> ";
            }
            if (end < total) {
                out << "<a href='/?q=" << query << "&page=" << (page+1) << "'>Next</a>";
            }
        }
        out << "<br><a href='/'>Go back</a></body></html>";

        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n\r\n" + out.str();

        send(clientSocket, response.c_str(), (int)response.size(), 0);
    }

    closesocket(clientSocket);
}
