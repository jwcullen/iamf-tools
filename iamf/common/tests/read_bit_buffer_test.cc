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
#include "iamf/common/read_bit_buffer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/bit_buffer_util.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {
using absl::StatusCode::kInvalidArgument;
using absl::StatusCode::kResourceExhausted;
using testing::ElementsAreArray;

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;

constexpr int kBitsPerByte = 8;
constexpr int kMaxUint32 = std::numeric_limits<uint32_t>::max();

class ReadBitBufferTest : public ::testing::Test {
 protected:
  void CreateReadBitBuffer() {
    rb_ = std::make_unique<ReadBitBuffer>(rb_capacity_, &source_data_);
    EXPECT_EQ(rb_->Tell(), 0);
  }

  std::vector<uint8_t> source_data_;
  int64_t rb_capacity_;
  std::unique_ptr<ReadBitBuffer> rb_;
};

TEST_F(ReadBitBufferTest, ReadBitBufferConstructor) {
  source_data_ = {};
  rb_capacity_ = 0;
  CreateReadBitBuffer();
  EXPECT_NE(rb_, nullptr);
}

// ---- Seek and Tell Tests -----
TEST_F(ReadBitBufferTest, SeekAndTellMatch) {
  source_data_ = {0xab, 0xcd, 0xef};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  // Start at position 0.
  EXPECT_EQ(rb_->Tell(), 0);

  // Move to various positions and expect that the positions are updated.
  EXPECT_THAT(rb_->Seek(3), IsOk());
  EXPECT_EQ(rb_->Tell(), 3);

  EXPECT_THAT(rb_->Seek(17), IsOk());
  EXPECT_EQ(rb_->Tell(), 17);

  EXPECT_THAT(rb_->Seek(23), IsOk());
  EXPECT_EQ(rb_->Tell(), 23);

  EXPECT_THAT(rb_->Seek(10), IsOk());
  EXPECT_EQ(rb_->Tell(), 10);
}

TEST_F(ReadBitBufferTest, SeekFailsWithNegativePosition) {
  source_data_ = {0xab, 0xcd, 0xef};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  EXPECT_THAT(rb_->Seek(-1), StatusIs(kInvalidArgument));
}

TEST_F(ReadBitBufferTest, SeekFailsWithPositionTooLarge) {
  source_data_ = {0xab, 0xcd, 0xef};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  EXPECT_THAT(rb_->Seek(24), StatusIs(kResourceExhausted));
}

