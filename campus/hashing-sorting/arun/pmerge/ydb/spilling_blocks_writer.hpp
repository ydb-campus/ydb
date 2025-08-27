//
// Created by nfrmtk on 6/18/25.
//

#ifndef SPILLING_BLOCKS_WRITER_HPP
#define SPILLING_BLOCKS_WRITER_HPP
#include <campus/hashing-sorting/arun/spilling_mem.h>

#include <campus/hashing-sorting/arun/pmerge/utils/time_interval.hpp>
#include <campus/hashing-sorting/arun/pmerge/ydb/types.hpp>
#include <span>

#include "campus/hashing-sorting/arun/pmerge/common/assert.hpp"
#include "campus/hashing-sorting/arun/pmerge/common/print.hpp"
namespace pmerge::ydb {

    class SpillingBlocksWriter {
    public:
        SpillingBlocksWriter(TSpilling& stats, TSpillingBlock external_memory,
                             std::span<uint64_t> buffer)
            : stats_(stats)
            ,
            external_memory_(external_memory)
            ,
            buffer_(buffer)
            ,
            buffer_left_(buffer_)
        {
        }
        void Write(ConstSlotView slot) {
            total_writes_++;
            if (total_writes_ % kCurrentIntervalSlots == 0) {
                static WriteInterval write_interval;
                write_interval.WriteHappend(kCurrentIntervalSlots);
            }
            if (buffer_left_.empty()) {
                Flush();
            }
            std::ranges::copy(slot, buffer_left_.data());

            buffer_left_ = buffer_left_.subspan(8);
        }
        void Flush() {
            size_t bytes_flushed = buffer_.size_bytes() - buffer_left_.size_bytes();
            external_memory_ =
                stats_.Append(external_memory_, buffer_.data(), bytes_flushed);
            buffer_left_ = buffer_;
            pmerge::println(
                "write another {} bytes to memory, currently {} bytes in "
                "external storage",
                bytes_flushed, external_memory_.BlockSize);
        }
        ~SpillingBlocksWriter() {
            PMERGE_ASSERT_M(buffer_left_.size() == buffer_.size(),
                            "writer buffer dropping some slots not written to memory");
        }
        auto GetExternalMemory() {
            return external_memory_;
        }
        int64_t TotalWrites() const {
            return total_writes_;
        }

    private:
        TSpilling& stats_;
        int64_t total_writes_ = 0;
        TSpillingBlock external_memory_;
        const std::span<uint64_t> buffer_;
        std::span<uint64_t> buffer_left_;
    };
} // namespace pmerge::ydb
#endif // SPILLING_BLOCKS_WRITER_HPP
