
#include "parquet/arrow/writer.h"
#include "arrow/util/type_fwd.h"

#include <arrow/array/builder_primitive.h>
#include <arrow/table.h>
#include <arrow/io/api.h>

#include <fstream>
#include <string>
#include <random>
#include <iostream>

const int64_t I_MIN = 0;
const int64_t I_MAX = 1'000'000;


std::shared_ptr<arrow::Array> doColumnUniform(std::string filename, int64_t min, int64_t max, size_t count) {

    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::cerr << "uniforma l" << std::max(min, I_MIN) << " " << std::min(max, I_MAX) << "\n";
    std::uniform_int_distribution<int64_t> distrib(std::max(min, I_MIN), std::min(max, I_MAX));

    arrow::Int64Builder builder;

    std::ofstream out;
    out.open(filename + ".csv");

    for (size_t i = 0; i < count; i++) {
        auto value = distrib(gen);
        out << value << "\n";
        if (!builder.Append(value).ok()) {
            assert(0);
        }
    }
    
    return builder.Finish().ValueOrDie();
}

std::shared_ptr<arrow::Array> doColumnNormalLocal(std::string filename, double mean) {

    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::normal_distribution distrib{mean, 100000.0};

    arrow::Int64Builder builder;

    std::ofstream out;
    out.open(filename + ".csv");

    for (size_t i = 0; i < 1000000; i++) {
        int64_t value = std::lround(distrib(gen));
        if (value < 0) {
            value = -value;
        }
        if (value > I_MAX) {
            value = I_MAX - (value - I_MAX);
        }
        out << value << "\n";
        if (!builder.Append(value).ok()) {
            assert(0);
        }
    }
    
    return builder.Finish().ValueOrDie();
}


arrow::Status generate(std::string filename, double mean, int64_t min, int64_t max, size_t count) {
    (void)mean;
    using parquet::ArrowWriterProperties;
    using parquet::WriterProperties;
    
    //auto t = GetTable();
    //ARROW_ASSIGN_OR_RAISE(std::shared_ptr<arrow::Table> table, GetTable());
    //auto type = std::make_shared<arrow::DataType>(arrow::Type::type::INT64);

    auto field = std::make_shared<arrow::Field>("test_name", arrow::int64(), false);
    auto chunked_array = std::make_shared<arrow::ChunkedArray>(doColumnUniform(filename, min, max, count));
    auto schema = std::make_shared<arrow::Schema>(arrow::FieldVector{field});
    auto table = arrow::Table::Make(schema, {chunked_array}, chunked_array->length());
    //auto table = arrow::Table::FromChunkedStructArray(chunked_array).ValueOrDie(); //std::make_shared<arrow::Table>();
    //ARROW_ASSIGN_OR_RAISE(table, table->AddColumn(0, field, chunked_array));

    // Choose compression
    std::shared_ptr<WriterProperties> props =
        WriterProperties::Builder().compression(arrow::Compression::SNAPPY)->build();

    // Opt to store Arrow schema for easier reads back into Arrow
    std::shared_ptr<ArrowWriterProperties> arrow_props =
        ArrowWriterProperties::Builder().store_schema()->build();

    std::shared_ptr<arrow::io::FileOutputStream> outfile;
    ARROW_ASSIGN_OR_RAISE(outfile, arrow::io::FileOutputStream::Open(filename + ".parquet"));

    
    ARROW_RETURN_NOT_OK(parquet::arrow::WriteTable(*table.get(),
                                                arrow::default_memory_pool(), outfile,
                                                /*chunk_size=*/100'000, props, arrow_props));

    return arrow::Status::OK();
}



int main() {
    double mean = 50000;

    (void)generate("single_overloaded_3", 250000, 190000, 310000, 3000000);

    for (int i = 0; i < 10; i++) {
        std::cerr << mean - 60000 << " " << mean + 60000 << "\n";
        if (i != 3) {
        std::cerr << "generate " << i << "\n";
        (void)generate("single_overloaded_" + std::to_string(i), mean, mean - 60000, mean + 60000, 1000000);
        }
        mean += 100000;
    }
    return 0;
}
