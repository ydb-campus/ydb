#include "merger.h"

#include <cassert>
#include <unordered_map>
#include <queue>

struct pair_hash
{
    std::size_t operator() (const std::pair<ui32, ui32> &pair) const
    {
        return pair.first << 32 + pair.second;
    }
};

struct Border {
    i64 value;
    bool open;
    ui32 sourceIndex;
    ui32 bucketIndex;
    const Bucket* source;
};

MultiBucket::MultiBucket(i64 left, i64 right)
    : left(left), right(right), count(0), maxCountIdx(-1)
{ }

void MultiBucket::addBucket(Bucket bucket, int source) {
    assert(bucket.left == left);
    assert(bucket.right == right);
    //assert(!buckets.contains(source));
    if (buckets.contains(source)) {
        std::cerr << " FATAL: add range " << bucket.left << ":" << bucket.right << " from " << source << "\n";
        assert(0);
    }
    count += bucket.count;
    if (maxCountIdx == -1 || buckets[maxCountIdx].count < bucket.count) {
        maxCountIdx = source;
    }
    buckets.emplace(source, std::move(bucket));
}

MultiHistogram multiMerge(const std::vector<Histogram>& sources) {
    using bucketKey = std::pair<ui32, ui32>;
    using bucketDistribution = std::pair<const Bucket*, ui64>;

    MultiHistogram result;

    std::unordered_map<bucketKey, bucketDistribution, pair_hash> currentBuckets;
    auto cmp = [](const Border& left, const Border& right) {
        if (left.value == right.value) {
            if (left.open && right.open) return false;
            return left.open;   // closing borders first
        }
        return left.value > right.value;
    };
    std::priority_queue<Border, std::vector<Border>, decltype(cmp)> pq;
    int sourceI = 0;
    for (const auto& hist : sources) {
        int bucketI = 0;
        for (auto& bucket : hist.buckets) {
            pq.emplace(bucket.left, true, sourceI, bucketI, &bucket);
            pq.emplace(bucket.right, false, sourceI, bucketI, &bucket);
            bucketI++;
        }
        sourceI++;
    }

    Border currBorder = pq.top();
    pq.pop();
    //YQL_ENSURE(currBorder.open);

    i64 currLine = currBorder.value;
    currentBuckets.emplace(std::make_pair(currBorder.sourceIndex, currBorder.bucketIndex), std::make_pair(currBorder.source, currBorder.source->count));
    while (!pq.empty() && pq.top().value == currLine) {
        //YQL_ENSURE(pq.top().open); // maybe just skip
        Border border = pq.top();
        pq.pop();
        currentBuckets.emplace(std::make_pair(border.sourceIndex, border.bucketIndex), std::make_pair(border.source, border.source->count));
    }

    while (!pq.empty()) {
        // nextline should not be equal to curr, aggregate everything
        i64 nextLine = pq.top().value;

        MultiBucket b(currLine, nextLine);
        // calculate new interval for current buckets
        ui64 count = 0;
        for (auto& [k, bucketDist] : currentBuckets) {
            ui64 len = bucketDist.first->right - bucketDist.first->left;
            //if (len == 0) continue;
            ui64 range = nextLine - currLine;
            ui64 fraction = bucketDist.first->count * range / len; // fraction of points in range, approx
            bucketDist.second -= fraction; // decrease remaining count
            if (bucketDist.first->right == nextLine) { // closing bucket, dump remaining count
                fraction += bucketDist.second;
                bucketDist.second = 0;
            }
            count += fraction;
            
            b.addBucket({currLine, nextLine, fraction}, k.first);
        }

        // get all closing borders on the line, remove from current
        while(!pq.empty() && pq.top().value == nextLine && !pq.top().open) {
            Border closing = pq.top();
            auto bucketKey = std::make_pair(closing.sourceIndex, closing.bucketIndex);
            currentBuckets.erase(bucketKey);
            pq.pop();
        }

        b.count = count;
        if (count > 0) {
            result.buckets.emplace_back(std::move(b));
        }

        // get all opening borders on the line, add to curr
        while(!pq.empty() && pq.top().value == nextLine && pq.top().open) {
            Border opening = pq.top();
            currentBuckets.emplace(std::make_pair(opening.sourceIndex, opening.bucketIndex), std::make_pair(opening.source, opening.source->count));
            pq.pop();
        }

        currLine = nextLine;
    }

    return result;
}