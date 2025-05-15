#include "histogram.h"

#include <algorithm>
#include <cassert>
#include <queue>
#include <unordered_map>

#define DEBUG

// static HistogramDistribution determineDist(size_t asc, size_t desc, size_t even) {
//     if (desc == 0) {
//         return HistogramDistribution::Ascending;
//     }
//     if (asc == 0) {
//         return HistogramDistribution::Descending;
//     }
// }


Histogram::Histogram(std::vector<Bucket> buckets)
    : buckets(std::move(buckets))
{ }



Histogram buildOrdered(const std::vector<i64>& c_data, int bucketsCount) {
    // size_t asc = 0;
    // size_t desc = 0;
    // size_t even = 0;
    // {
    //     size_t interval_size = data.size() / 10;
    //     auto left = data[0];
    //     auto right = data[0];
    //     for (size_t i = 1; i < data.size(); i++) {
    //         if (data[i] > data[i-1]) {
    //             asc++;
    //         } else if (data[i] < data[i-1]) {
    //             desc++;
    //         } else {
    //             even++;
    //         }
    //         /// min max
    //         //if (data)

    //         if (i % interval_size == 0) {

    //         }
    //     }
    // }
    std::vector<i64> data = c_data;


    std::vector<Bucket> buckets;
    buckets.reserve(bucketsCount);
    std::sort(data.begin(), data.end());
    size_t approx = data.size() / bucketsCount;

    i64 prev = data[0];
    i64 left = 0;
    i32 partNum = 1;
    for (size_t i = 0; i < data.size(); i++) {
        auto value = data[i];
        if (i - left >= approx &&  value != prev) {
            if (partNum == bucketsCount) {
                break;
            }
            buckets.emplace_back(data[left], data[i], i - left );
            left = i;
            partNum++;
        }
        prev = data[i];
    }
    
    buckets.emplace_back(data[left], data[data.size() - 1] + 1, data.size() - left);
    return Histogram(std::move(buckets));
}

struct Marker {
    double observation;
    size_t position;
};

template <typename T> 
int sign(T val) {
    return (T(0) < val) - (val < T(0));
}

Histogram buildP2(const std::vector<int64_t>& data, size_t bucketsCount) {
    size_t markersSz = bucketsCount + 1;
    assert(data.size() > markersSz);
    std::vector<int64_t> initial(markersSz);
    size_t minIdx = 0;
    size_t maxIdx = markersSz - 1;

    std::copy(data.begin(), data.begin() + markersSz, initial.begin());
    std::sort(initial.begin(), initial.end());

    std::vector<Marker> markers(markersSz);

    for (size_t i = 0; i < markersSz; i++) {
        markers[i] = {static_cast<double>(initial[i]), i+1};
    }

    for (size_t i = markersSz; i < data.size(); i++) {
        int64_t value = data[i];
        size_t k = 0;

        // find position to fit
        if (value < markers[minIdx].observation) { // new min
            markers[minIdx].observation = value;
            k = 0;
        } else if (markers[maxIdx].observation < value) { // new max
            markers[maxIdx].observation = value;
            k = maxIdx - 1;
        } else if (markers[maxIdx - 1].observation <= value && value <= markers[maxIdx].observation) { // premax, corner case
            k = maxIdx - 1;
        } else {
            for (size_t j = minIdx; j < maxIdx; j++) {
                if (markers[j].observation <= value && value < markers[j + 1].observation) {
                    k = j;
                    break;
                }
            }
        }

        // increment pos k+1..b+1
        for (size_t i = k + 1; i <= maxIdx; i++) {
            markers[i].position++;
        }

        // process values, adjust markers
        for (size_t i = minIdx + 1; i < maxIdx; i++) {
            // n' <- 1 + (i - 1)(n - 1)/b
            // adjucted to 0-based index
            double desiredPos = 1 + static_cast<double>(i)*(data.size() - 1) / maxIdx;
            double delta = desiredPos - markers[i].position;

            if (delta >= 1 && (markers[i+1].position - markers[i].position) > 1 
                || delta <= -1 && (static_cast<int64_t>(markers[i-1].position) - static_cast<int64_t>(markers[i].position)) < -1) {
                
                int d = sign(delta);

                size_t posPrev = markers[i-1].position;
                size_t posNext = markers[i+1].position;
                double qPrev = markers[i-1].observation;
                double qNext = markers[i+1].observation;
                
                // parabolic formula
                double adjustedQ = markers[i].observation;
                adjustedQ += static_cast<double>(d) / (posNext - posPrev) * (
                    (markers[i].position - posPrev + d) * (qNext - markers[i].observation) / (posNext - markers[i].position)
                    + (posNext - markers[i].position - d) * (markers[i].observation - qPrev) / (markers[i].position - posPrev)
                );
                if (markers[i-1].observation < adjustedQ && adjustedQ < markers[i+1].observation) {
                    markers[i].observation = adjustedQ;
                } else {
                    // linear formula
                    markers[i].observation = markers[i].observation + static_cast<double>(d) * (markers[i + d].observation - markers[i].observation) / (markers[i + d].position - markers[i].position); 
                }

                markers[i].position += d;
            }
        }
    }

    std::vector<Bucket> buckets;
    buckets.reserve(bucketsCount);

    for (size_t i = minIdx + 1; i <= maxIdx; i++) {
        int64_t left = std::round(markers[i-1].observation);
        int64_t right = std::round(markers[i].observation);
        if (i == maxIdx) {
            right += 1; // last exclusive
        }
        size_t cumDelta = markers[i].position - markers[i-1].position;
        buckets.emplace_back(left, right, cumDelta);
    }

    return Histogram(std::move(buckets));
}

