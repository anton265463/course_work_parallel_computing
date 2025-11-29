#ifndef CLIENT_HANDLER_H_INCLUDED
#define CLIENT_HANDLER_H_INCLUDED

#pragma once
#include "inverted_index.h"
#include "search_models.h"
#include "thread_pool.h"
#include <vector>
#include <string>

class ClientHandler {
public:
    ClientHandler(InvertedIndex& idx, ThreadPool& pool);

    SearchResponse handle(const SearchRequest& req);

private:
    InvertedIndex& index;
    ThreadPool& threadPool;

    std::vector<std::string> searchPhrase(const std::vector<std::string>& phrase);
};

#endif // CLIENT_HANDLER_H_INCLUDED
