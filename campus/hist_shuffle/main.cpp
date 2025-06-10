#include <util/system/types.h>

#include <arrow/type_fwd.h>


#include "histogram.h"
#include "merger.h"
#include "partitioner.h"
#include "hasher.h"

#include "arrow_reader.h"

#include <cstdio>


std::ostream& operator<<(std::ostream& out, const std::vector<size_t>& vec) {
    out << "[";
    for (auto v : vec) {
        out << v << " ";
    }
    out << "]";
    return out;
}

std::ostream& operator<<(std::ostream& out, const std::vector<i64>& vec) {
    out << "[";
    for (auto v : vec) {
        out << v << " ";
    }
    out << "]";
    return out;
}


enum class LocalHistType {
    FULL_ORDERED,
    P2,
    SAMPLE_ORDERED
};

enum class BalanceType {
    GREEDY_LPT,
    MULTIFIT,


    BALANCED_GREEDY,
    BALANCED_GREEDY_LPT
};

const char* to_string(LocalHistType lht) {
    switch (lht) {
        case LocalHistType::FULL_ORDERED: return "FULL_ORDERED";
        case LocalHistType::P2: return "P2";
        case LocalHistType::SAMPLE_ORDERED: return "SAMPLE_ORDERED";
    }
}

const char* to_string(BalanceType lht) {
    switch (lht) {
        case BalanceType::GREEDY_LPT: return "GREEDY_LPT";
        case BalanceType::MULTIFIT: return "MULTIFIT";
        case BalanceType::BALANCED_GREEDY: return "BALANCED_GREEDY";
        case BalanceType::BALANCED_GREEDY_LPT: return "BALANCED_GREEDY_LPT";
    }
}

struct HasherStats {

    ModuloHasher modulo;
    PrimeHasher prime;
    THashHasher thasher;


    HasherStats(size_t partitions) : modulo(partitions), prime(partitions), thasher(partitions)
    {}


    void consume(int64_t value, int source) {
        modulo.accept(value, source);
        prime.accept(value, source);
        thasher.accept(value, source);
    }

};




void exp1();
void exp2(int buckets, int partitions_num, int sample_size, std::vector<std::vector<i64>>& datasets);
void exp3(int buckets, int partitions_num, int sample_size, std::vector<std::vector<i64>>& datasets);

void expLPT(int buckets, int partitions_num, int sample_size, std::vector<std::vector<i64>>& datasets);

Histogram hist_partition(std::vector<i64>& raw, int bucketsCount, int sampleSize) {
    if (sampleSize > 0) {
        //std::sort(raw.begin(), raw.end());
        std::vector<i64> sample;
        sample.reserve(sampleSize);
        for (int i = 0; i < sampleSize; i++) {
            sample.push_back(raw[i]);
        }
        return buildOrdered(sample, bucketsCount);
    }
    return buildOrdered(raw, bucketsCount);
}

Histogram hist_partition(std::string name, std::string colName, int bucketsCount, int sampleSize) {
    std::vector<i64> raw = read_dataset<i64, arrow::Int32Array>(name, colName);
    if (sampleSize > 0) {
        raw.resize(sampleSize);
    }
    return buildOrdered(raw, bucketsCount);
}

Histogram hist_partition(const std::vector<i64>& raw, int bucketsCount, int sampleSize, LocalHistType localHistType) {
    if (localHistType == LocalHistType::P2) {
        return buildP2(raw, bucketsCount);
    }
    if (sampleSize > 0) {
        //std::sort(raw.begin(), raw.end());
        std::vector<i64> sample;
        sample.reserve(sampleSize);
        for (int i = 0; i < sampleSize; i++) {
            sample.push_back(raw[i]);
        }
        return buildOrdered(sample, bucketsCount);
    }
    return buildOrdered(raw, bucketsCount);
}

BinArray balancePartitions(MultiHistogram hist, int partitionsNum, double balanceCoef, BalanceType balanceType) {

    switch (balanceType) {
        case BalanceType::GREEDY_LPT: {
            return greedyLPT(hist, partitionsNum);
        }
        case BalanceType::MULTIFIT: {
            return multifit(hist, partitionsNum);
        }
        case BalanceType::BALANCED_GREEDY: {
            return greedyBalance(hist, partitionsNum);
        }
        case BalanceType::BALANCED_GREEDY_LPT: {
            return multiBalance(hist, partitionsNum, balanceCoef);
        }
    }
    
}



