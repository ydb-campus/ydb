#include "partitioner.h"

#include <algorithm>
#include <cassert>
#include <queue>
#include <unordered_map>


//#define DEBUG

void Bin::addBucket(const MultiBucket* bucket) {
    this->buckets.push_back(bucket);
    sum += bucket->count;
}

static BinArray first_fit_decreasing(const std::vector<const MultiBucket*> &sortedBuckets, ui64 bin_size) {
    BinArray result;
    result.push_back(Bin {});

    #ifdef DEBUG
    std::cerr << "run ffd with size " << bin_size << "\n";
    #endif
    
    for (const MultiBucket* bucket : sortedBuckets) {
        ui64 value = bucket->count;
        if (value > bin_size) {
            std::cerr << "bin size " << bin_size << " smaller than value " << value << "\n";
            assert(value <= bin_size);
        }
        size_t i = 0;
        for ( ; i < result.size(); i++) {
            if (result[i].sum + value <= bin_size) {
                result[i].addBucket(bucket);
                break;
            }
        }
        // didn't find bin, create new
        if (i == result.size()) {
            result.push_back(Bin { std::vector {bucket}, value });
        }
    }

    return result;
}

BinArray multifit(MultiHistogram hist, ui32 partitionsNum) {
    ui64 sum = 0;
    ui64 max = hist.buckets[0].count;

    std::vector<const MultiBucket*> sorted;
    sorted.reserve(hist.buckets.size());
    for (const auto& bucket : hist.buckets) {
        sorted.push_back(&bucket);
        sum += bucket.count;
        if (bucket.count > max) {
            max = bucket.count;
        }
    }

    std::sort(sorted.begin(), sorted.end(), [](const MultiBucket* left, const MultiBucket* right) {return left->count > right->count;});


    ui64 lower_size = std::max(sum / partitionsNum, max);
    ui64 upper_size = std::max(2 * sum / partitionsNum, max);
#ifdef DEBUG
    //std::cerr << "max: " << max << " sum / parts " << sum / partitionsNum << "\n";
    //std::cerr << "lower " << lower_size << " upper " << upper_size << "\n";
#endif

    for (int i = 0; i < 20; i++) {
        ui64 bin_size = (lower_size + upper_size) / 2;
        auto first_fit = first_fit_decreasing(sorted, bin_size);
        auto ffd_bin_count = first_fit.size();
        if (ffd_bin_count <= partitionsNum) {
            upper_size = bin_size;
        } else {
            lower_size = bin_size;
        }
    }
    
    return first_fit_decreasing(sorted, upper_size);
}

static size_t minBin(BinArray& arr) {
    ui64 minSum = arr[0].sum;
    size_t minIdx = 0;
    for (size_t i = 0; i < arr.size(); i++) {
        if (arr[i].sum < minSum) {
            minIdx = i;
            minSum = arr[i].sum;
        }
    }
    return minIdx;
}

BinArray greedyLPT(MultiHistogram hist, ui32 partitionsNum) {
    auto& buckets = hist.buckets;
    auto result = BinArray(partitionsNum);
    std::sort(buckets.begin(), buckets.end(), [](const MultiBucket& lhs, const MultiBucket& rhs) { return lhs.count > rhs.count; });

    for (auto& bucket : buckets) {
        Bin& min = result[minBin(result)];
        min.addBucket(&bucket);
    }

    return result;
}

BinArray simpleBalance(const MultiHistogram& hist, ui32 partitionsNum) {

    std::vector<std::vector<size_t>> distribution(partitionsNum);
    size_t idx = 0;
    for (const auto& mBucket : hist.buckets) {
        distribution[mBucket.maxCountIdx].push_back(idx);
        idx++;
    }

    // simple out 
    BinArray result;
    for (const auto& ids : distribution) {
        Bin bin;
        for (auto id : ids) {
            bin.buckets.push_back(&hist.buckets[id]);
            bin.sum += hist.buckets[id].count;
        }
        
        result.emplace_back(std::move(bin));
    }
    return result;
}

static i64 maxMultPart(const std::vector<MultiBucket>& buckets, int source, const std::vector<bool>& used) {
    i64 maxIdx = -1;
    i64 maxCount = -1;
    for (size_t i = 0; i < buckets.size(); i++) {
        if (used[i]) {
            continue;
        }
        if (!buckets[i].buckets.contains(source)) {
            continue;
        }
        auto& b = buckets[i].buckets.at(source);
        if (maxCount < static_cast<i64>(b.count)) {
            maxCount = b.count;
            maxIdx = i;
        }
    }
    return maxIdx;
}

static i64 minBucket(const std::vector<MultiBucket>& buckets, const std::vector<bool>& used) {
    i64 minIdx = -1;
    i64 minCount = std::numeric_limits<i64>::max();
    for (size_t i = 0; i < buckets.size(); i++) {
        if (used[i]) {
            continue;
        }
        if (static_cast<i64>(buckets[i].count) < minCount) {
            minCount = buckets[i].count;
            minIdx = i;
        }
    }
    assert(minIdx != -1);
    return minIdx;
}

