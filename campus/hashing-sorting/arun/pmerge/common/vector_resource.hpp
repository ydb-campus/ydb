//
// Created by nfrmtk on 4/16/25.
//

#ifndef VECTOR_RESOURCE_HPP
#define VECTOR_RESOURCE_HPP

#include <immintrin.h>

#include <bitset>
#include <cstdint>
#include <campus/hashing-sorting/arun/pmerge/common/resource.hpp>
#include <vector>

namespace pmerge::common {
    template <size_t IndexSize>
    std::vector<int64_t> FromDataAndIndex(const std::vector<uint64_t>& data,
                                          std::bitset<IndexSize> index) {
        std::vector<pmerge::IntermediateInteger> vec;
        for (int64_t num : data) {
            vec.push_back(pmerge::PackFrom(num, index));
        }
        return vec;
    }

    class VectorResource {
    public:
        explicit VectorResource(std::vector<pmerge::IntermediateInteger> vec);
        VectorResource(const VectorResource& other) = delete;
        VectorResource& operator=(const VectorResource& other) = delete;

        VectorResource(VectorResource&& other) noexcept = default;
        VectorResource& operator=(VectorResource&& other) noexcept = default;

        [[nodiscard]] int64_t Peek() const;

        __m256i GetOne();

    private:
        void AdvancePosition();
        std::vector<pmerge::IntermediateInteger> data_{};
        int pos_ = 0;
    };
} // namespace pmerge::common

static_assert(pmerge::Resource<pmerge::common::VectorResource>);

#endif // VECTOR_RESOURCE_HPP
