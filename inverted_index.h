#ifndef INVERTED_INDEX_H_INCLUDED
#define INVERTED_INDEX_H_INCLUDED

#pragma once
#include <mutex>
#include <shared_mutex>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <algorithm>
#include <filesystem>
using namespace std::filesystem;

struct Posting {
    std::string file;
    size_t position;
};

class InvertedIndex {
public:
    void addWord(const std::string& word, const std::string& file, size_t position) {
        std::unique_lock lock(mutex_);
        index[word].push_back({file, position});
        ++totalWords_;
    }

    const std::unordered_map<std::string, std::vector<Posting>>& getIndex() const {
        return index;
    }

    size_t totalWords() const { return totalWords_; }
    size_t uniqueWords() const { return index.size(); }

    void exportToFile(const std::string& outFile) const {
        std::ofstream out(outFile);
        if (!out.is_open()) return;

        for (const auto& [word, postings] : index) {
            out << word << " -> ";
            for (size_t i = 0; i < postings.size(); ++i) {
                out << std::filesystem::path(postings[i].file).filename().string()
                    << "@" << postings[i].position;
                if (i + 1 < postings.size()) out << ", ";
            }
            out << "\n";
        }
    }

private:
    std::unordered_map<std::string, std::vector<Posting>> index;
    size_t totalWords_ = 0;
    mutable std::shared_mutex mutex_;
};

#endif // INVERTED_INDEX_H_INCLUDED
