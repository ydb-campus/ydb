#pragma once

#include "merger.h"


struct Partition {
    i64 left;
    i64 right;
    i32 out;
};

struct Bin {
    std::vector<const MultiBucket*> buckets;
    ui64 sum = 0;

    void addBucket(const MultiBucket* bucket);
};

using BinArray = std::vector<Bin>;


BinArray multifit(MultiHistogram hist, ui32 partitionsNum);

BinArray greedyLPT(MultiHistogram hist, ui32 partitionsNum);

BinArray simpleBalance(const MultiHistogram& hist, ui32 partitionsNum);

BinArray greedyBalance(MultiHistogram hist, int partitionsNum);

BinArray multiBalance(MultiHistogram hist, int partitionsNum, double limitCoef);

struct BSPartitioner {


    using mapping = std::pair<const MultiBucket*, int>;
    // необязательно бакеты, в общем-то, просто ренжи
    std::vector<mapping> sortedBuckets;

    BSPartitioner(const BinArray& partitioning);

    int decide_partition(i64 key) const;

private:

};


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