void doExp(int buckets, int partitions_num, int sample_size, const std::vector<std::vector<i64>>& datasets, LocalHistType histType, BalanceType balanceType, double balanceCoef) {
    std::vector<Histogram> hists;
    for (auto& part : datasets) {
        hists.emplace_back(hist_partition(part, buckets, sample_size, histType));
        //std::cerr << "process part\n";
    }

    // for (const auto& h : hists) {
    //     std::cerr << h << "\n";
    // }


    MultiHistogram merged = multiMerge(hists);
    //std::cerr << "merge " << merged << "\n";
    BinArray fit = balancePartitions(merged, partitions_num, balanceCoef, balanceType);
    //BinArray fit = multiBalance(merged, partitions_num, 3.);
    std::cerr << " balance " << to_string(balanceType) << "\n";

    auto partitioner = std::make_shared<BSPartitioner>(std::move(fit));

    HistCollector collector(partitions_num, partitioner);

    std::vector<i64> hist_shuffle(partitions_num);

    std::vector<size_t> out_stat_hist(partitions_num, 0);
    
    int dataset_source_idx = 0;
    for (const auto& part : datasets) {
        for (i64 key : part) {
            collector.accept(key, dataset_source_idx);
        }
        dataset_source_idx++;
    }
    
    std::fprintf(stdout, "%s,%s,%d,%d,%.2f,%.12f,%.6f\n", to_string(histType), to_string(balanceType), buckets, sample_size, balanceCoef, collector.calcMSE(), collector.calcShuffle());
}


