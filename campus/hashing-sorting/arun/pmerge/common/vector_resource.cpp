//
// Created by nfrmtk on 4/16/25.
//
#include <cassert>
#include <campus/hashing-sorting/arun/pmerge/common/vector_resource.hpp>
namespace pmerge::common {
    VectorResource::VectorResource(std::vector<pmerge::IntermediateInteger> vec)
        : data_(std::move(vec))
    {
        data_.push_back(pmerge::kInf);
        data_.push_back(pmerge::kInf);
        data_.push_back(pmerge::kInf);
        data_.push_back(pmerge::kInf);
        // std::cout << std::format("VectorResource size is : {}\n",
        // std::ssize(data_));
    }
    int64_t VectorResource::Peek() const {
        return data_[pos_];
    }
    __m256i VectorResource::GetOne() {
        assert(!data_.empty());
        auto reg = _mm256_loadu_si256(reinterpret_cast<__m256i_u*>(&data_[pos_]));
        // std::cout << '{';
        // for(auto num: data_) {
        //   std::cout << num << ' ';
        // }
        // std::cout << "}\n";
        // std::cout << std::format("pos: {}, ssize(data) = {}, loaded: {}\n", pos_,
        // std::ssize(data_), ToHexString(reg));
        AdvancePosition();
        return reg;
    }
    void VectorResource::AdvancePosition() {
        pos_ += 4;
        if (pos_ + 4 > std::ssize(data_)) {
            pos_ = std::ssize(data_) - 4;
        }
    }
} // namespace pmerge::common
