#include <util/system/types.h>

// #include <arrow/io/api.h>
// #include <arrow/table.h>
// #include <arrow/array.h>
// #include <parquet/arrow/reader.h>
#include <arrow/type_fwd.h>


#include "histogram.h"
#include "arrow_reader.h"

// Histogram<Bucket> read_dataset(const std::string& filepath, i32 partitions) {
//     std::cout << "Read " << filepath << "\n";
//     arrow::MemoryPool* pool = arrow::default_memory_pool();
//     std::shared_ptr<arrow::io::RandomAccessFile> input;
//     input = arrow::io::ReadableFile::Open(filepath).ValueUnsafe();
//     //ARROW_ASSIGN_OR_RAISE(input, arrow::io::ReadableFile::Open(filepath));

//     std::unique_ptr<parquet::arrow::FileReader> arrow_reader;
//     auto st = parquet::arrow::OpenFile(input, pool, &arrow_reader);
//     //ARROW_RETURN_NOT_OK(parquet::arrow::OpenFile(input, pool, &arrow_reader));


//     std::shared_ptr<arrow::Table> table;
//     auto status = arrow_reader->ReadTable(&table);
//     //ARROW_RETURN_NOT_OK(arrow_reader->ReadTable(&table));

//     std::vector<i64> cast;
//     cast.reserve(table->num_rows());
//     for (auto chunk : table->GetColumnByName("PULocationID")->chunks()) {
//         auto data = std::static_pointer_cast<arrow::Int32Array>(chunk);
        
//         for (auto it = data->begin(); it != data->end(); it++) {
//             i32 v =  (*it).value();
//             cast.emplace_back(v);
//         }
//     }
    
//     size_t size = cast.size();
//     auto result =  Histogram<Bucket>(std::move(cast), partitions);
//     std::cerr << "Read " << filepath << "dataset. Rows num: " << size  << " Distribution " << result << "\n";
    
//     return result;
// }


int main() {

    i32 partitions_num = 8;
    std::vector<i64> raw_1 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-01.parquet", "PULocationID");
    std::vector<i64> raw_2 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-02.parquet", "PULocationID");
    std::vector<i64> raw_3 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-03.parquet", "PULocationID");
    Histogram<Bucket> hist1(std::move(raw_1), partitions_num);
    std::cerr << " Distribution " << hist1 << "\n";
    Histogram<Bucket> hist2(std::move(raw_2), partitions_num);
    std::cerr << " Distribution " << hist2 << "\n";
    Histogram<Bucket> hist3(std::move(raw_3), partitions_num);
    std::cerr << " Distribution " << hist3 << "\n";
    //auto hist1 = read_dataset("/home/vafilonov/taxidata/yellow_tripdata_2024-01.parquet", partitions_num);
    //auto hist2 = read_dataset("/home/vafilonov/taxidata/yellow_tripdata_2024-02.parquet", partitions_num);
    //auto hist3 = read_dataset("/home/vafilonov/taxidata/yellow_tripdata_2024-03.parquet", partitions_num);



    auto merged = multiMerge(std::vector<Histogram<Bucket>>{hist1, hist2, hist3});

    std::cerr << "Merged " << merged << "\n";

    BinArray fit = multifit(merged, 8);

    std::cout << "partitions distribution: " << fit << "\n";

    return 0;
}
