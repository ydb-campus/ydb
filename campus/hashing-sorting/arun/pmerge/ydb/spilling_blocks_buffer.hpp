//
// Created by nfrmtk on 6/14/25.
//

#ifndef SPILLING_BLOCKS_BUFFER_HPP
#define SPILLING_BLOCKS_BUFFER_HPP
#include <campus/hashing-sorting/arun/spilling_mem.h>

#include <campus/hashing-sorting/arun/pmerge/common/assert.hpp>
#include <campus/hashing-sorting/arun/pmerge/ydb/types.hpp>
#include <span>
namespace pmerge::ydb {

    class SpillingBlocksBuffer {
    public:
        SpillingBlocksBuffer(TSpilling& spilling, TSpillingBlock external_memory,
                             std::span<uint64_t> buffer)
            : spilling_(spilling)
            ,
            external_memory_(external_memory)
            ,
            buffer_(buffer)
        {
            AssertSplittableBySlots(buffer);
        }
        std::span<uint64_t> ResetBuffer() {
            int next_chunk_size_bytes = std::min(
                buffer_.size_bytes(), external_memory_.BlockSize - bytes_read_);
            if (next_chunk_size_bytes == 0) {
                return {};
            }
            spilling_.Load(external_memory_, bytes_read_, buffer_.data(),
                           next_chunk_size_bytes);
            bytes_read_ += next_chunk_size_bytes;
            return buffer_.subspan(0, next_chunk_size_bytes / 8);
        }

    private:
        TSpilling& spilling_;
        TSpillingBlock external_memory_;
        int64_t bytes_read_ = 0;
        std::span<uint64_t> buffer_;
    };

} // namespace pmerge::ydb

#endif // SPILLING_BLOCKS_BUFFER_HPP
