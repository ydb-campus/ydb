#pragma once

#include <vector>

#include <arrow/io/api.h>
#include <arrow/table.h>
#include <arrow/array.h>
#include <parquet/arrow/reader.h>

template<class T, class TArrowArray>
std::vector<T> read_dataset(const std::string& filepath, std::string colName) {
    arrow::MemoryPool* pool = arrow::default_memory_pool();
    std::shared_ptr<arrow::io::RandomAccessFile> input;
    input = arrow::io::ReadableFile::Open(filepath).ValueUnsafe();
    //ARROW_ASSIGN_OR_RAISE(input, arrow::io::ReadableFile::Open(filepath));

    std::unique_ptr<parquet::arrow::FileReader> arrow_reader;
    auto st = parquet::arrow::OpenFile(input, pool, &arrow_reader);
    //ARROW_RETURN_NOT_OK(parquet::arrow::OpenFile(input, pool, &arrow_reader));


    std::shared_ptr<arrow::Table> table;
    auto status = arrow_reader->ReadTable(&table);
    //ARROW_RETURN_NOT_OK(arrow_reader->ReadTable(&table));

    std::vector<T> cast;
    cast.reserve(table->num_rows());
    for (auto chunk : table->GetColumnByName(colName)->chunks()) {
        auto data = std::static_pointer_cast<TArrowArray>(chunk);
        
        for (auto it = data->begin(); it != data->end(); it++) {
            cast.emplace_back((*it).value());
        }
    }
    
    return cast;
}

