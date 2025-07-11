#include "aggr.h"
#include "aggr_rh_ht.h"
#include "campus/hashing-sorting/arun/spilling_mem.h"
#include "merge_pway_input_data.h"
#include <util/stream/format.h>
#include <campus/hashing-sorting/arun/pmerge/ydb/merge_spilling_blocks.hpp>
#include <vector>
#include <deque>

namespace reference{
template <bool finalize, ui32 keyCount, ui32 p>
ui32 merge2pway(ui64 * partBuffer, ui32 partBufferSize, TSpilling& sp, std::deque<TSpillingBlock>& spills) {
    static_assert(!finalize, "writing in final format is not implemented");
    constexpr ui32 n = 1ul << p;
    auto inputBufferSize = partBufferSize >> (p + 1);
    auto mergeBufferSize = partBufferSize >> 1;

    TInputData<keyCount> data[n];
    auto buffer = partBuffer;
    for (ui32 i = 0; i < n; i++) {
        data[i].Init(spills.front(), buffer, inputBufferSize);
        spills.pop_front();
        buffer += inputBufferSize * slotSize;
    }

    auto mergeBuffer = buffer;
    auto merged = sp.Empty(0);
    ui32 indexm = 0;
    ui32 result = 0;

    while (true) {

        auto notEmpty = false;
        for (ui32 i = 0; i < n; i++) {
            notEmpty |= data[i].Next(sp);
        }

        if (!notEmpty) {
            break;
        }


        // REWRITE THIS TO PRODUCE USE USE UI32(BASICALLY "WHICH ARRAYS HAVE CURRENT MINIMUM").
        // THIS SHOULD BE CONST LOSER-TREE WISE.
        // ALSO PRODUCE RECORD AND COUNT. RECORD IS (HASH, KEY). COUNT IS SUMM OF COUNTS OF ALL ELEMENTS(WHATEVER)
        ui64 count = 0;
        ui64 * record = nullptr;

        ui32 use = 0;
        for (ui32 i = 0; i < n; i++) {
            use = data[i].Compare(record, count, use, 1 << i);
        }
        // END REWRITE

        if constexpr (finalize) {
            auto recordm = mergeBuffer + indexm * (keyCount + 2);
            std::copy(record, record + 1 + keyCount, recordm);
            recordm[keyCount + 1] = count;
        } else {
            auto recordm = mergeBuffer + indexm * slotSize;
            std::copy(record, record + 1 + keyCount, recordm);
            recordm[slotSize - 1] = count;
        }

        // THIS SHOULD CHANGE TOO. NOT SURE YET.
        // I HAVE LOSER TREE. THATS IT.
        // NOW I DO NEED BUFFERING.
        // I NEED TO HAVE AN ARRAY OF (USE, COUNT, RECORD) - GENERAL CASE,
        // MOST OF THE TIME THIS ARRAY WILL HAVE SIZE 1
        // DO I NEED USE ACTUALLY?
        // PROBABLY NOT. Compare WILL JUST POP THE SMALLEST ELEMENT(SO IT WILL NOT BE CONST).
        // IF THERE IS NOTHING TO POP, THAN I WILL GET BATCH FROM LOSER TREE. 
        // WITH THIS BATCH, I WILL LOAD NECESSARY THINGS FROM RAM (USE-LIKE THING WILL BE HANDY HERE) 


        for (ui32 i = 0; i < n; i++) {
            data[i].IncIfUse(use, 1 << i);
        }

        if (Y_UNLIKELY(++indexm == mergeBufferSize)) {
            if constexpr (finalize) {
            } else {
                merged = sp.Append(merged, mergeBuffer, indexm * slotSize * 8);
            }
            result += indexm;
            indexm = 0;
        }
    }

    for (ui32 i = 0; i < n; i++) {
        sp.Delete(data[i].Block);
    }

    if constexpr (finalize) {
    } else {
        spills.push_back(sp.Append(merged, mergeBuffer, indexm * slotSize * 8));
    }
    result += indexm;

    return result;
}
} // namespace reference 

