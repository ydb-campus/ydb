//
// Created by nfrmtk on 6/13/25.
//

#ifndef SPILLING_BLOCK_RESOURCE_HPP
#define SPILLING_BLOCK_RESOURCE_HPP
#include <campus/hashing-sorting/arun/spilling_mem.h>

#include <bitset>
#include <format>
#include <campus/hashing-sorting/arun/pmerge/common/assert.hpp>
#include <campus/hashing-sorting/arun/pmerge/common/resource.hpp>
#include <campus/hashing-sorting/arun/pmerge/ydb/spilling_block_reader.hpp>

#include "campus/hashing-sorting/arun/pmerge/simd/utils.hpp"
namespace pmerge::ydb {
    namespace detail {
        // class BufferedSpillBlock {
        //  public:
        //   BufferedSpillBlock(TSpilling& stats, TSpillingBlock data)
        //       : stats_(stats), data_(data) {
        //   }
        //   std::span<const Slot> ConsumeAll() {
        //     // PMERGE_ASSERT(
        //     //     data_offset_bytes_ % 64 == 0,
        //     //     std::format(
        //     //         "incorrect offset {}, TSpillingBlock should be read by
        //     Slot's",
        //     //         data_offset_bytes_));
        //     int64_t slots_consumed = std::min(
        //         slots,
        //         (static_cast<int64_t>(data_.BlockSize) / 64 - data_offset_slots_));
        //     std::span<const Slot> slots_output = buffer_.subspan(0, slots_consumed);
        //         // static_cast<const Slot*>(data_.ExternalMemory) +
        //         //     data_offset_slots_, static_cast<const
        //         Slot*>(data_.ExternalMemory) + data_offset_slots_ + slots_consumed};
        //     data_offset_slots_ += slots_consumed * kSlotBytes;
        //     return slots_output;
        //   }
        //   template <int Size>
        //   void Consume(std::span<IntermediateInteger>& buffer,
        //                std::bitset<Size> index) noexcept {
        //     constexpr int kIntermediateIntegerSize = sizeof(IntermediateInteger);
        //     int64_t new_span_size = std::min(
        //         std::ssize(buffer),
        //         static_cast<int64_t>(data_.BlockSize / 64) - data_offset_slots_);
        //
        //     buffer = buffer.subspan(0, new_span_size);
        //     int64_t total_external_memory_read_bytes = new_span_size * kSlotBytes;
        //     for (int offset = 0; offset < total_external_memory_read_bytes;
        //          offset += kSlotBytes) {
        //       uint64_t hash;
        //       stats_.Load(data_, data_offset_slots_ * kSlotBytes + offset, &hash,
        //                   sizeof(hash));  // fixme: dont call TSpilling::Load too
        //                   often.
        //       buffer[offset / kSlotBytes] = PackFrom(hash, index);
        //     }
        //     data_offset_slots_ += new_span_size;
        //     // PMERGE_ASSERT(buffer.size() % 4 == 0, "buffer must be splittable in
        //     __m256i");
        //   }
        //   // std::span<const uint64_t> GetLoaded() const noexcept;
        //   // void AdvanceBy(int shift);
        //  private:
        //   TSpilling& stats_;
        //   TSpillingBlock data_;
        //   int64_t data_offset_slots_ = 0;
        //   std::span<Slot> buffer_;
        //   // std::span<uint64_t> buffer_left_;
        //   // const std::span<uint64_t> buffer_;
        // };

    } // namespace detail

    template <int IndexSizeBits>
    class SpillingBlockBufferedResource {
    public:
        SpillingBlockBufferedResource(TSpilling& spilling, TSpillingBlock block,
                                      std::span<uint64_t> my_buffer,
                                      std::bitset<IndexSizeBits> resource_identifier)
            : resource_identifier_(resource_identifier)
            ,
            stats_(spilling)
            ,
            blocks_reader_(
                SpillingBlocksBuffer{spilling, block, my_buffer},
                std::format("resource-{}", resource_identifier.to_ullong()))
            ,
            total_slots_(block.BlockSize >> 6)
            ,
            current_vec_(GetOneHelper())
        {
            static_assert(
                pmerge::Resource<
                    pmerge::ydb::SpillingBlockBufferedResource<IndexSizeBits>>,
                "SpillingBlockResource must be resource");
            pmerge::output << "SpillingBlockResource ctor finished\n";
        }
        IntermediateInteger Peek() const {
            return simd::Get64MostSignificantBits(current_vec_);
        }
        __m256i GetOne() {
            PMERGE_ASSERT_M(
                IsValid(pmerge::simd::Get64MostSignificantBits(current_vec_)),
                "SpillingBlockBufferedResource::GetOne should be called with "
                "current_vec_ being valid");
            auto next = GetOneHelper();
            pmerge::output << "SpillingBlockBufferedResource::GetOne another array: "
                           << simd::ToString(next) << '\n';
            std::swap(next, current_vec_);
            // auto next = _mm256_loadu_si256(
            //     reinterpret_cast<__m256i* const>(current_buffer_.data()));
            return next;
        }

    private:
        __m256i GetOneHelper() {
            pmerge::println("currently_processed_slots_: {}, total_slots_: {}",
                            currently_processed_slots_, total_slots_);
            if (currently_processed_slots_ >= total_slots_) {
                return simd::kInfVector;
            }
            if (currently_processed_slots_ + 4 >
                total_slots_) { // at most once per this resource
                IntermediateInteger pack[4] = {kInf, kInf, kInf, kInf};
                int slots_left = total_slots_ - currently_processed_slots_;
                PMERGE_ASSERT(slots_left >= 0);
                PMERGE_ASSERT(slots_left < 4);
                for (int idx = 0; idx < slots_left; ++idx) {
                    pack[idx] = GetPackedInt();
                }
                return pmerge::simd::MakeFrom(pack[0], pack[1], pack[2], pack[3]);
            }
            IntermediateInteger first = GetPackedInt();
            IntermediateInteger second = GetPackedInt();
            IntermediateInteger third = GetPackedInt();
            IntermediateInteger forth = GetPackedInt();

            return pmerge::simd::MakeFrom(first, second, third, forth);
        }
        IntermediateInteger GetPackedInt() {
            currently_processed_slots_++;
            return PackFrom(GetHash(blocks_reader_.AdvanceByOne()),
                            resource_identifier_);
        }
        // std::span<const Slot> ResetBuffer() {
        //   auto buff = blocks_buffer_.ResetBuffer();
        //   PMERGE_ASSERT(
        //       buff.size() % 4 == 0,
        //       std::format(
        //           "buffer needs to be splittable in groups of 4, current size: {}",
        //           buff.size()));
        //   return buff;
        // }

        std::bitset<IndexSizeBits> resource_identifier_;
        TSpilling& stats_;
        SpillingBlockReader blocks_reader_;
        int64_t total_slots_;
        int64_t currently_processed_slots_ = 0;
        __m256i current_vec_ = simd::kInfVector;
        // pmerge::ydb::SpillingBlocksBuffer blocks_buffer_;
        // std::span<uint64_t> current_buffer_;
    };

} // namespace pmerge::ydb

#endif // SPILLING_BLOCK_RESOURCE_HPP
