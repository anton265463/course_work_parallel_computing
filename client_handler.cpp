#include "client_handler.h"

#include <future>
#include <iostream>


ClientHandler::ClientHandler(InvertedIndex& idx, ThreadPool& pool)
    : index(idx), threadPool(pool) {}

SearchResponse ClientHandler::handle(const SearchRequest& req) {
    SearchResponse resp;
    if (!req.valid()) {
        resp.ok = false;
        resp.message = "Порожній запит";
        return resp;
    }

    std::istringstream iss(req.raw);
    std::string token;
    std::vector<std::string> phrase;
    while (iss >> token) {
        std::transform(token.begin(), token.end(), token.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        phrase.push_back(token);
    }

    if (phrase.empty()) {
        resp.ok = false;
        resp.message = "Запит не містить слів";
        return resp;
    }

    auto start = std::chrono::high_resolution_clock::now();
    auto results = searchPhrase(phrase);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    resp.ok = !results.empty();
    resp.files = results;
    resp.query = req.raw;
    resp.durationMs = duration;

    std::cout << "\n[ПОШУК] \"" << resp.query
              << "\" | Час: " << resp.durationMs << " мс"
              << " | Результатів: " << resp.files.size()
              << std::endl;
    std::cout << "DEBUG: використано потоків = "
              << threadPool.getMaxActive()
              << " / " << threadPool.getTotalThreads() << "\n";

    return resp;
}

std::vector<std::string> ClientHandler::searchPhrase(const std::vector<std::string>& phrase) {
    std::vector<std::string> results;
    const auto& idx = index.getIndex();

    auto it = idx.find(phrase[0]);
    if (it == idx.end()) return results;

    std::mutex resMutex;
    std::atomic<int> counter(it->second.size());
    std::promise<void> done;
    auto fut = done.get_future();

    for (const auto& p : it->second) {
        threadPool.enqueue([&, p]{
            bool match = true;
            for (size_t i = 1; i < phrase.size(); ++i) {
                auto it2 = idx.find(phrase[i]);
                if (it2 == idx.end()) { match = false; break; }

                bool found = false;
                for (const auto& p2 : it2->second) {
                    if (p2.file == p.file && p2.position == p.position + i) {
                        found = true;
                        break;
                    }
                }
                if (!found) { match = false; break; }
            }
            if (match) {
                std::lock_guard<std::mutex> lock(resMutex);
                results.push_back(p.file);
            }
            if (--counter == 0) done.set_value();
        });
    }

    fut.wait();
    return results;
}
