#include "file_collector.hpp"

#include <algorithm>
#include <cstring>

FileCollector::FileEntry::FileEntry(size_t size)
    : total_size(size)
    , data(std::make_shared<std::vector<uint8_t>>(size, 0)) {
}

void FileCollector::CollectFile(uint32_t file_id, size_t file_size) {
    auto entry = std::make_shared<FileEntry>(file_size);

    const std::unique_lock lock(registry_mutex_);
    files_[file_id] = std::move(entry);
}

void FileCollector::OnNewChunk(uint32_t file_id, size_t pos, std::vector<uint8_t>&& chunk) {
    if (chunk.empty()) {
        return;
    }

    std::shared_ptr<FileEntry> entry;
    {
        const std::shared_lock lock(registry_mutex_);
        const auto it = files_.find(file_id);
        if (it == files_.end()) {
            return;
        }
        entry = it->second;
    }

    const std::unique_lock file_lock(entry->mutex);

    if (pos >= entry->total_size) {
        return;
    }

    const size_t available_len = std::min(chunk.size(), entry->total_size - pos);
    const size_t start = pos;
    const size_t end = pos + available_len;

    auto it = entry->intervals.upper_bound(start);
    if (it != entry->intervals.begin()) {
        const auto prev = std::prev(it);
        if (prev->first <= start && prev->second >= end) {
            return;
        }
    }

    std::memcpy(entry->data->data() + start, chunk.data(), available_len);

    size_t new_start = start;
    size_t new_end = end;
    size_t overlap_bytes = 0;

    it = entry->intervals.lower_bound(start);
    if (it != entry->intervals.begin()) {
        const auto prev = std::prev(it);
        if (prev->second >= start) {
            it = prev;
        }
    }

    while (it != entry->intervals.end() && it->first <= end) {
        const size_t overlap_start = std::max(start, it->first);
        const size_t overlap_end = std::min(end, it->second);

        if (overlap_end > overlap_start) {
            overlap_bytes += (overlap_end - overlap_start);
        }

        new_start = std::min(new_start, it->first);
        new_end = std::max(new_end, it->second);
        it = entry->intervals.erase(it);
    }

    entry->intervals[new_start] = new_end;
    entry->collected_bytes += ((end - start) - overlap_bytes);
}

std::shared_ptr<const std::vector<uint8_t>> FileCollector::GetFile(uint32_t file_id) const {
    std::shared_ptr<FileEntry> entry;
    {
        const std::shared_lock lock(registry_mutex_);
        const auto it = files_.find(file_id);
        if (it == files_.end()) {
            return nullptr;
        }
        entry = it->second;
    }

    const std::shared_lock file_lock(entry->mutex);
    if (entry->collected_bytes < entry->total_size) {
        return nullptr;
    }

    return entry->data;
}

bool FileCollector::ReadExact(uint32_t file_id, size_t pos, std::span<uint8_t> destination) const {
    if (destination.empty()) {
        return true;
    }

    std::shared_ptr<FileEntry> entry;
    {
        const std::shared_lock lock(registry_mutex_);
        const auto it = files_.find(file_id);
        if (it == files_.end()) {
            return false;
        }
        entry = it->second;
    }

    const std::shared_lock file_lock(entry->mutex);

    if (pos >= entry->total_size || destination.size() > entry->total_size - pos) {
        return false;
    }

    const size_t end = pos + destination.size();
    auto it = entry->intervals.upper_bound(pos);
    if (it == entry->intervals.begin()) {
        return false;
    }

    --it;
    if (it->first <= pos && it->second >= end) {
        std::memcpy(destination.data(), entry->data->data() + pos, destination.size());
        return true;
    }

    return false;
}

bool FileCollector::IsComplete(uint32_t file_id) const {
    std::shared_ptr<FileEntry> entry;
    {
        const std::shared_lock lock(registry_mutex_);
        const auto it = files_.find(file_id);
        if (it == files_.end()) {
            return false;
        }
        entry = it->second;
    }

    const std::shared_lock file_lock(entry->mutex);
    return entry->collected_bytes == entry->total_size;
}

size_t FileCollector::GetFileSize(uint32_t file_id) const {
    std::shared_ptr<FileEntry> entry;
    {
        const std::shared_lock lock(registry_mutex_);
        const auto it = files_.find(file_id);
        if (it == files_.end()) {
            return 0;
        }
        entry = it->second;
    }

    const std::shared_lock file_lock(entry->mutex);
    return entry->total_size;
}

size_t FileCollector::GetReceivedBytes(uint32_t file_id) const {
    std::shared_ptr<FileEntry> entry;
    {
        const std::shared_lock lock(registry_mutex_);
        const auto it = files_.find(file_id);
        if (it == files_.end()) {
            return 0;
        }
        entry = it->second;
    }

    const std::shared_lock file_lock(entry->mutex);
    return entry->collected_bytes;
}