int main() {
    //exp1();
    ///exp2(100, 15, 2000);
    // std::vector<i64> raw_1 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-01.parquet", "PULocationID");
    // std::vector<i64> raw_2 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-02.parquet", "PULocationID");
    // std::vector<i64> raw_3 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-03.parquet", "PULocationID");



    // std::vector<i64> raw_1 = read_dataset<i64, arrow::Int64Array>("datasets/general_uniformal/gen_uniform_0.parquet", "test_name");
    // std::vector<i64> raw_2 = read_dataset<i64, arrow::Int64Array>("datasets/general_uniformal/gen_uniform_1.parquet", "test_name");
    // std::vector<i64> raw_3 = read_dataset<i64, arrow::Int64Array>("datasets/general_uniformal/gen_uniform_2.parquet", "test_name");
    // std::vector<i64> raw_4 = read_dataset<i64, arrow::Int64Array>("datasets/general_uniformal/gen_uniform_3.parquet", "test_name");
    // std::vector<i64> raw_5 = read_dataset<i64, arrow::Int64Array>("datasets/general_uniformal/gen_uniform_4.parquet", "test_name");
    // std::vector<i64> raw_6 = read_dataset<i64, arrow::Int64Array>("datasets/general_uniformal/gen_uniform_5.parquet", "test_name");
    // std::vector<i64> raw_7 = read_dataset<i64, arrow::Int64Array>("datasets/general_uniformal/gen_uniform_6.parquet", "test_name");
    // std::vector<i64> raw_8 = read_dataset<i64, arrow::Int64Array>("datasets/general_uniformal/gen_uniform_7.parquet", "test_name");

    // std::vector<i64> raw_1 = read_dataset<i64, arrow::Int64Array>("datasets/nonintersect_uniformal/non_intersect_uniformal_0.parquet", "test_name");
    // std::vector<i64> raw_2 = read_dataset<i64, arrow::Int64Array>("datasets/nonintersect_uniformal/non_intersect_uniformal_1.parquet", "test_name");
    // std::vector<i64> raw_3 = read_dataset<i64, arrow::Int64Array>("datasets/nonintersect_uniformal/non_intersect_uniformal_2.parquet", "test_name");
    // std::vector<i64> raw_4 = read_dataset<i64, arrow::Int64Array>("datasets/nonintersect_uniformal/non_intersect_uniformal_3.parquet", "test_name");
    // std::vector<i64> raw_5 = read_dataset<i64, arrow::Int64Array>("datasets/nonintersect_uniformal/non_intersect_uniformal_4.parquet", "test_name");
    // std::vector<i64> raw_6 = read_dataset<i64, arrow::Int64Array>("datasets/nonintersect_uniformal/non_intersect_uniformal_5.parquet", "test_name");
    // std::vector<i64> raw_7 = read_dataset<i64, arrow::Int64Array>("datasets/nonintersect_uniformal/non_intersect_uniformal_6.parquet", "test_name");
    // std::vector<i64> raw_8 = read_dataset<i64, arrow::Int64Array>("datasets/nonintersect_uniformal/non_intersect_uniformal_7.parquet", "test_name");
    

    // std::vector<i64> raw_1 = read_dataset<i64, arrow::Int64Array>("datasets/local_normal/gen_local_normal_0.parquet", "test_name");
    // std::vector<i64> raw_2 = read_dataset<i64, arrow::Int64Array>("datasets/local_normal/gen_local_normal_1.parquet", "test_name");
    // std::vector<i64> raw_3 = read_dataset<i64, arrow::Int64Array>("datasets/local_normal/gen_local_normal_2.parquet", "test_name");
    // std::vector<i64> raw_4 = read_dataset<i64, arrow::Int64Array>("datasets/local_normal/gen_local_normal_3.parquet", "test_name");
    // std::vector<i64> raw_5 = read_dataset<i64, arrow::Int64Array>("datasets/local_normal/gen_local_normal_4.parquet", "test_name");
    // std::vector<i64> raw_6 = read_dataset<i64, arrow::Int64Array>("datasets/local_normal/gen_local_normal_5.parquet", "test_name");
    // std::vector<i64> raw_7 = read_dataset<i64, arrow::Int64Array>("datasets/local_normal/gen_local_normal_6.parquet", "test_name");
    // std::vector<i64> raw_8 = read_dataset<i64, arrow::Int64Array>("datasets/local_normal/gen_local_normal_7.parquet", "test_name");

    std::vector<i64> raw_1 = read_dataset<i64, arrow::Int64Array>("datasets/single_overloaded/single_overloaded_0.parquet", "test_name");
    std::vector<i64> raw_2 = read_dataset<i64, arrow::Int64Array>("datasets/single_overloaded/single_overloaded_1.parquet", "test_name");
    std::vector<i64> raw_3 = read_dataset<i64, arrow::Int64Array>("datasets/single_overloaded/single_overloaded_2.parquet", "test_name");
    std::vector<i64> raw_4 = read_dataset<i64, arrow::Int64Array>("datasets/single_overloaded/single_overloaded_3.parquet", "test_name");
    std::vector<i64> raw_5 = read_dataset<i64, arrow::Int64Array>("datasets/single_overloaded/single_overloaded_4.parquet", "test_name");
    std::vector<i64> raw_6 = read_dataset<i64, arrow::Int64Array>("datasets/single_overloaded/single_overloaded_5.parquet", "test_name");
    std::vector<i64> raw_7 = read_dataset<i64, arrow::Int64Array>("datasets/single_overloaded/single_overloaded_6.parquet", "test_name");
    std::vector<i64> raw_8 = read_dataset<i64, arrow::Int64Array>("datasets/single_overloaded/single_overloaded_7.parquet", "test_name");

    // std::cerr << raw_1.size() << " " << raw_2.size() << " " << raw_3.size() << "\n";
    std::vector<std::vector<i64>> data = {std::move(raw_1), std::move(raw_2), std::move(raw_3), std::move(raw_4), std::move(raw_5), std::move(raw_6), std::move(raw_7), std::move(raw_8)};
    int partitions_num = 8;
    
    std::vector<int> buckets = {64, 128, 256 };
    //std::vector<int> partitions = {5, 10, 15, 30 };
    std::vector<int> sample_size = {0, 5000,10000 };
    std::vector<LocalHistType> h_type = {LocalHistType::SAMPLE_ORDERED, LocalHistType::FULL_ORDERED};
    std::vector<BalanceType> b_type = {BalanceType::GREEDY_LPT, BalanceType::MULTIFIT, BalanceType::BALANCED_GREEDY, BalanceType::BALANCED_GREEDY_LPT };
    std::vector<double> b_coefs = {1., 1.5, 2., 2.5, 3.};

    
    //expLPT(256, 5, 0, data);  


    for (auto h_t : h_type) {
        for (auto b_t : b_type) {
            for (auto b : buckets) {
                for (auto s : sample_size) {
                    if (s > 0 && h_t != LocalHistType::SAMPLE_ORDERED) {
                        break;
                    }
                    if (b_t == BalanceType::BALANCED_GREEDY || b_t == BalanceType::BALANCED_GREEDY_LPT) {
                        for (auto coef : b_coefs) {
                            doExp(b, partitions_num, s, data, h_t, b_t, coef);    
                        }
                    } else {
                        doExp(b, partitions_num, s, data, h_t, b_t, 1.);
                        break;
                    }
                }
            }
        }
    }
    
    HasherStats stats((partitions_num));
    int dataset_source_idx = 0;
    for (const auto& part : data) {
        for (i64 key : part) {
            stats.consume(key, dataset_source_idx);
        }
        dataset_source_idx++;
    }
    std::cerr << stats.modulo.getPartitionStat() << "\n";
    std::cerr << stats.modulo.getShuffleStat() << "\n";
    std::fprintf(stdout, "%s,%s,%d,%d,%.2f,%.12f,%.6f\n", "null", "MODULO", 0, 0, 1., stats.modulo.calcMSE(), stats.modulo.calcShuffle());
    std::fprintf(stdout, "%s,%s,%d,%d,%.2f,%.12f,%.6f\n", "null", "PRIME", 0, 0, 1., stats.prime.calcMSE(), stats.prime.calcShuffle());
    std::fprintf(stdout, "%s,%s,%d,%d,%.2f,%.12f,%.6f\n", "null", "THASH", 0, 0, 1., stats.thasher.calcMSE(), stats.thasher.calcShuffle());
    
    return 0;
}


