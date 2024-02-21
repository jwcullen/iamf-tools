/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/read_bit_buffer.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
using absl::StatusCode::kResourceExhausted;
using testing::UnorderedElementsAreArray;
namespace {

class ReadBitBufferTest : public ::testing::Test {
 public:
  std::vector<uint8_t> source_data_;
  int64_t rb_capacity_;
  std::unique_ptr<ReadBitBuffer> CreateReadBitBuffer() {
    return std::make_unique<ReadBitBuffer>(rb_capacity_, &source_data_);
  }
};

TEST_F(ReadBitBufferTest, ReadBitBufferConstructor) {
  source_data_ = {};
  rb_capacity_ = 0;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_NE(rb_, nullptr);
}

TEST_F(ReadBitBufferTest, LoadBitsByteAligned) {
  source_data_ = {0x09, 0x02, 0xab};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(24).ok());
  EXPECT_THAT(rb_->bit_buffer(), UnorderedElementsAreArray(source_data_));
}

TEST_F(ReadBitBufferTest, LoadBitsNotByteAligned) {
  source_data_ = {0b10100001};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(3).ok());
  // Only read the first 3 bits (101) - the rest of the bits in the byte are
  // zeroed out.
  std::vector<uint8_t> expected = {0b10100000};
  EXPECT_THAT(rb_->bit_buffer(), UnorderedElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 3);
  // Load bits again. This will clear the buffer while still reading from the
  // updated source offset.
  EXPECT_TRUE(rb_->LoadBits(5).ok());
  expected = {
      0b00001000};  // {00001} these bits are loaded from the 5 remaining bits
                    // in the buffer - the rest of the bits are zeroed out.
  EXPECT_THAT(rb_->bit_buffer(), UnorderedElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 8);
}

TEST_F(ReadBitBufferTest, LoadBitsNotEnoughSourceBits) {
  source_data_ = {0x09, 0x02, 0xab};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_EQ(rb_->LoadBits(32).code(), kResourceExhausted);
  EXPECT_EQ(rb_->bit_buffer().size(), 0);
}

}  // namespace
}  // namespace iamf_tools
