#pragma once

#include "partitioner.h"

#include <cstdint>
#include <vector>

#include <util/str_stl.h>


struct Hasher {

    Hasher(size_t partitions);

    virtual ~Hasher() = default;
    void accept(int64_t value, int source);

    const std::vector<size_t>& getPartitionStat() const;
    const std::vector<size_t>& getShuffleStat() const;

    double calcShuffle() const;

    double calcMSE() const;

protected:

    virtual int hash_to_bin(int64_t) const = 0;

    size_t out_partitions;
private:
    size_t total;
    std::vector<size_t> stat;

    size_t total_shuffle;
    std::vector<size_t> shuffle;
};


class ModuloHasher : public Hasher {
public:
    ModuloHasher(size_t partitions);
protected:
    int hash_to_bin(int64_t value) const override;

};

class PrimeHasher : public Hasher {
    public:
    PrimeHasher(size_t partitions);

    int hash_to_bin(int64_t value) const override;

    const uint64_t p = 11000053081;
    const uint64_t a = 31;
    const uint64_t b = 151;

};

class THashHasher : public Hasher {
public:
    THashHasher(size_t partitions);
protected:
    int hash_to_bin(int64_t value) const override;

private:
    THash<i64> hasher;

};


class HistCollector : public Hasher {
public:

    HistCollector(size_t partitions, std::shared_ptr<BSPartitioner> partitioner);
    
protected:
    int hash_to_bin(int64_t value) const override;

private:
    std::shared_ptr<BSPartitioner> partitioner;
};
