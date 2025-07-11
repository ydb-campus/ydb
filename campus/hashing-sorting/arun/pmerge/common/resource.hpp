//
// Created by nfrmtk on 4/11/25.
//

#ifndef RESOURCE_HPP
#define RESOURCE_HPP
#include <strings.h>

#include <bitset>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <optional>
#include <limits>
#include <campus/hashing-sorting/arun/pmerge/common/assert.hpp>
namespace pmerge {
    using IntermediateInteger = std::int64_t;

    template <typename T>
    concept Resource = requires(T t) {
        t.GetOne();
        { t.Peek() } -> std::same_as<IntermediateInteger>;
        // } && std::copyable<T>;
    };

    // https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm256_cmpgt_ep&ig_expand=4455,4419,1024,1024
    // avx2 doesn't allow for efficient implementation of MinMax for uint64, only
    // int64. So, all numbers will be negative, or anything >= 0 as 'infinity'.
    // max() is chosen for easier testing
    static constexpr IntermediateInteger kInf =
        std::numeric_limits<IntermediateInteger>::max();

    inline bool IsValid(IntermediateInteger integer) {
        return integer < 0 || integer == kInf;
    }

    template <uint64_t IndexSize>
    IntermediateInteger PackFrom(uint64_t num, std::bitset<IndexSize> index) {
        static_assert(IndexSize <= 10, "index must be small");
        uint64_t taken_bits = (num >> (IndexSize + 1)) << IndexSize;
        constexpr uint64_t kMaxIndex = 1ull << IndexSize;
        PMERGE_ASSERT(index.to_ullong() < kMaxIndex);
        PMERGE_ASSERT((index.to_ullong() & (taken_bits << IndexSize)) == 0);
        IntermediateInteger intermediate =
            static_cast<IntermediateInteger>(index.to_ullong()) +
            std::numeric_limits<IntermediateInteger>::min() +
            static_cast<IntermediateInteger>(taken_bits);
        PMERGE_ASSERT(IsValid(intermediate));
        return intermediate;
    }
    std::string MakeReadableString(pmerge::IntermediateInteger num);

    template <pmerge::Resource Resource>
    bool IsResourceFinished(const Resource& res) {
        return res.Peek() == kInf;
    }
    template <size_t IndexSize>
    std::optional<std::bitset<IndexSize>> ExtractIdentifer(
        IntermediateInteger num) {
        PMERGE_ASSERT_M(IsValid(num),
                        "Extracting identifier from invalid or infinite number {}");
        if (num == kInf) {
            return std::nullopt;
        }
        return std::make_optional<std::bitset<IndexSize>>(num &
                                                          ((1 << IndexSize) - 1));
    }

    template <size_t IndexSize>
    std::optional<uint64_t> ExtractCutHash(IntermediateInteger num) {
        PMERGE_ASSERT_M(IsValid(num),
                        "Extracting identifier from invalid or infinite number {}");
        if (num == kInf) {
            return std::nullopt;
        } else {
            return std::make_optional<uint64_t>(num >> IndexSize);
        }
    }

    static_assert(31 >> 2 == 7);

    template <pmerge::Resource LhsResource, pmerge::Resource RhsResource>
    bool operator==(std::remove_reference_t<LhsResource>&& lhs,
                    std::remove_reference_t<RhsResource>&& rhs) {
        while (!IsResourceFinished(lhs) && !IsResourceFinished(rhs)) {
            auto lhs_reg = AsArray(lhs.GetOne());
            auto rhs_reg = AsArray(rhs.GetOne());
            if (lhs_reg != rhs_reg) {
                return false;
            }
        }
        return IsResourceFinished(lhs) && IsResourceFinished(rhs);
    }

} // namespace pmerge

#endif // RESOURCE_HPP
