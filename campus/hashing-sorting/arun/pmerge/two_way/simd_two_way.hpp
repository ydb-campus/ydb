//
// Created by nfrmtk on 3/15/25.
//

#ifndef SIMD_TWO_WAY_HPP
#define SIMD_TWO_WAY_HPP
#include <immintrin.h>

#include <campus/hashing-sorting/arun/pmerge/common/resource.hpp>
#include <campus/hashing-sorting/arun/pmerge/simd/inregister_merge.hpp>
#include <campus/hashing-sorting/arun/pmerge/simd/utils.hpp>
#include <source_location>
namespace pmerge::two_way {

    template <pmerge::Resource FirstSource, pmerge::Resource SecondSource>
    class SimdTwoWayMerger {
    public:
        using ValueType = __m256i;
        SimdTwoWayMerger(FirstSource first, SecondSource second) noexcept
            : SimdTwoWayMerger(std::move(first), std::move(second), true)
        {
        }
        SimdTwoWayMerger(FirstSource first, SecondSource second,
                         bool print_registers) noexcept
            : print_registers_(print_registers)
            ,
            first_(std::forward<FirstSource>(first))
            ,
            second_(std::forward<SecondSource>(second))
            ,
            vMin_(pmerge::simd::kInfVector)
            ,
            vMax_(pmerge::simd::kInfVector)
        {
            static_assert(pmerge::Resource<SimdTwoWayMerger>,
                          "SimdTwoWayMerger must be pmerge::Resource");
            vMin_ = first_.GetOne();
            vMax_ = second_.GetOne();
            PrintRegister(
                "SimdTwoWayMerger ctor, just read things from first and second");
            PMERGE_MERGE(vMin_, vMax_)
            PrintRegister("SimdTwoWayMerger ctor, merged vMin_ and vMax_");
        }
        ValueType GetOne() {
            PrintRegister("SimdTwoWayMerger::GetOne begin");
            ValueType ret = vMin_;
            if (first_.Peek() < second_.Peek()) {
                vMin_ = first_.GetOne();
            } else {
                vMin_ = second_.GetOne();
            }

            PrintRegister(
                "SimdTwoWayMerger::GetOne after get next, before merge next and prev");
            PMERGE_MERGE(vMin_, vMax_)
            PrintRegister("SimdTwoWayMerger::GetOne after merge next and prev");
            return ret;
        }
        [[nodiscard]] IntermediateInteger Peek() const {
            PrintRegister("SimdTwoWayMerger::Peek");
            return pmerge::simd::Get64MostSignificantBits(vMin_);
        }

    private:
        void PrintRegister(
            [[maybe_unused]] std::string_view prefix = "",
            [[maybe_unused]] std::source_location loc = std::source_location::current()) const {
#if false
    if (print_registers_) {
      pmerge::println(
          "state: {},  min: {}, max: {}, header line number: {}\n", prefix,
          simd::ToString(vMin_), simd::ToString(vMax_), loc.line());
    }
#endif
        }
        bool print_registers_;
        FirstSource first_;
        SecondSource second_;
        ValueType vMin_;
        ValueType vMax_;
    };

    template <typename P1, typename P2>
    SimdTwoWayMerger(P1, P2) -> SimdTwoWayMerger<P1, P2>;
} // namespace pmerge::two_way
#endif // SIMD_TWO_WAY_HPP
