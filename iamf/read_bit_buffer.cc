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
#include <vector>

#include "absl/status/status.h"
#include "iamf/bit_buffer_util.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/ia.h"

namespace iamf_tools {

namespace {

bool ShouldRead(const int64_t& source_offset,
                const std::vector<uint8_t>& source,
                const int32_t& remaining_bits_to_read) {
  const bool valid_bit_offset = (source_offset / 8) < source.size();
  const bool bits_to_read = remaining_bits_to_read > 0;
  return valid_bit_offset && bits_to_read;
}

// Reads one bit from data at position `source_offset`. Reads in order of most
// significant to least significant - that is, offset = 0 refers to the bit in
// position 2^7, offset = 1 refers to the bit in position 2^6, etc. Caller
// should ensure that source_offset/8 is < source.size().
uint8_t GetUpperBit(int64_t source_offset, std::vector<uint8_t>* source) {
  int64_t byteIndex = source_offset / 8;
  uint8_t bitIndex = 7 - (source_offset % 8);
  return (source->at(byteIndex) >> bitIndex) & 0x01;
}
}  // namespace

ReadBitBuffer::ReadBitBuffer(int64_t capacity, std::vector<uint8_t>* source,
                             const LebGenerator& leb_generator)
    : leb_generator_(leb_generator), source_(source) {
  bit_buffer_.reserve(capacity);
}

// Reads n = `num_bits` bits from the buffer. These are the lower n bits of
// `bit_buffer_`. n must be <= 64. The read data is consumed, meaning
// `bit_buffer_offset_` is incremented by n as a side effect of this fxn.
absl::Status ReadBitBuffer::ReadUnsignedLiteral(uint64_t* data, int num_bits) {
  return absl::UnimplementedError("Not yet implemented.");
}

// Loads enough bits from source such that there are at least n =
// `required_num_bits` in `bit_buffer_` after completion. Returns an error if
// there are not enough bits in `source_` to fulfill this request. If `source_`
// contains enough data, this function will fill the read buffer completely.
absl::Status ReadBitBuffer::LoadBits(const int32_t required_num_bits) {
  DiscardAllBits();
  int original_source_offset = source_bit_offset_;
  int remaining_bits_to_load = required_num_bits;
  int64_t bit_buffer_write_offset = 0;
  while (ShouldRead(source_bit_offset_, *source_, remaining_bits_to_load) &&
         (bit_buffer_.size() != bit_buffer_.capacity())) {
    if (remaining_bits_to_load < 8 || source_bit_offset_ % 8 != 0) {
      // Load bit by bit
      uint8_t loaded_bit = GetUpperBit(source_bit_offset_, source_);
      RETURN_IF_NOT_OK(
          CanWriteBits(true, 1, bit_buffer_write_offset, bit_buffer_));
      RETURN_IF_NOT_OK(
          WriteBit(loaded_bit, bit_buffer_write_offset, bit_buffer_));
      bit_buffer_write_offset = bit_buffer_write_offset % 8;
      source_bit_offset_++;
      remaining_bits_to_load--;
    } else {
      // Load byte by byte
      bit_buffer_.push_back(source_->at(source_bit_offset_ / 8));
      source_bit_offset_ += 8;
      remaining_bits_to_load -= 8;
    }
  }
  if (remaining_bits_to_load != 0) {
    source_bit_offset_ = original_source_offset;
    DiscardAllBits();
    return absl::ResourceExhaustedError("Not enough bits in source.");
  }
  return absl::OkStatus();
}

void ReadBitBuffer::DiscardAllBits() {
  buffer_bit_offset_ = 0;
  bit_buffer_.clear();
}

}  // namespace iamf_tools
