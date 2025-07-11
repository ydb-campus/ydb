//
// Created by nfrmtk on 6/13/25.
//

#ifndef TYPES_HPP
#define TYPES_HPP
#include <algorithm>
#include <cstdint>
#include <campus/hashing-sorting/arun/pmerge/common/assert.hpp>
#include <source_location>
#include <span>
typedef uint32_t ui32;
typedef int32_t i32;

typedef uint64_t ui64;
typedef int64_t i64;

namespace pmerge::ydb {

    static constexpr int kSlotBytes = 64;
    using SlotView = std::span<uint64_t, 8>;
    using ConstSlotView = std::span<const uint64_t, 8>;
    struct Slot {
        static Slot FromView(ConstSlotView view) {
            Slot s{};
            std::ranges::copy(view, s.nums);
            return s;
        }
        ConstSlotView AsView() const {
            return nums;
        }
        uint64_t nums[8];
    };
    inline SlotView GetSlot(std::span<uint64_t>& buffer) {
        SlotView ret = buffer.subspan<0, 8>();
        buffer = buffer.subspan<8>();
        return ret;
    }

    inline ConstSlotView GetConstSlot(std::span<const uint64_t>& buffer) {
        ConstSlotView ret = buffer.subspan<0, 8>();
        buffer = buffer.subspan<8>();
        return ret;
    }

    inline void AssertSplittableBySlots(
        std::span<const uint64_t> buffer,
        [[maybe_unused]] std::source_location loc = std::source_location::current()) {
        PMERGE_ASSERT_M(buffer.size_bytes() % kSlotBytes == 0, loc.function_name());
    }

    inline uint64_t GetHash(ConstSlotView slot) {
        return slot[0];
    }
    inline uint64_t GetHash(const Slot& slot) {
        return slot.nums[0];
    }
    inline ConstSlotView AsView(const Slot& slot) {
        return slot.nums;
    }
    inline SlotView AsView(Slot& slot) {
        return slot.nums;
    }

    inline uint64_t& GetAggregateValue(SlotView slot) {
        return slot[7];
    }
    template <ui64 KeyCount>
    using Key = std::span<const uint64_t, KeyCount>;

    template <ui64 KeyCount>
    Key<KeyCount> GetKey(ConstSlotView slot) {
        return Key<KeyCount>{&slot[1], &slot[1 + KeyCount]};
    }

    template <size_t keyCount>
    bool Equal(pmerge::ydb::Key<keyCount> first,
               pmerge::ydb::Key<keyCount> second) {
        for (int idx = 0; idx < int{keyCount}; ++idx) {
            if (first[idx] != second[idx]) {
                return false;
            }
        }
        return true;
    }

    template <size_t keyCount>
    bool Less(pmerge::ydb::Key<keyCount> first, pmerge::ydb::Key<keyCount> second) {
        for (int idx = 0; idx < int{keyCount}; ++idx) {
            if (first[idx] > second[idx]) {
                return false;
            }
            if (first[idx] < second[idx]) {
                return true;
            }
        }
        return false;
    }

    // SlotLess satisfies strict weak ordering.
    template <ui32 keyCount>
    bool SlotLess(const pmerge::ydb::Slot& first, const pmerge::ydb::Slot& second) {
        if (pmerge::ydb::GetHash(first) < pmerge::ydb::GetHash(second)) {
            return true;
        }
        if (pmerge::ydb::GetHash(first) > pmerge::ydb::GetHash(second)) {
            return false;
        }
        auto first_span = pmerge::ydb::GetKey<keyCount>(first.AsView());
        auto second_span = pmerge::ydb::GetKey<keyCount>(second.AsView());
        if (Equal(first_span, second_span)) {
            return false;
        }
        if (Less(first_span, second_span)) {
            return true;
        } else {
            PMERGE_ASSERT(
                std::ranges::lexicographical_compare(second_span, first_span));
            return false;
        }
    }
    template <ui32 keyCount>
    bool SlotEqual(pmerge::ydb::ConstSlotView first,
                   pmerge::ydb::ConstSlotView second) {
        return pmerge::ydb::GetHash(first) == pmerge::ydb::GetHash(second) &&
               Equal(pmerge::ydb::GetKey<keyCount>(first),
                     pmerge::ydb::GetKey<keyCount>(second));
    }

} // namespace pmerge::ydb

#endif // TYPES_HPP