static int decide_hash_partition_modulo(i64 key, int partitionNum) {
    return key % partitionNum;
}


// static int decide_hash_parition_fib_4(i64 key) {
//     return static_cast<int>((key * 11400714819323198485llu) >> 62);
// }

// static int decide_hash_parition_fib_8(i64 key) {
//     return static_cast<int>((key * 11400714819323198485llu) >> 61);
// }

// static int decide_hash_parition_prime(i64 key, int partitionNum) {

// }

void exp1() {
    i32 partitions_num = 3;

    std::vector<i64> raw_1 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-01.parquet", "PULocationID");
    std::vector<i64> raw_2 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-02.parquet", "PULocationID");
    std::vector<i64> raw_3 = read_dataset<i64, arrow::Int32Array>("/home/vafilonov/taxidata/yellow_tripdata_2024-03.parquet", "PULocationID");
    Histogram hist1 = buildOrdered(raw_1, partitions_num);
    Histogram p1 = buildP2(raw_1, partitions_num);
    std::cerr << " Distribution " << hist1 << "\n";
    std::cerr << " P2 " << p1 << "\n";
    Histogram hist2 = buildOrdered(raw_2, partitions_num);
    Histogram p2 = buildP2(raw_2, partitions_num);
    std::cerr << " Distribution " << hist2 << "\n";
    std::cerr << " P2 " << p2 << "\n";
    Histogram hist3 = buildOrdered(raw_3, partitions_num);
    Histogram p3 = buildP2(raw_3, partitions_num);
    std::cerr << " Distribution " << hist3 << "\n";
    std::cerr << " P2 " << p3 << "\n";
    //auto hist1 = read_dataset("/home/vafilonov/taxidata/yellow_tripdata_2024-01.parquet", partitions_num);
    //auto hist2 = read_dataset("/home/vafilonov/taxidata/yellow_tripdata_2024-02.parquet", partitions_num);
    //auto hist3 = read_dataset("/home/vafilonov/taxidata/yellow_tripdata_2024-03.parquet", partitions_num);



    auto merged = multiMerge(std::vector<Histogram>{hist1, hist2, hist3});

    std::cerr << "Merged " << merged << "\n";

    BinArray fit = multifit(merged, 8);

    std::cerr << "partitions distribution: " << fit << "\n";
}


