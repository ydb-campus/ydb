//
// Created by nfrmtk on 4/9/25.
//

#ifndef SIMD_UTILS_HPP
#define SIMD_UTILS_HPP
#include <immintrin.h>

#include <array>
#include <campus/hashing-sorting/arun/pmerge/common/print.hpp>
#include <campus/hashing-sorting/arun/pmerge/common/resource.hpp>
#include <campus/hashing-sorting/arun/pmerge/simd/inregister_merge.hpp>
#include <string>
namespace pmerge::simd {

    inline __m256i MakeFrom(int64_t n1, int64_t n2, int64_t n3, int64_t n4) {
        return _mm256_set_epi64x(n4, n3, n2, n1);
    }

    template <size_t IndexBits>
    inline __m256i MakeFromDataAndBitset(uint64_t n1, uint64_t n2, uint64_t n3,
                                         uint64_t n4,
                                         std::bitset<IndexBits> index) {
        return MakeFrom(pmerge::PackFrom(n1, index), pmerge::PackFrom(n2, index),
                        pmerge::PackFrom(n3, index), pmerge::PackFrom(n4, index));
    }
    inline bool CompareRegistersEqual(__m256i lhs, __m256i rhs) {
        __m256i pcmp = _mm256_cmpeq_epi32(lhs, rhs); // epi8 is fine too
        unsigned bitmask = _mm256_movemask_epi8(pcmp);
        return bitmask == 0xffffffffU;
    }

    template <typename T, int Shift>
    T GetAtPos(__m256i reg) {
        return static_cast<T>(_mm256_extract_epi64(reg, Shift));
    }
    std::string ToString(__m256i reg);

    std::string ToHexString(__m256i reg);

    inline std::array<int64_t, 4> AsArray(__m256i reg) {
        pmerge::println("pmerge::simd::AsArray reg: {}",
                        pmerge::simd::ToHexString(reg));
        return {GetAtPos<int64_t, 0>(reg), GetAtPos<int64_t, 1>(reg),
                GetAtPos<int64_t, 2>(reg), GetAtPos<int64_t, 3>(reg)};
    }

    static const __m256i kInfVector =
        pmerge::simd::MakeFrom(kInf, kInf, kInf, kInf);

    inline IntermediateInteger Get64MostSignificantBits(const __m256i& reg) {
        return _mm256_extract_epi64(reg, 0);
    }

} // namespace pmerge::simd

namespace pmerge {
    std::string ToStringHex(int64_t num);

} // namespace pmerge

namespace std {
    template <>
    struct formatter<__m256i> {
    };
} // namespace std
#endif // SIMD_UTILS_HPP