template <ui32 keyCount>
void aggr_external_merge(TFileInput& fi, ui64 rowCount, ui64 cardinality, ui16 hashBits, ui16 fillRatio, ui32 partBufferSize, bool simd_approach) {

    Cout << "Robin Hood HT, aggregation in External Memory with Merge combine" << Endl;

    if (hashBits == 0) {
        hashBits = round_to_nearest_power_of_two(cardinality);
    }

    TSpilling sp(8 * 1024 * 1024);
    std::deque<TSpillingBlock> spills;

    ui64 swaps = 0;
    ui64 probes = 0;
    ui64 sum = 0;
    ui64 total1 = 0;
    ui64 total2 = 0;
    ui64 overlaps = 0;

  {
    // 1. intermediate aggr

    auto slotCount = 1ull << hashBits;
    assert(fillRatio <= 100);
    auto fillCount = slotCount * fillRatio / 100;

    ui64 * buffer = new ui64[slotCount * slotSize];
    TRHHashTable ht(buffer, hashBits, keyCount, slotSize);

    Cout << "HT1 Size: " << ht.SlotCount << ", Rows Width: " << slotSize * 8 << Endl;
    Cout << "HT1 Mem: " << slotCount * slotSize * 8 << Endl;

    total1 = slotCount * slotSize * 8;

    Cout << "Total MEM 1: " << total1 << Endl;

    ui64 n = 1024;

    ui64 * readBuffer = new ui64[8 * n];

    while (rowCount) {
        ui64 d = rowCount > n ? n : rowCount;
        rowCount -= d;
        fi.Load(readBuffer, d * 8 * 8);
        auto record = readBuffer;
        for (ui64 i = 0; i < d; i++) {
            auto hash = hash_keys(record, keyCount);
            auto result = ht.Insert(hash, record);
            assert(result);
            Y_UNUSED(result);
            record += 8;

            if (ht.Count >= fillCount || (rowCount == 0 && i == d - 1)) {
                swaps += ht.CollisionSwapCount;
                probes += ht.CollisionProbes;

                overlaps += (ht.MinHashIndex > 0);
                auto b = sp.Save(buffer + ht.MinHashIndex * slotSize, (slotCount - ht.MinHashIndex) * slotSize * 8, 0);
                spills.push_back(sp.Append(b, buffer, ht.MinHashIndex * slotSize * 8));

                ht.Reset();
            }
        }
        sum += d;
    }

    delete[] readBuffer;
    delete[] buffer;
  }

    Cout << "Total record processed: " << sum << Endl;
    Cout << "Overlaps: " << overlaps << Endl;
    Cout << "Collision Swaps: " << swaps << Endl;
    Cout << "Extra probes: " << probes << Endl;
    if (sum) {
        Cout << "Probe length: " << (sum + probes) / double(sum) << Endl;
    }

    ui64 nw = 0;

  {
    // 2. final aggr

    ui64 * partBuffer = new ui64[partBufferSize * slotSize];
    assert(partBufferSize >= 4);
    total2 = partBufferSize * slotSize * 8;

    Cout << "Buffers 2: " << total2 << Endl;
    Cout << "Total MEM 2: " << total2 << Endl;

    while(!spills.empty()) {

        switch (std::min<ui32>(spills.size(), partBufferSize >> 1)) {
        case 0:
        case 1:
        {
            break;
        }
        case 2: case 3:
        {
            if (simd_approach) {
                nw += pmerge::ydb::merge2pway<false, keyCount, 1>(partBuffer, partBufferSize,  sp, spills);
            } else {
                nw += reference::merge2pway<false, keyCount, 1>(partBuffer, partBufferSize,  sp, spills);
            }
            break;
        }
        case 4: case 5: case 6: case 7:
        {
            if (simd_approach) {
                nw += pmerge::ydb::merge2pway<false, keyCount, 2>(partBuffer, partBufferSize,  sp, spills);
            } else {
                nw += reference::merge2pway<false, keyCount, 2>(partBuffer, partBufferSize,  sp, spills);
            }
            break;
        }
        case 8: case 9: case 10: case 11:
        case 12: case 13: case 14: case 15:
        {
            if (simd_approach) {
                nw += pmerge::ydb::merge2pway<false, keyCount, 3>(partBuffer, partBufferSize,  sp, spills);
            } else {
                nw += reference::merge2pway<false, keyCount, 3>(partBuffer, partBufferSize,  sp, spills);
            }
            break;
        }
        default:
        {
            if (simd_approach) {
                nw += pmerge::ydb::merge2pway<false, keyCount, 4>(partBuffer, partBufferSize,  sp, spills);
            } else {
                nw += reference::merge2pway<false, keyCount, 4>(partBuffer, partBufferSize,  sp, spills);
            }
            break;
        }
        }
    }

    delete[] partBuffer;
  }

    Cout << "Unique keys sets: " << nw << Endl;
    Cout << "Spilling WriteChunkCount: " << sp.WriteChunkCount << Endl;
    Cout << "Spilling ReadChunkCount: " << sp.ReadChunkCount << Endl;
    Cout << "Spilling MaxChunkCount: " << sp.MaxChunkCount << Endl;
    Cout << "Grand total MEM: " << std::max(total1, total2) << Endl;
}

template
void aggr_external_merge<1>(TFileInput& fi, ui64 rowCount, ui64 cardinality, ui16 hashBits, ui16 fillRatio, ui32 partBufferSize, bool simd_approach );

template
void aggr_external_merge<2>(TFileInput& fi, ui64 rowCount, ui64 cardinality, ui16 hashBits, ui16 fillRatio, ui32 partBufferSize, bool simd_approach);

template
void aggr_external_merge<3>(TFileInput& fi, ui64 rowCount, ui64 cardinality, ui16 hashBits, ui16 fillRatio, ui32 partBufferSize, bool simd_approach);

template
void aggr_external_merge<4>(TFileInput& fi, ui64 rowCount, ui64 cardinality, ui16 hashBits, ui16 fillRatio, ui32 partBufferSize, bool simd_approach);
