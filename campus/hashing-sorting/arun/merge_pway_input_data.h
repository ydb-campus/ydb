#pragma once

#include "campus/hashing-sorting/arun/spilling_mem.h"
#include <algorithm>
constexpr ui32 slotSize = 8;

template <ui32 keyCount>
struct TInputData {
    TInputData() {
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
        EndOfBuffer = Buffer;
    }

    bool Next(TSpilling& sp) {
        while (true) {
            while (Record < EndOfBuffer && Record[slotSize - 1] == 0) {
                Record += slotSize;
            }
            // either record.count > 0 or valid record is not found inside this

            if (Record < EndOfBuffer) { // if record is valid
                return true;
            }

            if (Offset == Size) { // Block doesn't have data left
                return false;
            }

            Count = std::min(BufferSize, Size - Offset);
            sp.Load(Block, Offset * slotSize * 8, Buffer, Count * slotSize * 8);
            Offset += Count;
            Record = Buffer;
            EndOfBuffer = Buffer + Count * slotSize;
        }
    }

    inline ui32 Compare(ui64*& record, ui64& count, ui32 mask, ui32 ownMask) const {
        // count is aggregation value.
        // function sets record and count if this->Record is smaller or equal that *record;
        if (Record >= EndOfBuffer) {
            return mask;
        }
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
                if (record[1] == Record[1] && record[2] == Record[2] && record[3] == Record[3]) {
                    count += Record[slotSize - 1];
                    return (mask | ownMask);
                }
            } else if constexpr (keyCount == 4) {
                if (record[1] == Record[1] && record[2] == Record[2] && record[3] == Record[3] && record[4] == Record[4]) {
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
