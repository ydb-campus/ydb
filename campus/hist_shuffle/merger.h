#pragma once

#include "histogram.h"

#include <map>


struct MultiBucket {
    i64 left;
    i64 right;
    ui64 count;
    std::map<int, Bucket> buckets;
    int maxCountIdx;

    MultiBucket() = delete;
    MultiBucket(i64 left, i64 right);

    void addBucket(Bucket bucket, int source);
};

struct MultiHistogram {
    std::vector<MultiBucket> buckets;
};

MultiHistogram multiMerge(const std::vector<Histogram>& sources);


inline std::ostream& operator<<(std::ostream& out, const MultiHistogram& hist) {
    out << "{ ";
    size_t sz = hist.buckets.size();
    for (size_t i = 0; i < sz; i++) {
        const MultiBucket& b = hist.buckets[i];
        out << "[" << b.left << " (" << b.count << ") ";

        //out << "<" << b.maxCountIdx << " , " << b.buckets.at(b.maxCountIdx).count << ">";

        // for (const auto& [source, bucket] : b.buckets) {
        //     out << "<" << source << " , " << bucket.count << "> ";
        // }
        out << b.right << "]";

        if (i != sz - 1) out << ", ";
    }
    out << "}";
    return out;
}
