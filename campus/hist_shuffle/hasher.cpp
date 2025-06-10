#include "hasher.h"

#include <cassert>

#include <parquet/murmur3.h>

Hasher::Hasher(size_t partitions) : out_partitions(partitions), total(0), stat(partitions), total_shuffle(0), shuffle(partitions) 
{}

const std::vector<size_t>& Hasher::getPartitionStat() const {
    return stat;
}
const std::vector<size_t>& Hasher::getShuffleStat() const {
    return shuffle;
}

void Hasher::accept(int64_t value, int source) {
    int target = hash_to_bin(value);
    assert(target < static_cast<int>(out_partitions));
    total++;
    stat[target]++;
    if (target != source) {
        total_shuffle++;
        shuffle[source]++;
    }
}

double Hasher::calcShuffle() const {
    return total_shuffle * 1. / total;
}

double Hasher::calcMSE() const {
    i64 ideal = total / out_partitions;

    double squares_sum = 0;
    for (const auto& part : stat) {
        i64 delta = static_cast<i64>(part) - ideal;
        double normalized_delta = delta * 1. / total;
        squares_sum += normalized_delta * normalized_delta;
    }
    
    return squares_sum / out_partitions;
}

ModuloHasher::ModuloHasher(size_t partitions) : Hasher(partitions)
{}

int ModuloHasher::hash_to_bin(int64_t value) const {
    return value % out_partitions;
}

PrimeHasher::PrimeHasher(size_t partitions) : Hasher(partitions)
{}

//High Speed Hashing for Integers and Strings
// Mikkel Thorup 2012
int PrimeHasher::hash_to_bin(int64_t value) const {
    // hashes x strongly universally into the range [m]
    // using the random seeds a and b.

    return ((a*value+b) % p) % out_partitions;
}


THashHasher::THashHasher(size_t partitions) : Hasher(partitions), hasher()
{}
int THashHasher::hash_to_bin(int64_t value) const {
    return hasher(value) % out_partitions;
}

HistCollector::HistCollector(size_t partitions, std::shared_ptr<BSPartitioner> partitioner) : Hasher(partitions), partitioner(std::move(partitioner))
{}

int HistCollector::hash_to_bin(int64_t value) const {
    return partitioner->decide_partition(value);
}

