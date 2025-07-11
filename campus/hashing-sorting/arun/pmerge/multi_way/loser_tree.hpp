//
// Created by nfrmtk on 3/15/25.
//

#ifndef LOSER_TREE_HPP
#define LOSER_TREE_HPP
#include <immintrin.h>

#include <bit>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <campus/hashing-sorting/arun/pmerge/common/resource.hpp>
#include <campus/hashing-sorting/arun/pmerge/two_way/simd_two_way.hpp>
#include <span>
#include <vector>
namespace pmerge::multi_way {
    namespace detail {

        template <pmerge::Resource Resource, size_t Num>
        struct LoserTreeImpl {
            using Type = pmerge::two_way::SimdTwoWayMerger<
                typename LoserTreeImpl<Resource, Num / 2>::Type,
                typename LoserTreeImpl<Resource, Num - Num / 2>::Type>;
            template <typename Iter>
            static Type Make(Iter it) {
                static_assert(Num >= 2);
                return Type{LoserTreeImpl<Resource, Num / 2>::Make(it),
                            LoserTreeImpl<Resource, Num - Num / 2>::Make(it + Num / 2)};
            }
        };

        template <pmerge::Resource Resource>
        struct LoserTreeImpl<Resource, 1> {
            using Type = Resource;
            template <typename Iter>
            static Type Make(Iter value) {
                return Type{*value};
            }
        };

    } // namespace detail

    template <pmerge::Resource Resource, size_t ResourcesAmount>
    using LoserTree =
        typename detail::LoserTreeImpl<Resource, ResourcesAmount>::Type;

    template <pmerge::Resource ThisResource, size_t ResourcesAmount,
              std::ranges::random_access_range Data>
    LoserTree<ThisResource, ResourcesAmount> MakeLoserTree(const Data& range) {
        using DataType = std::ranges::range_value_t<Data>;
        static_assert(std::constructible_from<ThisResource, const DataType&>,
                      "cannot construct Resource from const Data::value_type&");
        return detail::LoserTreeImpl<ThisResource, ResourcesAmount>::Make(
            range.begin());
    }

} // namespace pmerge::multi_way
#endif // LOSER_TREE_HPP
