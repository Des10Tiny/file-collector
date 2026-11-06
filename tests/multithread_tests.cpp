#include <gtest/gtest.h>

#include "file_collector.hpp"

#include <atomic>
#include <random>
#include <thread>
#include <vector>

TEST(FileCollectorMultithread, SingleFileConcurrentWriters) {
    FileCollector collector;
    const uint32_t file_id = 1;
    const size_t file_size = 50000;
    collector.CollectFile(file_id, file_size);

    std::vector<uint8_t> reference(file_size);
    for (size_t i = 0; i < file_size; ++i) {
        reference[i] = static_cast<uint8_t>(i % 256);
    }

    const size_t thread_count = 8;
    const size_t chunk_size = 128;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (size_t t = 0; t < thread_count; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t pos = t * 64; pos < file_size; pos += thread_count * 64) {
                const size_t len = std::min(chunk_size, file_size - pos);
                std::vector<uint8_t> chunk(reference.begin() + pos, reference.begin() + pos + len);
                collector.OnNewChunk(file_id, pos, std::move(chunk));
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_TRUE(collector.IsComplete(file_id));
    const auto assembled = collector.GetFile(file_id);
    ASSERT_NE(assembled, nullptr);
    EXPECT_EQ(*assembled, reference);
}

TEST(FileCollectorMultithread, MultipleFilesConcurrent) {
    FileCollector collector;
    const size_t file_count = 32;
    const size_t file_size = 4096;

    std::vector<std::vector<uint8_t>> references(file_count);
    for (size_t i = 0; i < file_count; ++i) {
        references[i].resize(file_size);
        for (size_t j = 0; j < file_size; ++j) {
            references[i][j] = static_cast<uint8_t>((i * 37 + j) % 256);
        }
        collector.CollectFile(static_cast<uint32_t>(i), file_size);
    }

    const size_t thread_count = 8;
    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (size_t t = 0; t < thread_count; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = t; i < file_count; i += thread_count) {
                const auto fid = static_cast<uint32_t>(i);
                const size_t chunk_size = 256;
                for (size_t pos = 0; pos < file_size; pos += chunk_size) {
                    const size_t len = std::min(chunk_size, file_size - pos);
                    std::vector<uint8_t> chunk(
                        references[i].begin() + pos, references[i].begin() + pos + len
                    );
                    collector.OnNewChunk(fid, pos, std::move(chunk));
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    for (size_t i = 0; i < file_count; ++i) {
        const auto fid = static_cast<uint32_t>(i);
        EXPECT_TRUE(collector.IsComplete(fid));
        const auto result = collector.GetFile(fid);
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(*result, references[i]);
    }
}

TEST(FileCollectorMultithread, ConcurrentWritersAndReaders) {
    FileCollector collector;
    const uint32_t file_id = 99;
    const size_t file_size = 30000;
    collector.CollectFile(file_id, file_size);

    std::vector<uint8_t> reference(file_size);
    for (size_t i = 0; i < file_size; ++i) {
        reference[i] = static_cast<uint8_t>(i % 251);
    }

    std::atomic<bool> writers_done{false};
    std::vector<std::thread> threads;
    threads.reserve(8);

    for (size_t t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            const size_t step = 4;
            const size_t chunk_len = 100;
            for (size_t i = t * 50; i < file_size; i += step * 50) {
                const size_t len = std::min(chunk_len, file_size - i);
                std::vector<uint8_t> chunk(reference.begin() + i, reference.begin() + i + len);
                collector.OnNewChunk(file_id, i, std::move(chunk));
            }
        });
    }

    std::atomic<size_t> successful_partial_reads{0};
    for (size_t r = 0; r < 4; ++r) {
        threads.emplace_back([&]() {
            std::vector<uint8_t> read_buffer(64);
            while (!writers_done.load(std::memory_order_relaxed)) {
                if (collector.ReadExact(file_id, 0, read_buffer)) {
                    successful_partial_reads.fetch_add(1, std::memory_order_relaxed);
                }
                static_cast<void>(collector.IsComplete(file_id));
                static_cast<void>(collector.GetFile(file_id));
            }
        });
    }

    for (size_t i = 0; i < 4; ++i) {
        threads[i].join();
    }

    writers_done.store(true, std::memory_order_relaxed);

    for (size_t i = 4; i < 8; ++i) {
        threads[i].join();
    }

    EXPECT_TRUE(collector.IsComplete(file_id));
    const auto final_file = collector.GetFile(file_id);
    ASSERT_NE(final_file, nullptr);
    EXPECT_EQ(*final_file, reference);
}