// ---- ReadUnsignedLiteral Tests -----
TEST_F(ReadBitBufferTest, ReadZeroBitsFromEmptySourceSucceeds) {
  source_data_ = {};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  uint64_t output_literal = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(0, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0);
  EXPECT_EQ(rb_->Tell(), 0);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralByteAlignedAllBits) {
  source_data_ = {0xab, 0xcd, 0xef};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  uint64_t output_literal = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(24, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0xabcdef);
  EXPECT_EQ(rb_->Tell(), 24);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralByteAlignedMultipleReads) {
  source_data_ = {0xab, 0xcd, 0xef, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  uint64_t output_literal = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(24, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0xabcdef);
  EXPECT_EQ(rb_->Tell(), 24);

  // Second read to same output integer - will be overwritten.
  EXPECT_THAT(rb_->ReadUnsignedLiteral(8, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0xff);
  EXPECT_EQ(rb_->Tell(), 32);
}

TEST_F(ReadBitBufferTest,
       ReadUnsignedLiteralByteAlignedNotEnoughBitsInBufferOrSource) {
  source_data_ = {0xab, 0xcd, 0xef};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  uint64_t output_literal = 0;
  // We request more bits than there are in the buffer. ReadUnsignedLiteral will
  // attempt to load more bits from source into the buffer, but that will fail,
  // since there aren't enough bits in the source either.
  EXPECT_THAT(rb_->ReadUnsignedLiteral(32, output_literal),
              StatusIs(kResourceExhausted));
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralNotByteAlignedOneByte) {
  source_data_ = {0b10100001};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  uint8_t output_literal = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(3, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b101);
  EXPECT_EQ(rb_->Tell(), 3);

  // Read 5 bits more and expect the position is at 8 bits.
  EXPECT_THAT(rb_->ReadUnsignedLiteral(5, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b00001);
  EXPECT_EQ(rb_->Tell(), 8);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralNotByteAlignedMultipleReads) {
  source_data_ = {0b11000101, 0b10000010, 0b00000110};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  uint64_t output_literal = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(6, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b110001);
  EXPECT_EQ(rb_->Tell(), 6);

  EXPECT_THAT(rb_->ReadUnsignedLiteral(10, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b0110000010);
  EXPECT_EQ(rb_->Tell(), 16);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralRequestTooLarge) {
  source_data_ = {0b00000101, 0b00000010, 0b00000110};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  uint64_t output_literal = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(65, output_literal),
              StatusIs(kInvalidArgument));
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralAfterSeek) {
  source_data_ = {0b00000111, 0b10000000};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  // Move the position to the 6-th bit, which points to the first "1".
  EXPECT_THAT(rb_->Seek(5), IsOk());
  EXPECT_EQ(rb_->Tell(), 5);

  // Read in 4 bits, which are all "1"s.
  uint64_t output_literal = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(4, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b1111);

  // Read in another 7 bits, which are all "0"s.
  EXPECT_THAT(rb_->ReadUnsignedLiteral(7, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b0000000);
}

// ---- ReadULeb128 Tests -----

// Successful Uleb128 reads.
TEST_F(ReadBitBufferTest, ReadUleb128Read5Bytes) {
  source_data_ = {0x81, 0x83, 0x81, 0x83, 0x0f};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(rb_->ReadULeb128(output_leb), IsOk());
  EXPECT_EQ(output_leb, 0b11110000011000000100000110000001);
  EXPECT_EQ(rb_->Tell(), 40);
}

TEST_F(ReadBitBufferTest, ReadUleb128Read5BytesAndStoreSize) {
  source_data_ = {0x81, 0x83, 0x81, 0x83, 0x0f};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  int8_t encoded_leb_size = 0;
  EXPECT_THAT(rb_->ReadULeb128(output_leb, encoded_leb_size), IsOk());
  EXPECT_EQ(output_leb, 0b11110000011000000100000110000001);
  EXPECT_EQ(encoded_leb_size, 5);
  EXPECT_EQ(rb_->Tell(), 40);
}

TEST_F(ReadBitBufferTest, ReadUleb128TwoBytes) {
  source_data_ = {0x81, 0x03, 0x81, 0x83, 0x0f};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(rb_->ReadULeb128(output_leb), IsOk());

  // Expect the buffer to read only the first two bytes, since 0x03 does not
  // have a one in the most significant spot of the byte.
  EXPECT_EQ(output_leb, 0b00000110000001);
  EXPECT_EQ(rb_->Tell(), 16);
}

TEST_F(ReadBitBufferTest, ReadUleb128ExtraZeroes) {
  source_data_ = {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(rb_->ReadULeb128(output_leb), IsOk());
  EXPECT_EQ(output_leb, 0b1);
  EXPECT_EQ(rb_->Tell(), 64);
}

TEST_F(ReadBitBufferTest, ReadUleb128ExtraZeroesAndStoreSize) {
  source_data_ = {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  int8_t encoded_leb_size = 0;
  EXPECT_THAT(rb_->ReadULeb128(output_leb, encoded_leb_size), IsOk());
  EXPECT_EQ(output_leb, 0b1);
  EXPECT_EQ(encoded_leb_size, 8);
  EXPECT_EQ(rb_->Tell(), 64);
}

// Uleb128 read errors.
TEST_F(ReadBitBufferTest, ReadUleb128Overflow) {
  source_data_ = {0x80, 0x80, 0x80, 0x80, 0x10};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(rb_->ReadULeb128(output_leb), StatusIs(kInvalidArgument));
}

TEST_F(ReadBitBufferTest, ReadUleb128TooManyBytes) {
  source_data_ = {0x80, 0x83, 0x81, 0x83, 0x80, 0x80, 0x80, 0x80};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(rb_->ReadULeb128(output_leb), StatusIs(kInvalidArgument));
}

TEST_F(ReadBitBufferTest, ReadUleb128NotEnoughDataInBufferOrSource) {
  source_data_ = {0x80, 0x80, 0x80, 0x80};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;

  // Buffer has a one in the most significant position of each byte, which tells
  // us to continue reading to the next byte. The 4th byte tells us to read the
  // next byte, but there is no 5th byte in neither the buffer nor the source.
  EXPECT_THAT(rb_->ReadULeb128(output_leb), StatusIs(kResourceExhausted));
}

// ---- ReadIso14496_1Expanded Tests -----

struct ReadIso14496_1ExpandedTestCase {
  std::vector<uint8_t> source_data;
  uint32_t expected_size_of_instance;
};

using ReadIso14496_1Expanded =
    ::testing::TestWithParam<ReadIso14496_1ExpandedTestCase>;

TEST_P(ReadIso14496_1Expanded, ReadIso14496_1Expanded) {
  // Grab a copy of the data because the ReadBitBuffer will consume it.
  std::vector<uint8_t> source_data = GetParam().source_data;
  ReadBitBuffer rb(source_data.size() * kBitsPerByte, &source_data);

  uint32_t output_size_of_instance = 0;
  EXPECT_THAT(rb.ReadIso14496_1Expanded(kMaxUint32, output_size_of_instance),
              IsOk());

  EXPECT_EQ(output_size_of_instance, GetParam().expected_size_of_instance);
}

INSTANTIATE_TEST_SUITE_P(OneByteInput, ReadIso14496_1Expanded,
                         testing::ValuesIn<ReadIso14496_1ExpandedTestCase>({
                             {{0x00}, 0},
                             {{0x40}, 64},
                             {{0x7f}, 127},
                         }));

INSTANTIATE_TEST_SUITE_P(TwoByteInput, ReadIso14496_1Expanded,
                         testing::ValuesIn<ReadIso14496_1ExpandedTestCase>({
                             {{0x81, 0x00}, 128},
                             {{0x81, 0x01}, 129},
                             {{0xff, 0x7e}, 0x3ffe},
                             {{0xff, 0x7f}, 0x3fff},
                         }));

INSTANTIATE_TEST_SUITE_P(FourByteInput, ReadIso14496_1Expanded,
                         testing::ValuesIn<ReadIso14496_1ExpandedTestCase>({
                             {{0x81, 0x80, 0x80, 0x00}, 0x0200000},
                             {{0x81, 0x80, 0x80, 0x01}, 0x0200001},
                             {{0xff, 0xff, 0xff, 0x7e}, 0x0ffffffe},
                             {{0xff, 0xff, 0xff, 0x7f}, 0x0fffffff},
                         }));

INSTANTIATE_TEST_SUITE_P(FiveByteInput, ReadIso14496_1Expanded,
                         testing::ValuesIn<ReadIso14496_1ExpandedTestCase>({
                             {{0x81, 0x80, 0x80, 0x80, 0x00}, 0x10000000},
                             {{0x8f, 0x80, 0x80, 0x80, 0x00}, 0xf0000000},
                             {{0x8f, 0xff, 0xff, 0xff, 0x7f}, 0xffffffff},
                         }));

INSTANTIATE_TEST_SUITE_P(HandlesLeadingZeroes, ReadIso14496_1Expanded,
                         testing::ValuesIn<ReadIso14496_1ExpandedTestCase>({
                             {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01},
                              1},
                         }));

TEST(ReadIso14496_1Expanded, ValidWhenDecodedValueEqualToMaxClassSize) {
  constexpr uint32_t kMaxClassSizeExact = 127;
  std::vector<uint8_t> source_data = {0x7f};
  ReadBitBuffer rb(source_data.size() * kBitsPerByte, &source_data);

  uint32_t unused_output = 0;
  EXPECT_THAT(rb.ReadIso14496_1Expanded(kMaxClassSizeExact, unused_output),
              IsOk());
}

TEST(ReadIso14496_1Expanded, InvalidWhenDecodedValueIsGreaterThanMaxClassSize) {
  constexpr uint32_t kMaxClassSizeTooLow = 126;
  std::vector<uint8_t> source_data = {0x7f};
  ReadBitBuffer rb(source_data.size() * kBitsPerByte, &source_data);

  uint32_t unused_output = 0;
  EXPECT_FALSE(
      rb.ReadIso14496_1Expanded(kMaxClassSizeTooLow, unused_output).ok());
}

TEST(ReadIso14496_1Expanded, InvalidWhenDecodedValueDoesNotFitIntoUint32) {
  std::vector<uint8_t> source_data = {0x90, 0x80, 0x80, 0x80, 0x00};
  ReadBitBuffer rb(source_data.size() * kBitsPerByte, &source_data);

  uint32_t unused_output = 0;
  EXPECT_FALSE(rb.ReadIso14496_1Expanded(kMaxUint32, unused_output).ok());
}

TEST(ReadIso14496_1Expanded, InvalidWhenInputDataSignalsMoreThan8Bytes) {
  std::vector<uint8_t> source_data = {0x80, 0x80, 0x80, 0x80, 0x80,
                                      0x80, 0x80, 0x80, 0x01};
  ReadBitBuffer rb(source_data.size() * kBitsPerByte, &source_data);

  uint32_t unused_output = 0;
  EXPECT_FALSE(rb.ReadIso14496_1Expanded(kMaxUint32, unused_output).ok());
}

// --- `ReadUint8Span` tests ---

// Successful usage of `ReadUint8Span`.
TEST(ReadUint8Span, SucceedsWithAlignedBuffer) {
  constexpr size_t kOutputSize = 5;
  std::vector<uint8_t> source_data = {0x01, 0x23, 0x45, 0x68, 0x89};
  const auto source_data_copy = source_data;
  ReadBitBuffer rb(source_data.size() * kBitsPerByte, &source_data);

  std::vector<uint8_t> output(kOutputSize);
  EXPECT_THAT(rb.ReadUint8Span(absl::MakeSpan(output)), IsOk());
  EXPECT_EQ(output, source_data_copy);
  EXPECT_EQ(rb.Tell(), 40);
}

TEST(ReadUint8Span, SucceedsWithMisalignedBuffer) {
  // Prepare the buffer with source data, but where partial bytes have been
  // read, so later reads are not on byte boundaries.
  std::vector<uint8_t> source_data = {0xab, 0xcd, 0xef, 0x01, 0x23};
  constexpr size_t kOffsetBits = 4;
  constexpr std::array<uint8_t, 4> kExpectedOutput{0xbc, 0xde, 0xf0, 0x12};
  ReadBitBuffer rb(source_data.size() * kBitsPerByte, &source_data);

  // Read a 4-bit literal to misalign the buffer.
  uint8_t literal = 0;
  EXPECT_THAT(rb.ReadUnsignedLiteral(kOffsetBits, literal), IsOk());
  EXPECT_EQ(rb.Tell(), kOffsetBits);

  std::vector<uint8_t> output(4);
  EXPECT_THAT(rb.ReadUint8Span(absl::MakeSpan(output)), IsOk());
  EXPECT_THAT(output, testing::ElementsAreArray(kExpectedOutput));
  EXPECT_EQ(rb.Tell(), 36);
}

// `ReadUint8Span` errors.
TEST(ReadUint8Span, InvalidWhenNotEnoughDataInBufferToFillSpan) {
  constexpr size_t kSourceSize = 4;
  constexpr size_t kOutputSizeTooLarge = 5;
  std::vector<uint8_t> source_data(kSourceSize);
  ReadBitBuffer rb(source_data.size() * kBitsPerByte, &source_data);

  std::vector<uint8_t> output(kOutputSizeTooLarge);
  EXPECT_THAT(rb.ReadUint8Span(absl::MakeSpan(output)),
              StatusIs(kResourceExhausted));
}

// --- ReadBoolean tests ---

// Successful ReadBoolean reads
TEST_F(ReadBitBufferTest, ReadBoolean8Bits) {
  source_data_ = {0b10011001};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  bool output = false;
  std::vector<bool> expected_output = {true, false, false, true,
                                       true, false, false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_THAT(rb_->ReadBoolean(output), IsOk());
    EXPECT_EQ(output, expected_output[i]);
  }
  EXPECT_EQ(rb_->Tell(), 8);
}

TEST_F(ReadBitBufferTest, ReadBooleanMisalignedBuffer) {
  source_data_ = {0b10000001, 0b01000000};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  uint64_t literal = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(2, literal), IsOk());
  EXPECT_EQ(rb_->Tell(), 2);

  bool output = false;
  // Expected output starts reading at bit 2 instead of at 0.
  std::vector<bool> expected_output = {false, false, false, false,
                                       false, true,  false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_THAT(rb_->ReadBoolean(output), IsOk());
    EXPECT_EQ(output, expected_output[i]);
  }
  EXPECT_EQ(rb_->Tell(), 10);
}

// ReadBoolean Error
TEST_F(ReadBitBufferTest, ReadBooleanNotEnoughDataInBufferOrSource) {
  source_data_ = {0b10011001};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  bool output = false;
  std::vector<bool> expected_output = {true, false, false, true,
                                       true, false, false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_THAT(rb_->ReadBoolean(output), IsOk());
    EXPECT_EQ(output, expected_output[i]);
  }
  EXPECT_THAT(rb_->ReadBoolean(output), StatusIs(kResourceExhausted));
}

// --- ReadSigned16 tests ---

TEST_F(ReadBitBufferTest, Signed16Zero) {
  source_data_ = {0x00, 0x00};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, 0);
  EXPECT_EQ(rb_->Tell(), 16);
}

TEST_F(ReadBitBufferTest, Signed16MaxPositive) {
  source_data_ = {0x7f, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, 32767);
  EXPECT_EQ(rb_->Tell(), 16);
}

TEST_F(ReadBitBufferTest, Signed16MinPositive) {
  source_data_ = {0x00, 0x01};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, 1);
  EXPECT_EQ(rb_->Tell(), 16);
}

TEST_F(ReadBitBufferTest, Signed16MinNegative) {
  source_data_ = {0x80, 0x00};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, -32768);
  EXPECT_EQ(rb_->Tell(), 16);
}

TEST_F(ReadBitBufferTest, Signed16MaxNegative) {
  source_data_ = {0xff, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, -1);
  EXPECT_EQ(rb_->Tell(), 16);
}

TEST_F(ReadBitBufferTest, IsDataAvailable) {
  source_data_ = {0xff, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  EXPECT_TRUE(rb_->IsDataAvailable());
  uint64_t output = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(16, output), IsOk());
  EXPECT_FALSE(rb_->IsDataAvailable());
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralMax32) {
  source_data_ = {0xff, 0xff, 0xff, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  uint32_t output = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(32, output), IsOk());
  EXPECT_EQ(output, 4294967295);
  EXPECT_EQ(rb_->Tell(), 32);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteral32Overflow) {
  source_data_ = {0xff, 0xff, 0xff, 0xff, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  uint32_t output = 0;
  EXPECT_FALSE(rb_->ReadUnsignedLiteral(40, output).ok());
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralMax16) {
  source_data_ = {0xff, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  uint16_t output = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(16, output), IsOk());
  EXPECT_EQ(output, 65535);
  EXPECT_EQ(rb_->Tell(), 16);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteral16Overflow) {
  source_data_ = {0xff, 0xff, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  uint16_t output = 0;
  EXPECT_FALSE(rb_->ReadUnsignedLiteral(24, output).ok());
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralMax8) {
  source_data_ = {0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  uint8_t output = 0;
  EXPECT_THAT(rb_->ReadUnsignedLiteral(8, output), IsOk());
  EXPECT_EQ(output, 255);
  EXPECT_EQ(rb_->Tell(), 8);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteral8Overflow) {
  source_data_ = {0xff, 0xff};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  uint8_t output = 0;
  EXPECT_FALSE(rb_->ReadUnsignedLiteral(9, output).ok());
}

TEST_F(ReadBitBufferTest, StringOnlyNullCharacter) {
  source_data_ = {'\0'};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  std::string output;
  EXPECT_THAT(rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, "");
}

TEST_F(ReadBitBufferTest, StringAscii) {
  source_data_ = {'A', 'B', 'C', '\0'};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  std::string output;
  EXPECT_THAT(rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, "ABC");
}

TEST_F(ReadBitBufferTest, StringOverrideOutputParam) {
  source_data_ = {'A', 'B', 'C', '\0'};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  std::string output = "xyz";
  EXPECT_THAT(rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, "ABC");
}

TEST_F(ReadBitBufferTest, StringUtf8) {
  source_data_ = {0xc3, 0xb3,              // A 1-byte UTF-8 character.
                  0xf0, 0x9d, 0x85, 0x9f,  // A 4-byte UTF-8 character.
                  '\0'};
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  std::string output;
  EXPECT_THAT(rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, "\303\263\360\235\205\237");
}

TEST_F(ReadBitBufferTest, StringMaxLength) {
  source_data_ = std::vector<uint8_t>(kIamfMaxStringSize - 1, 'a');
  source_data_.push_back('\0');
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  std::string output;
  EXPECT_THAT(rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, std::string(kIamfMaxStringSize - 1, 'a'));
}

TEST_F(ReadBitBufferTest, InvalidStringMissingNullTerminator) {
  source_data_ = std::vector<uint8_t>({'a', 'b', 'c'});
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  std::string output;
  EXPECT_FALSE(rb_->ReadString(output).ok());
}

TEST_F(ReadBitBufferTest, InvalidStringMissingNullTerminatorMaxLength) {
  source_data_ = std::vector<uint8_t>(kIamfMaxStringSize, 'a');
  rb_capacity_ = 1024;
  CreateReadBitBuffer();
  std::string output;
  EXPECT_FALSE(rb_->ReadString(output).ok());
}

}  // namespace
}  // namespace iamf_tools