void exp2(int buckets, int partitions_num, int sample_size, std::vector<std::vector<i64>>& datasets) {
    
    std::vector<Histogram> hists;
    for (auto& part : datasets) {
        hists.emplace_back(hist_partition(part, buckets, sample_size));
        //std::cerr << "process part\n";
    }

    for (const auto& h : hists) {
        std::cerr << h << "\n";
    }
    MultiHistogram merged = multiMerge(hists);
    std::cerr << "merge " << merged << "\n";
    BinArray fit = multifit(merged, partitions_num);
    std::cerr << "fit " << fit << "\n";

    BSPartitioner partitioner(fit);
    //std::cerr << "part\n";

    std::vector<i64> hist_shuffle(partitions_num);
    std::vector<i64> hash_shuffle(partitions_num);

    std::vector<size_t> out_stat_hist(partitions_num, 0);
    std::vector<size_t> out_stat_hash(partitions_num, 0);
    size_t total = 0;
    int dataset_source_idx = 0;
    size_t hist_shuffle_sum = 0;
    size_t hash_shuffle_sum = 0;
    for (const auto& part : datasets) {
        //std::cerr << "read partition\n";
        for (i64 key : part) {
            total++;
            int hist_partition = partitioner.decide_partition(key);
            //try {
            out_stat_hist[hist_partition]++;
            if (hist_partition != dataset_source_idx) {
                hist_shuffle[dataset_source_idx]++;
                hist_shuffle_sum++;
            }
            // } catch (const std::exception& e) {
            //     std::cerr << hist_partition;
            //     std::rethrow_exception(std::current_exception());
            // }
            int hash_partition = decide_hash_partition_modulo(key, partitions_num);
            out_stat_hash[hash_partition]++;
            if (hash_partition != dataset_source_idx) {
                hash_shuffle[dataset_source_idx]++;
                hash_shuffle_sum++;
            }
        }
        dataset_source_idx++;
        //std::cerr << "partitioned partition\n";
    }

    i64 ideal = total / partitions_num;
    std::cerr << "ideal " << ideal << "\n";
    std::vector<i64> hist_error(partitions_num);
    std::vector<i64> hash_error(partitions_num);
    double hist_mse = 0;
    double hash_mse = 0;
    for (i32 i = 0; i < partitions_num; i++) {
        i64 hist_delta = ideal - static_cast<i64>(out_stat_hist[i]);
        double norm_hist_delta = hist_delta * 1. / total;
        hist_error[i] = hist_delta;
        hist_mse += norm_hist_delta * norm_hist_delta;
        i64 hash_delta = ideal - static_cast<i64>(out_stat_hash[i]); 
        double norm_hash_delta = hash_delta * 1. / total;
        hash_error[i] = hash_delta;
        hash_mse += norm_hash_delta * norm_hash_delta;
    }

    hist_mse /= partitions_num;
    hash_mse /= partitions_num;

    std::cerr << "Buckets: " << buckets << " partitions " << partitions_num << "\n";
    std::cerr << "hist_shuffle: " << hist_shuffle << " " << hist_shuffle_sum << " " << hist_shuffle_sum * 1. / total << "\n";
    std::cerr << "hash_shuffle: " << hash_shuffle << " " << hash_shuffle_sum << " " << hash_shuffle_sum * 1. / total << "\n";
    std::cerr << "hist_stat: " << out_stat_hist << "\n";
    std::cerr << "hash_stat: " << out_stat_hash << "\n";
    std::cerr << "hist_err: " << hist_error << "\n";
    std::cerr << "hash_err: " << hash_error << "\n";
    std::cerr << "hist_mse: " << hist_mse / partitions_num << "\n";
    std::cerr << "hash_mse: " << hash_mse / partitions_num << "\n";
    std::cerr << "\n";
    
    std::fprintf(stdout, "%d,%d,%d,%.3f,%.6f,%.6f\n", sample_size, buckets, partitions_num, hist_mse, hash_mse, hist_mse - hash_mse);
}

