//
// Created by nfrmtk on 6/18/25.
//

#ifndef MRGE_REFERENCE_HPP
#define MRGE_REFERENCE_HPP

#include <campus/hashing-sorting/arun/spilling_mem.h>

#include <deque>
#include <iosfwd>
#include <campus/hashing-sorting/arun/pmerge/common/print.hpp>
#include <string_view>

#include "campus/hashing-sorting/arun/pmerge/ydb/spilling_blocks_writer.hpp"

constexpr ui32 slotSize = 8;

template <ui32 keyCount>
struct TInputData {
    TInputData()
        : Block(nullptr, 0, 0)
    {
    }

    TInputData(TSpillingBlock block, ui64* buffer, ui64 bufferSize) {
        Init(block, buffer, bufferSize);
    }

    void Init(TSpillingBlock block, ui64* buffer, ui64 bufferSize) {
        Block = block;
        Offset = 0;
        Count = 0;
        Size = Block.BlockSize / (slotSize * 8);
        Buffer = buffer;
        BufferSize = bufferSize;
        Record = Buffer;
        pmerge::println("[[reference]] Init Record[0]: {}", Record[0]);
        EndOfBuffer = Buffer;
    }

    bool Next(TSpilling& sp) {
        while (true) {
            while (Record < EndOfBuffer && Record[slotSize - 1] == 0) {
                Record += slotSize;
            }

            if (Record < EndOfBuffer) {
                return true;
            }

            if (Offset == Size) {
                return false;
            }

            Count = std::min(BufferSize, Size - Offset);
            sp.Load(Block, Offset * slotSize * 8, Buffer, Count * slotSize * 8);
            Offset += Count;
            Record = Buffer;
            EndOfBuffer = Buffer + Count * slotSize;
        }
    }

    inline ui32 Compare(ui64*& record, ui64& count, ui32 mask, ui32 ownMask) {
        if (Record >= EndOfBuffer) {
            return mask;
        }
        pmerge::println("[[reference]] Record[0]: {}", Record[0]);
        if (mask == 0) {
            count = Record[slotSize - 1];
            record = Record;
            return ownMask;
        } else {
            if (record[0] < Record[0]) {
                return mask;
            } else if (record[0] > Record[0]) {
                count = Record[slotSize - 1];
                record = Record;
                return ownMask;
            }

            // record[0] == Record[0]

            if constexpr (keyCount == 1) {
                if (record[1] == Record[1]) {
                    count += Record[slotSize - 1];
                    return (mask | ownMask);
                }
            } else if constexpr (keyCount == 2) {
                if (record[1] == Record[1] && record[2] == Record[2]) {
                    count += Record[slotSize - 1];
                    return (mask | ownMask);
                }
            } else if constexpr (keyCount == 3) {
                if (record[1] == Record[1] && record[2] == Record[2] &&
                    record[3] == Record[3]) {
                    count += Record[slotSize - 1];
                    return (mask | ownMask);
                }
            } else if constexpr (keyCount == 4) {
                if (record[1] == Record[1] && record[2] == Record[2] &&
                    record[3] == Record[3] && record[4] == Record[4]) {
                    count += Record[slotSize - 1];
                    return (mask | ownMask);
                }
            }

            for (ui32 i = 1; i < keyCount + 1; i++) {
                if (record[i] < Record[i]) {
                    return mask;
                } else if (record[i] > Record[i]) {
                    count = Record[slotSize - 1];
                    record = Record;
                    return ownMask;
                }
            }

            count += Record[slotSize - 1];
            return (mask | ownMask);
        }
    }

    inline void IncIfUse(ui32 mask, ui32 ownMask) {
        Record += ((mask & ownMask) != 0) * slotSize;
    }

    TSpillingBlock Block;
    ui64* Buffer;
    ui64* Record;
    ui64* EndOfBuffer;
    ui64 BufferSize;
    ui64 Offset;
    ui64 Count;
    ui64 Size;
};
namespace ydb_reference {
    template <bool finalize, ui32 keyCount, ui32 p>
    ui32 merge2pway(ui64* partBuffer, ui32 partBufferSize, TSpilling& sp,
                    std::deque<TSpillingBlock>& spills) {
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

            ui64 count = 0;
            ui64* record = nullptr;

            ui32 use = 0;
            for (ui32 i = 0; i < n; i++) {
                pmerge::println("[[reference]] use: 0x{:b}\n", use);
                use = data[i].Compare(record, count, use, 1 << i);
            }

            if constexpr (finalize) {
                auto recordm = mergeBuffer + indexm * (keyCount + 2);

                std::copy(record, record + 1 + keyCount, recordm);
                recordm[keyCount + 1] = count;
            } else {
                pmerge::println("[[reference]] write hash to memory: {}\n", record[0]);
                auto recordm = mergeBuffer + indexm * slotSize;
                std::copy(record, record + 1 + keyCount, recordm);
                recordm[slotSize - 1] = count;
            }

            for (ui32 i = 0; i < n; i++) {
                data[i].IncIfUse(use, 1 << i);
            }
            if ((result + indexm) % kCurrentIntervalSlots == 0) {
                static WriteInterval reference_write_interval;
                reference_write_interval.WriteHappend(kCurrentIntervalSlots);
            }

            if (++indexm == mergeBufferSize) {
                if constexpr (finalize) {
                    assert(false);
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
            assert(false);
        } else {
            spills.push_back(sp.Append(merged, mergeBuffer, indexm * slotSize * 8));
        }
        result += indexm;

        return result;
    }
} // namespace ydb_reference
#endif // MRGE_REFERENCE_HPP
