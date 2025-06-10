#pragma once

#include <util/system/types.h>
#include <vector>
#include <iostream>


enum class HistogramForm {
    GrowingRight,
    GrowingLeft,
    GrowingBoth,
    FullRange,
};

enum class HistogramDistribution {
    Ascending,
    GenerallyAscending,
    Descending,
    GeneralyDescending,
    Random,
};

struct Bucket {
    i64 left;
    i64 right;
    ui64 count;
};

struct Histogram {
    std::vector<Bucket> buckets;

    Histogram() = default;

    Histogram(const Histogram&) = default;
    Histogram& operator=(const Histogram&) = default;

    Histogram(Histogram&&) = default;
    Histogram& operator=(Histogram&&) = default;

    Histogram(std::vector<Bucket> buckets);
};

Histogram buildOrdered(const std::vector<i64>& c_data, int bucketsCount);

Histogram buildP2(const std::vector<int64_t>& data, size_t bucketsCount);





inline std::ostream& operator<<(std::ostream& out, const Histogram& hist) {
    out << "{ ";
    size_t sz = hist.buckets.size();
    for (size_t i = 0; i < sz; i++) {
        const Bucket& b = hist.buckets[i];
        out << "[" << b.left << " (" << b.count << ") " << b.right << "]";
        if (i != sz - 1) out << ", ";
    }
    out << "}";
    return out;
}