void exp3(int buckets, int partitions_num, int sample_size, std::vector<std::vector<i64>>& datasets) {
    std::vector<Histogram> hists;
    for (auto& part : datasets) {
        hists.emplace_back(hist_partition(part, buckets, sample_size));
        //std::cerr << "process part\n";
    }

    for (const auto& h : hists) {
        std::cerr << h << "\n";
    }
    MultiHistogram merged = multiMerge(hists);
    std::cerr << "merge " << merged << "\n";
    BinArray fit = simpleBalance(merged, partitions_num);
    //BinArray fit = multiBalance(merged, partitions_num, 3.);
    std::cerr << "simple balance " << fit << "\n";

    BSPartitioner partitioner(fit);
    //std::cerr << "part\n";

    std::vector<i64> hist_shuffle(partitions_num);
    std::vector<i64> hash_shuffle(partitions_num);

    std::vector<size_t> out_stat_hist(partitions_num, 0);
    std::vector<size_t> out_stat_hash(partitions_num, 0);
    size_t total = 0;
    int dataset_source_idx = 0;
    size_t hist_shuffle_sum = 0;
    size_t hash_shuffle_sum = 0;
    for (const auto& part : datasets) {
        //std::cerr << "read partition\n";
        for (i64 key : part) {
            total++;
            int hist_partition = partitioner.decide_partition(key);
            //try {
            out_stat_hist[hist_partition]++;
            if (hist_partition != dataset_source_idx) {
                hist_shuffle[dataset_source_idx]++;
                hist_shuffle_sum++;
            }
            // } catch (const std::exception& e) {
            //     std::cerr << hist_partition;
            //     std::rethrow_exception(std::current_exception());
            // }
            int hash_partition = decide_hash_partition_modulo(key, partitions_num);
            out_stat_hash[hash_partition]++;
            if (hash_partition != dataset_source_idx) {
                hash_shuffle[dataset_source_idx]++;
                hash_shuffle_sum++;
            }
        }
        dataset_source_idx++;
        //std::cerr << "partitioned partition\n";
    }

    i64 ideal = total / partitions_num;
    std::cerr << "ideal " << ideal << "\n";
    std::vector<i64> hist_error(partitions_num);
    std::vector<i64> hash_error(partitions_num);
    double hist_mse = 0;
    double hash_mse = 0;
    for (i32 i = 0; i < partitions_num; i++) {
        i64 hist_delta = ideal - static_cast<i64>(out_stat_hist[i]);
        double norm_hist_delta = hist_delta * 1. / total;
        hist_error[i] = hist_delta;
        hist_mse += norm_hist_delta * norm_hist_delta;
        i64 hash_delta = ideal - static_cast<i64>(out_stat_hash[i]); 
        double norm_hash_delta = hash_delta * 1. / total;
        hash_error[i] = hash_delta;
        hash_mse += norm_hash_delta * norm_hash_delta;
    }

    hist_mse /= partitions_num;
    hash_mse /= partitions_num;

    std::cerr << "Buckets: " << buckets << " partitions " << partitions_num << "\n";
    std::cerr << "hist_shuffle: " << hist_shuffle << " " << hist_shuffle_sum << " " << hist_shuffle_sum * 1. / total << "\n";
    std::cerr << "hash_shuffle: " << hash_shuffle << " " << hash_shuffle_sum << " " << hash_shuffle_sum * 1. / total << "\n";
    std::cerr << "hist_stat: " << out_stat_hist << "\n";
    std::cerr << "hash_stat: " << out_stat_hash << "\n";
    std::cerr << "hist_err: " << hist_error << "\n";
    std::cerr << "hash_err: " << hash_error << "\n";
    std::cerr << "hist_mse: " << hist_mse / partitions_num << "\n";
    std::cerr << "hash_mse: " << hash_mse / partitions_num << "\n";
    std::cerr << "\n";
    
    std::fprintf(stdout, "%d,%d,%d,%.3f,%.6f,%.6f\n", sample_size, buckets, partitions_num, hist_mse, hash_mse, hist_mse - hash_mse);
}

