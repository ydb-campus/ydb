//
// Created by nfrmtk on 6/14/25.
//

#ifndef MERGE_SPILLING_BLOCKS_HPP
#define MERGE_SPILLING_BLOCKS_HPP
#include <sys/stat.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <campus/hashing-sorting/arun/pmerge/multi_way/loser_tree.hpp>
#include <campus/hashing-sorting/arun/pmerge/ydb/spilling_block_reader.hpp>
#include <campus/hashing-sorting/arun/pmerge/ydb/spilling_block_resource.hpp>
#include <campus/hashing-sorting/arun/pmerge/ydb/spilling_blocks_writer.hpp>
#include <ranges>

#include "campus/hashing-sorting/arun/pmerge/common/assert.hpp"
#include "campus/hashing-sorting/arun/pmerge/common/print.hpp"
#include "campus/hashing-sorting/arun/pmerge/common/resource.hpp"
#include "campus/hashing-sorting/arun/pmerge/simd/utils.hpp"
#include "campus/hashing-sorting/arun/pmerge/ydb/types.hpp"
#include "campus/hashing-sorting/arun/spilling_mem.h"

namespace pmerge::ydb {
    template <bool finalize /*use fo?*/, ui32 keyCount /*hashed string length*/,
              ui32 p /*loser tree depth*/>
    ui32 merge2pway(ui64* wordsBuffer, ui32 BufferSizeSlots, TSpilling& sp,
                    std::deque<TSpillingBlock>& spills) {
        int64_t slotBufferSize = BufferSizeSlots;
        int buffer_offset_slots = 0;
        auto make_buffer_with_size = [&](int size) -> std::span<ui64> {
            PMERGE_ASSERT(size > 0);
            PMERGE_ASSERT(buffer_offset_slots + size <= slotBufferSize);
            buffer_offset_slots += size;
            return {wordsBuffer + 8 * (buffer_offset_slots - size),
                    wordsBuffer + 8 * buffer_offset_slots};
        };
        constexpr i32 n = 1ul << p;
        using Identifer = std::bitset<p>;
        uint64_t external_buffer_size = slotBufferSize / 3;
        auto input_buffer_size = external_buffer_size >> p;
        auto output_buffer_size = external_buffer_size >> p;
        SpillingBlocksWriter external_memory_buffer{
            sp, sp.Empty(0), make_buffer_with_size(external_buffer_size)};
        using TreeType =
            pmerge::multi_way::LoserTree<SpillingBlockBufferedResource<p>, 1 << p>;
        assert(spills.size() >= n);
        std::vector<pmerge::ydb::SpillingBlockReader> output_readers;
        for (int idx = 0; idx < n; ++idx) {
            output_readers.emplace_back(
                SpillingBlocksBuffer{sp, spills[idx],
                                     make_buffer_with_size(output_buffer_size)},
                std::format("output-reader-{}", idx));
        }
        TreeType loser_tree = pmerge::multi_way::MakeLoserTree<
            SpillingBlockBufferedResource<p>, 1 << p>(
            std::views::iota(0, n) | std::views::transform([&](size_t block_index) {
                return SpillingBlockBufferedResource<p>(
                    sp, spills[block_index], make_buffer_with_size(input_buffer_size),
                    Identifer{block_index});
            }));
        std::deque<IntermediateInteger> nums_left;
        auto append_array = [&](const std::array<IntermediateInteger, 4>& arr) {
            nums_left.push_back(arr[0]);
            nums_left.push_back(arr[1]);
            nums_left.push_back(arr[2]);
            nums_left.push_back(arr[3]);
        };
        std::vector<pmerge::ydb::Slot> slots;
        auto reset_amounts_of_slots_from = [&] {
            auto read_num = [&]() {
                nums_left.pop_front();
                return nums_left.front();
            };
            slots.clear();
            PMERGE_ASSERT_M(!nums_left.empty(), "numbers shouldn't be empty");
            pmerge::IntermediateInteger this_num = nums_left.front();
            const uint64_t current_hash_bits = *pmerge::ExtractCutHash<p>(this_num);
            while (!nums_left.empty() &&
                   *pmerge::ExtractCutHash<p>(this_num) == current_hash_bits) {
                PMERGE_ASSERT_M(this_num != kInf, "only real numbers here are allowed");
                pmerge::println("{}", this_num - std::numeric_limits<int64_t>::min());
                size_t index = pmerge::ExtractIdentifer<p>(this_num)->to_ullong();
                slots.emplace_back(Slot::FromView(output_readers[index].AdvanceByOne()));
                this_num = read_num();
            }
        };
        while (true) {
            if (loser_tree.Peek() == kInf && nums_left.empty()) {
                break;
            }

            while (nums_left.empty() || ExtractCutHash<p>(nums_left.front()) ==
                                            ExtractCutHash<p>(loser_tree.Peek())) {
                PMERGE_ASSERT_M(nums_left.front() != kInf,
                                "nums_left are actual nums not inf");
                append_array(simd::AsArray(loser_tree.GetOne()));
            }
            while (nums_left.back() == kInf) { // merge almost ended
                nums_left.pop_back();
            }
            reset_amounts_of_slots_from();
            if (slots.size() == 1) {
                external_memory_buffer.Write(AsView(slots.front()));
            } else {
                std::ranges::sort(slots, [](const Slot& first, const Slot& second) {
                    return SlotLess<keyCount>(first, second);
                });
                SlotView current = AsView(slots.front());
                for (int idx = 1; idx < std::ssize(slots); idx++) {
                    if (SlotEqual<keyCount>(slots[idx - 1].AsView(), slots[idx].AsView())) {
                        GetAggregateValue(current) += GetAggregateValue(AsView(slots[idx]));
                    } else {
                        external_memory_buffer.Write(current);
                        current = AsView(slots[idx]);
                    }
                }
                external_memory_buffer.Write(current);
            }
        }
        external_memory_buffer.Flush();
        spills.push_back(external_memory_buffer.GetExternalMemory());
        for (int idx = 0; idx < n; ++idx) {
            sp.Delete(spills.front());
            spills.pop_front();
        }
        return external_memory_buffer.TotalWrites();
    }
} // namespace pmerge::ydb

#endif // MERGE_SPILLING_BLOCKS_HPP
