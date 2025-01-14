/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "minikin/SparseBitSet.h"

#include <random>

#include <gtest/gtest.h>

#include "BufferUtils.h"

namespace minikin {

TEST(SparseBitSetTest, randomTest) {
    const uint32_t kTestRangeNum = 4096;

    std::mt19937 mt;  // Fix seeds to be able to reproduce the result.
    std::uniform_int_distribution<uint16_t> distribution(1, 512);

    std::vector<uint32_t> range{distribution(mt)};
    for (size_t i = 1; i < kTestRangeNum * 2; ++i) {
        range.push_back((range.back() - 1) + distribution(mt));
    }

    SparseBitSet bitset(range.data(), range.size() / 2);

    uint32_t ch = 0;
    for (size_t i = 0; i < range.size() / 2; ++i) {
        uint32_t start = range[i * 2];
        uint32_t end = range[i * 2 + 1];

        for (; ch < start; ch++) {
            ASSERT_FALSE(bitset.get(ch)) << std::hex << ch;
        }
        for (; ch < end; ch++) {
            ASSERT_TRUE(bitset.get(ch)) << std::hex << ch;
        }
    }
    for (; ch < 0x1FFFFFF; ++ch) {
        ASSERT_FALSE(bitset.get(ch)) << std::hex << ch;
    }
}

TEST(SparseBitSetTest, bufferTest) {
    std::vector<uint32_t> range({10, 20});
    SparseBitSet originalBitset(range.data(), range.size() / 2);
    std::vector<uint8_t> buffer = writeToBuffer(originalBitset);
    BufferReader reader(buffer.data());
    SparseBitSet bitset(&reader);

    for (size_t i = 0; i < 10; ++i) ASSERT_FALSE(bitset.get(i)) << i;
    for (size_t i = 10; i < 20; ++i) ASSERT_TRUE(bitset.get(i)) << i;
    for (size_t i = 20; i < 30; ++i) ASSERT_FALSE(bitset.get(i)) << i;
    std::vector<uint8_t> newBuffer = writeToBuffer(bitset);
    ASSERT_EQ(buffer, newBuffer);
}

TEST(SparseBitSetTest, emptyBitSetBufferTest) {
    SparseBitSet empty;
    std::vector<uint8_t> buffer = writeToBuffer(empty);
    BufferReader reader(buffer.data());
    SparseBitSet bitset(&reader);

    ASSERT_FALSE(bitset.get(0));
    std::vector<uint8_t> newBuffer = writeToBuffer(bitset);
    ASSERT_EQ(buffer, newBuffer);
}

TEST(SparseBitSetTest, sizeTest) {
    ASSERT_EQ(sizeof(void*), sizeof(SparseBitSet));
}

}  // namespace minikin