void expLPT(int buckets, int partitions_num, int sample_size, std::vector<std::vector<i64>>& datasets) {
    std::vector<Histogram> hists;
    for (auto& part : datasets) {
        hists.emplace_back(hist_partition(part, buckets, sample_size));
        //std::cerr << "process part\n";
    }

    // for (const auto& h : hists) {
    //     std::cerr << h << "\n";
    // }
    MultiHistogram merged = multiMerge(hists);
    std::cerr << "merge " << merged << "\n";
    BinArray fit = greedyLPT(merged, partitions_num);
    std::cerr << "balance " << fit << "\n";

    BSPartitioner partitioner(fit);
    //std::cerr << "part\n";

    std::vector<i64> hist_shuffle(partitions_num);
    std::vector<i64> hash_shuffle(partitions_num);

    std::vector<size_t> out_stat_hist(partitions_num, 0);
    std::vector<size_t> out_stat_hash(partitions_num, 0);
    size_t total = 0;
    int dataset_source_idx = 0;
    size_t hist_shuffle_sum = 0;
    size_t hash_shuffle_sum = 0;
    for (const auto& part : datasets) {
        //std::cerr << "read partition\n";
        for (i64 key : part) {
            total++;
            int hist_partition = partitioner.decide_partition(key);
            //try {
            out_stat_hist[hist_partition]++;
            if (hist_partition != dataset_source_idx) {
                hist_shuffle[dataset_source_idx]++;
                hist_shuffle_sum++;
            }
            // } catch (const std::exception& e) {
            //     std::cerr << hist_partition;
            //     std::rethrow_exception(std::current_exception());
            // }
            int hash_partition = decide_hash_partition_modulo(key, partitions_num);
            //int hash_partition = decide_hash_parition_fib_8(key) % partitions_num;
            out_stat_hash[hash_partition]++;
            if (hash_partition != dataset_source_idx) {
                hash_shuffle[dataset_source_idx]++;
                hash_shuffle_sum++;
            }
        }
        dataset_source_idx++;
        //std::cerr << "partitioned partition\n";
    }

    i64 ideal = total / partitions_num;
    std::cerr << "ideal " << ideal << "\n";
    std::vector<i64> hist_error(partitions_num);
    std::vector<i64> hash_error(partitions_num);
    double hist_mse = 0;
    double hash_mse = 0;
    for (i32 i = 0; i < partitions_num; i++) {
        i64 hist_delta = ideal - static_cast<i64>(out_stat_hist[i]);
        double norm_hist_delta = hist_delta * 1. / total;
        hist_error[i] = hist_delta;
        hist_mse += norm_hist_delta * norm_hist_delta;
        i64 hash_delta = ideal - static_cast<i64>(out_stat_hash[i]); 
        double norm_hash_delta = hash_delta * 1. / total;
        hash_error[i] = hash_delta;
        hash_mse += norm_hash_delta * norm_hash_delta;
    }

    hist_mse /= partitions_num;
    hash_mse /= partitions_num;


    std::cerr << "Buckets: " << buckets << " partitions " << partitions_num << "\n";
    std::cerr << "hist_shuffle: " << hist_shuffle << " " << hist_shuffle_sum << " " << hist_shuffle_sum * 1. / total << "\n";
    std::cerr << "hash_shuffle: " << hash_shuffle << " " << hash_shuffle_sum << " " << hash_shuffle_sum * 1. / total << "\n";
    std::cerr << "hist_stat: " << out_stat_hist << "\n";
    std::cerr << "hash_stat: " << out_stat_hash << "\n";
    std::cerr << "hist_err: " << hist_error << "\n";
    std::cerr << "hash_err: " << hash_error << "\n";
    std::cerr << "hist_mse: " << hist_mse / partitions_num << "\n";
    std::cerr << "hash_mse: " << hash_mse / partitions_num << "\n";
    std::cerr << "\n";
    
    std::fprintf(stdout, "%d,%d,%d,%.3f,%.6f,%.6f\n", sample_size, buckets, partitions_num, hist_mse, hash_mse, hist_mse - hash_mse);
}

