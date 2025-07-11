//
// Created by nfrmtk on 6/14/25.
//
#include <format>
#include <campus/hashing-sorting/arun/pmerge/common/assert.hpp>
#include <campus/hashing-sorting/arun/pmerge/common/resource.hpp>
std::string pmerge::MakeReadableString(pmerge::IntermediateInteger num) {
    PMERGE_ASSERT_M(IsValid(num), std::format("invalid number: {}", num));
    if (num == kInf) {
        return "inf";
    }
    return std::format("{}", num - std::numeric_limits<int64_t>::min());
}