BinArray greedyBalance(MultiHistogram hist, int partitionsNum) {
    auto& buckets = hist.buckets;
    std::vector<bool> used(buckets.size(), false);
    BinArray result(partitionsNum);
    size_t sz = buckets.size();
    for (size_t i = 0; i < sz; i++) {
        size_t minPart = minBin(result);
        i64 maxMultiPart = maxMultPart(buckets, minPart, used);
        if (maxMultiPart != -1) {
            used[maxMultiPart] = true;
            result[minPart].addBucket(&buckets[maxMultiPart]);
        } else {
            // fallback to minimal total bucket
            i64 min = minBucket(buckets, used);
            used[min] = true;
            result[minPart].addBucket(&buckets[min]);
        }
    }
    return result;
}

struct q_comparer {

    int idx;

    bool operator()(const std::pair<MultiBucket*, bool>* lhs, const std::pair<MultiBucket*, bool>* rhs) {
        ui64 left = 0, right = 0;
        if (lhs->first->buckets.contains(idx)) {
            left = lhs->first->buckets.at(idx).count;
        }
        if (rhs->first->buckets.contains(idx)) {
            right = rhs->first->buckets.at(idx).count;
        }
        return left < right;
    }
};


BinArray multiBalance(MultiHistogram hist, int partitionsNum, double limitCoef) {
    using q_element = std::pair<MultiBucket*, bool>;
    using q_queue = std::priority_queue<q_element*, std::vector<q_element*>, q_comparer>;

    std::vector<q_element> q_elements;
    q_elements.reserve(hist.buckets.size());
    ui64 bucketsTotal = 0;
    for (auto& b : hist.buckets) {
        q_elements.emplace_back(&b, false);
        bucketsTotal += b.count;
    }
    ui64 bucketLimit = std::lround((bucketsTotal * 1. / partitionsNum) * limitCoef);
    // sorted parts of multibuckets
    std::vector<q_queue> multiQueues;
    for (int i = 0; i < partitionsNum; i++) {
        q_queue q(q_comparer{i});
        for (auto& b : q_elements) {
            if (b.first->buckets.contains(i)) {
                q.push(&b);
            }
        }
        multiQueues.push_back(std::move(q));
    }

    BinArray result(partitionsNum);
    size_t sz = q_elements.size();

    std::vector<q_element*> remainder;
    
    for (size_t i = 0; i < sz; i++) {
        int max = -1;

        // find max local part
        for (int b = 0; b < partitionsNum; b++) {
            // remove deleted elements
            while (!multiQueues[b].empty() && multiQueues[b].top()->second) {
                multiQueues[b].pop();
            }
            if (multiQueues[b].empty()) {
                continue;
            }

            if (max == -1 || multiQueues[b].top()->first->buckets.at(b).count > multiQueues[max].top()->first->buckets.at(max).count) {
                max = b;
            }
        }

        q_element* maxEl = multiQueues[max].top();
        ui64 count = maxEl->first->count;
        if (result[max].sum + count <= bucketLimit) {
            result[max].addBucket(maxEl->first);
            multiQueues[max].pop();
            maxEl->second = true;
        } else {
            // send him to Detroit
            remainder.push_back(maxEl);
            multiQueues[max].pop();
        }
    }

    if (!remainder.empty()) {
        remainder.erase(std::remove_if(remainder.begin(), remainder.end(), [](q_element* el) { return el->second; }));
        std::sort(remainder.begin(), remainder.end(), [](const q_element* lhs, const q_element* rhs) { return lhs->first->count > rhs->first->count; });
        for (auto& q_el : remainder) {
            Bin& min = result[minBin(result)];
            min.addBucket(q_el->first);
        }
    }

    return result;
}


BSPartitioner::BSPartitioner(const BinArray& partitioning)
{
    int part = 0;
    for (const auto& arr : partitioning) {
        for (const auto* bucket : arr.buckets) {
            sortedBuckets.push_back(std::make_pair(bucket, part));
            //std::cerr << "bucket -> part: [" << bucket->left << ";" << bucket->right << "] -> " << part << "\n";
        }
        part++;
    }
    std::sort(sortedBuckets.begin(), sortedBuckets.end(), [](mapping& lhs, mapping& rhs) {return lhs.first->left < rhs.first->left;});
}

int BSPartitioner::decide_partition(i64 key) const {
    // go to first partition
    // TODO: extrapolate
    if (key < sortedBuckets[0].first->left) {
        return sortedBuckets[0].second;
    }
    // go to last partition
    // TODO: extrapolate
    if (key >= sortedBuckets[sortedBuckets.size() - 1].first->left) {
        return sortedBuckets[sortedBuckets.size() - 1].second;
    }
    size_t left = 0;
    size_t right = sortedBuckets.size();
    while (right - left > 1) {
        size_t mid = left + (right - left) / 2;
        i64 val = sortedBuckets[mid].first->left;
        if (key < val) {
            right = mid;
        } else {
            left = mid;
        }
    }


    if (sortedBuckets[left].first->right <= key) {
        // hole between buckets
        // TODO: extrapolate
    }
    return sortedBuckets.at(left).second;
}


