#ifndef SEARCH_MODELS_H_INCLUDED
#define SEARCH_MODELS_H_INCLUDED

#pragma once
#include <string>
#include <vector>
#include <winsock2.h>

struct InvertedIndex;
class ThreadPool;
enum OutputMode {
    CONSOLE,
    WEB
};

struct SearchRequest {
    std::string raw;
    std::vector<std::string> tokens;

    static SearchRequest parseLine(const std::string& line);
    bool valid() const;

private:
    static std::vector<std::string> tokenize(const std::string& text);
};

struct SearchResponse {
    bool ok = false;
    std::string message;
    std::vector<std::string> files;

    std::string query;
    long long durationMs=0;

    std::string serialize() const;
};

void handleSearchClient(SOCKET clientSocket, InvertedIndex& index, ThreadPool& pool);

void handleWebClient(SOCKET clientSocket,
                     InvertedIndex& index,
                     ThreadPool& pool,
                     std::vector<std::string>& lastResults,
                     std::string& lastQuery,
                     int pages);

std::string decodeQuery(const std::string& q);
std::string resolvePath(const std::string& shortName);
std::string fileWithCaps(const std::string& filepath, const std::string& phrase, int mode = 1);

#endif // SEARCH_MODELS_H_INCLUDED
