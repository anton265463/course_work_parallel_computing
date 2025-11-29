#ifndef INDEX_BUILDER_H_INCLUDED
#define INDEX_BUILDER_H_INCLUDED

#pragma once
#include "inverted_index.h"
#include "thread_pool.h"

#include <regex>
#include <atomic>

int indexThreads = 8;
std::atomic<bool> running(true);

inline std::string cleanWord(const std::string& word) {
    std::string noTags = std::regex_replace(word, std::regex("<[^>]*>"), "");
    std::string result;
    for (char ch : noTags) {
        if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '\'' || ch == '-') {
            result.push_back(std::tolower(static_cast<unsigned char>(ch)));
        }
    }

    if (result.empty() || result == "-") return "";
    return result;
}

class FileSet {
private:
    struct Bucket {
        std::string key;
        bool used = false;
    };

    std::vector<Bucket> table;
    size_t count;

    size_t hash(const std::string& s) const {
        return std::hash<std::string>{}(s) % table.size();
    }

public:
    FileSet(size_t capacity = 1024) : table(capacity), count(0) {}

    bool contains(const std::string& key) const {
        size_t idx = hash(key);
        for (size_t i = 0; i < table.size(); ++i) {
            size_t probe = (idx + i) % table.size();
            if (!table[probe].used) return false;
            if (table[probe].key == key) return true;
        }
        return false;
    }

    void insert(const std::string& key) {
        if (contains(key)) return;

        size_t idx = hash(key);
        for (size_t i = 0; i < table.size(); ++i) {
            size_t probe = (idx + i) % table.size();
            if (!table[probe].used) {
                table[probe].key = key;
                table[probe].used = true;
                ++count;
                return;
            }
        }
    }
};


class IndexBuilder {
public:
    explicit IndexBuilder(InvertedIndex& idx) : index(idx) {}

    void buildFile(const std::string& filepath) {
        std::ifstream in(filepath);
        if (!in.is_open()) return;

        std::string filename = std::filesystem::path(filepath).filename().string();
        std::string prefix;
        if (filepath.find("/train/neg") != std::string::npos) prefix = "train_neg_";
        else if (filepath.find("/train/pos") != std::string::npos) prefix = "train_pos_";
        else if (filepath.find("/train/unsup") != std::string::npos) prefix = "train_unsup_";
        else if (filepath.find("/test/neg") != std::string::npos) prefix = "test_neg_";
        else if (filepath.find("/test/pos") != std::string::npos) prefix = "test_pos_";
        else prefix = "added_";

        std::string renamed = prefix + filename;

        std::string line;
        size_t position = 0;
        while (std::getline(in, line)) {
            std::istringstream iss(line);
            std::string word;
            while (iss >> word) {
                std::string cleaned = cleanWord(word);
                if (!cleaned.empty()) {
                    std::lock_guard<std::mutex> lock(indexMutex);
                    index.addWord(cleaned, renamed, position++);
                }
            }
        }
    }

    void buildFiles(const std::vector<std::string>& files, size_t numThreads) {
        ThreadPool pool(numThreads);
        for (const auto& f : files) {
            pool.enqueue([this, f]() { buildFile(f); });
        }
    }

    static std::vector<std::string> listTextFiles(const std::string& dir) {
        namespace fs = std::filesystem;
        std::vector<std::string> result;
        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".txt") {
                result.push_back(entry.path().string());
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    void schedulerUpdate(InvertedIndex& index, IndexBuilder& builder, const std::string& dir) {
        FileSet indexedFiles;

        while (running) {
            std::vector<std::string> files = IndexBuilder::listTextFiles(dir);
            std::vector<std::string> newFiles;

            for (const auto& f : files) {
                if (!indexedFiles.contains(f)) {
                    newFiles.push_back(f);
                    indexedFiles.insert(f);
                }
            }

            if (!newFiles.empty()) {
                std::lock_guard<std::mutex> lock(indexMutex);
                builder.buildFiles(newFiles, indexThreads);
                std::cout << "[Scheduler] Оновлено індекс: додано "
                          << newFiles.size() << " нових файлів\n";
            }

            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }

private:
    InvertedIndex& index;
    std::mutex indexMutex;
};

#endif // INDEX_BUILDER_H_INCLUDED
