//
// Created by nfrmtk on 6/15/25.
//

#ifndef SPILLING_BLOCK_READER_HPP
#define SPILLING_BLOCK_READER_HPP
#include <campus/hashing-sorting/arun/pmerge/ydb/spilling_blocks_buffer.hpp>

#include "campus/hashing-sorting/arun/pmerge/common/print.hpp"
#include "campus/hashing-sorting/arun/pmerge/common/resource.hpp"
#include "campus/hashing-sorting/arun/pmerge/simd/utils.hpp"
#include "campus/hashing-sorting/arun/pmerge/ydb/types.hpp"

namespace pmerge::ydb {

    class SpillingBlockReader {
    public:
        explicit SpillingBlockReader(
            const SpillingBlocksBuffer& external_memory_cache, std::string name)
            : external_memory_cache_(external_memory_cache)
            , name_(std::move(name))
        {
        }
        SlotView AdvanceByOne() {
            pmerge::println("slots left in storage: {}", slots_not_read_.size() >> 3);
            if (slots_not_read_.empty()) {
                slots_not_read_ = external_memory_cache_.ResetBuffer();
                pmerge::println("reader '{}' read from external memory {} bytes", name_,
                                slots_not_read_.size_bytes());
                PMERGE_ASSERT_M(!slots_not_read_.empty(), "reading past-the-end block");
            }
            auto next = GetSlot(slots_not_read_);
            pmerge::println("Get another slot with hash: {:x}\n", GetHash(next));
            return next;
        }

    private:
        SpillingBlocksBuffer external_memory_cache_;
        std::span<uint64_t> slots_not_read_;
        std::string name_;
    };

} // namespace pmerge::ydb
#endif // SPILLING_BLOCK_READER_HPP
