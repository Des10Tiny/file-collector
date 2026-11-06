#include <gtest/gtest.h>

#include "file_collector.hpp"

#include <numeric>
#include <vector>

TEST(FileCollectorBasic, PdfExample) {
    FileCollector collector;
    const uint32_t file_id = 1;
    collector.CollectFile(file_id, 10);

    EXPECT_FALSE(collector.IsComplete(file_id));
    EXPECT_EQ(collector.GetFile(file_id), nullptr);

    collector.OnNewChunk(file_id, 0, std::vector<uint8_t>{'A', 'B', 'C', 'D'});
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 4);
    EXPECT_FALSE(collector.IsComplete(file_id));

    collector.OnNewChunk(file_id, 2, std::vector<uint8_t>{'C', 'D', 'E', 'F'});
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 6);
    EXPECT_FALSE(collector.IsComplete(file_id));

    collector.OnNewChunk(file_id, 6, std::vector<uint8_t>{'G', 'H', 'I', 'J'});
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 10);
    EXPECT_TRUE(collector.IsComplete(file_id));

    const auto result = collector.GetFile(file_id);
    ASSERT_NE(result, nullptr);
    const std::vector<uint8_t> expected = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'};
    EXPECT_EQ(*result, expected);
}

TEST(FileCollectorBasic, OutOfOrderWithGaps) {
    FileCollector collector;
    const uint32_t file_id = 42;
    const size_t file_size = 8;
    collector.CollectFile(file_id, file_size);

    collector.OnNewChunk(file_id, 6, std::vector<uint8_t>{6, 7});
    EXPECT_FALSE(collector.IsComplete(file_id));
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 2);

    collector.OnNewChunk(file_id, 2, std::vector<uint8_t>{2, 3});
    EXPECT_FALSE(collector.IsComplete(file_id));
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 4);

    collector.OnNewChunk(file_id, 0, std::vector<uint8_t>{0, 1});
    EXPECT_FALSE(collector.IsComplete(file_id));
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 6);

    collector.OnNewChunk(file_id, 4, std::vector<uint8_t>{4, 5});
    EXPECT_TRUE(collector.IsComplete(file_id));
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 8);

    const auto result = collector.GetFile(file_id);
    ASSERT_NE(result, nullptr);
    const std::vector<uint8_t> expected = {0, 1, 2, 3, 4, 5, 6, 7};
    EXPECT_EQ(*result, expected);
}

TEST(FileCollectorBasic, DuplicateAndEnclosingChunks) {
    FileCollector collector;
    const uint32_t file_id = 10;
    collector.CollectFile(file_id, 6);

    collector.OnNewChunk(file_id, 2, std::vector<uint8_t>{2, 3});
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 2);

    collector.OnNewChunk(file_id, 2, std::vector<uint8_t>{2, 3});
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 2);

    collector.OnNewChunk(file_id, 0, std::vector<uint8_t>{0, 1, 2, 3, 4, 5});
    EXPECT_TRUE(collector.IsComplete(file_id));
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 6);

    const auto result = collector.GetFile(file_id);
    ASSERT_NE(result, nullptr);
    const std::vector<uint8_t> expected = {0, 1, 2, 3, 4, 5};
    EXPECT_EQ(*result, expected);
}

TEST(FileCollectorBasic, BoundaryClampingAndOutOfBounds) {
    FileCollector collector;
    const uint32_t file_id = 7;
    collector.CollectFile(file_id, 5);

    collector.OnNewChunk(file_id, 10, std::vector<uint8_t>{1, 2, 3});
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 0);

    collector.OnNewChunk(file_id, 3, std::vector<uint8_t>{3, 4, 99, 100});
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 2);

    collector.OnNewChunk(file_id, 0, std::vector<uint8_t>{0, 1, 2});
    EXPECT_TRUE(collector.IsComplete(file_id));
    EXPECT_EQ(collector.GetReceivedBytes(file_id), 5);

    const auto result = collector.GetFile(file_id);
    ASSERT_NE(result, nullptr);
    const std::vector<uint8_t> expected = {0, 1, 2, 3, 4};
    EXPECT_EQ(*result, expected);
}

TEST(FileCollectorBasic, EmptyChunkAndEmptyFile) {
    FileCollector collector;
    const uint32_t empty_file_id = 100;
    collector.CollectFile(empty_file_id, 0);

    EXPECT_TRUE(collector.IsComplete(empty_file_id));
    const auto empty_result = collector.GetFile(empty_file_id);
    ASSERT_NE(empty_result, nullptr);
    EXPECT_TRUE(empty_result->empty());

    const uint32_t normal_file_id = 101;
    collector.CollectFile(normal_file_id, 4);
    collector.OnNewChunk(normal_file_id, 0, std::vector<uint8_t>{});
    EXPECT_EQ(collector.GetReceivedBytes(normal_file_id), 0);
    EXPECT_FALSE(collector.IsComplete(normal_file_id));
}

TEST(FileCollectorBasic, PartialReading) {
    FileCollector collector;
    const uint32_t file_id = 55;
    collector.CollectFile(file_id, 10);

    collector.OnNewChunk(file_id, 2, std::vector<uint8_t>{20, 30, 40, 50});

    std::vector<uint8_t> buffer(2);
    EXPECT_TRUE(collector.ReadExact(file_id, 2, buffer));
    EXPECT_EQ(buffer, (std::vector<uint8_t>{20, 30}));

    EXPECT_TRUE(collector.ReadExact(file_id, 4, buffer));
    EXPECT_EQ(buffer, (std::vector<uint8_t>{40, 50}));

    EXPECT_FALSE(collector.ReadExact(file_id, 0, buffer));
    EXPECT_FALSE(collector.ReadExact(file_id, 5, buffer));
    EXPECT_FALSE(collector.ReadExact(file_id, 9, buffer));

    std::vector<uint8_t> zero_buf{};
    EXPECT_TRUE(collector.ReadExact(file_id, 2, zero_buf));
}

TEST(FileCollectorBasic, NonExistentFile) {
    FileCollector collector;
    EXPECT_FALSE(collector.IsComplete(999));
    EXPECT_EQ(collector.GetFile(999), nullptr);
    EXPECT_EQ(collector.GetFileSize(999), 0);
    EXPECT_EQ(collector.GetReceivedBytes(999), 0);

    std::vector<uint8_t> buf(4);
    EXPECT_FALSE(collector.ReadExact(999, 0, buf));

    collector.OnNewChunk(999, 0, std::vector<uint8_t>{1, 2, 3});
    EXPECT_EQ(collector.GetFileSize(999), 0);
}
