#pragma once

#include <util/system/types.h>
#include <algorithm>
#include <vector>
#include <iostream>


struct Bucket {
    i64 left;
    i64 right;
    ui64 count;
};

struct FreqBucket {
    i64 left;
    i64 right;
    double count;
};

template<class TBucket>
struct Histogram {
    std::vector<TBucket> buckets;

    Histogram() = default;

    Histogram(std::vector<TBucket> buckets) 
    : buckets(std::move(buckets))
    { }

    Histogram<Bucket>(std::vector<i64> data, i32 parts)
    : buckets()
    {
        buckets.reserve(parts);
        std::sort(data.begin(), data.end());
        size_t approx = data.size() / parts;

        i64 prev = data[0];
        i64 left = 0;
        i32 partNum = 1;
        for (size_t i = 0; i < data.size(); i++) {
            if (i - left >= approx && data[i] != prev) {
                if (partNum == parts) {
                    break;
                }
                buckets.emplace_back(data[left], data[i], i - left );
                left = i;
                partNum++;
            }
            prev = data[i];
        }
        buckets.emplace_back(data[left], data[data.size() - 1], data.size() - left );
    }
};

Histogram<FreqBucket> multiMerge(const std::vector<Histogram<Bucket>>& sources);

struct Partition {
    i64 left;
    i64 right;
    i32 out;
};

struct Bin {
    std::vector<const FreqBucket*> buckets;
    ui64 sum;
};

using BinArray = std::vector<Bin>;

BinArray multifit(const Histogram<FreqBucket>& hist, ui32 partitionsNum);

template<class TBucket>
inline std::ostream& operator<<(std::ostream& out, const Histogram<TBucket>& hist) {
    out << "{ ";
    size_t sz = hist.buckets.size();
    for (size_t i = 0; i < sz; i++) {
        const TBucket& b = hist.buckets[i];
        out << "[" << b.left << " (" << b.count << ") " << b.right << "]";
        if (i != sz - 1) out << ", ";
    }
    out << "}";
    return out;
}

inline std::ostream& operator<<(std::ostream& out, const BinArray& arr) {
    out << "{ ";
    size_t sz = arr.size();
    for (size_t i = 0; i < sz; i++) {
        const Bin& b = arr[i];
        out << "[" << b.sum << "]";
        if (i != sz - 1) out << ", ";
    }
    out << "}";
    return out;
}