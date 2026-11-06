#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <shared_mutex>
#include <span>
#include <unordered_map>
#include <vector>

class FileCollector {
public:
    FileCollector() = default;
    ~FileCollector() = default;

    FileCollector(const FileCollector&) = delete;
    FileCollector& operator=(const FileCollector&) = delete;
    FileCollector(FileCollector&&) = delete;
    FileCollector& operator=(FileCollector&&) = delete;

    void CollectFile(uint32_t file_id, size_t file_size);
    void OnNewChunk(uint32_t file_id, size_t pos, std::vector<uint8_t>&& chunk);

    [[nodiscard]] std::shared_ptr<const std::vector<uint8_t>> GetFile(uint32_t file_id) const;
    [[nodiscard]] bool
    ReadExact(uint32_t file_id, size_t pos, std::span<uint8_t> destination) const;
    [[nodiscard]] bool IsComplete(uint32_t file_id) const;
    [[nodiscard]] size_t GetFileSize(uint32_t file_id) const;
    [[nodiscard]] size_t GetReceivedBytes(uint32_t file_id) const;

private:
    struct FileEntry {
        explicit FileEntry(size_t size);

        size_t total_size = 0;
        size_t collected_bytes = 0;
        std::shared_ptr<std::vector<uint8_t>> data;
        std::map<size_t, size_t> intervals;
        mutable std::shared_mutex mutex;
    };

    mutable std::shared_mutex registry_mutex_;
    std::unordered_map<uint32_t, std::shared_ptr<FileEntry>> files_;
};
