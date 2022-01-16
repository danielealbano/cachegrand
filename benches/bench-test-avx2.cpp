//#include <string>
//#include <benchmark/benchmark.h>
//#include <cassert>
//#include <cstdint>
//#include <immintrin.h>
//
//double calculate_similarity_score(
//        int fingerprint_size,
//        uint16_t *fingerprint_input,
//        uint16_t *fingerprint_datastore) {
//    __m256i fingerprint_input_chunk_vector, fingerprint_datastore_chunk_vector;
//    __m256i zero_vector, fingerprint_input_chunk_zero_vector, fingerprint_datastore_chunk_nonzero_vector;
//    __m256i fingerprint_diff_abs_vector;
//
//    assert((fingerprint_size % 32) == 0);
//    zero_vector = _mm256_set1_epi64x(0);
//    double total_diff = 0;
//
//    for(int fingerprint_chunk = 0; fingerprint_chunk < fingerprint_size; fingerprint_chunk += 16) {
//        uint16_t fingerprint_diff_abs[16] =  { 0 };
//
//        // Load the chunks to compare
//        fingerprint_input_chunk_vector =
//                _mm256_loadu_si256((__m256i*)(fingerprint_input + fingerprint_chunk));
//        fingerprint_datastore_chunk_vector =
//                _mm256_loadu_si256((__m256i*)(fingerprint_datastore + fingerprint_chunk));
//
//        // We want to compare only counters that are non zero in the input chunk, so we find the 0, perform
//        // a NOT operation and then and AND operation
//        fingerprint_input_chunk_zero_vector =
//                _mm256_cmpeq_epi16(fingerprint_input_chunk_vector, zero_vector);
//        fingerprint_datastore_chunk_nonzero_vector =
//                _mm256_andnot_si256(fingerprint_input_chunk_zero_vector, fingerprint_datastore_chunk_vector);
//
//        // Perform the abs sub between fingerprint_datastore_chunk_nonzero_vector and
//        // fingerprint_input_chunk_zero_vector to find how much the counters are different
//        fingerprint_diff_abs_vector = _mm256_or_si256(
//                _mm256_subs_epu16(fingerprint_input_chunk_vector, fingerprint_datastore_chunk_nonzero_vector),
//                _mm256_subs_epu16(fingerprint_datastore_chunk_nonzero_vector, fingerprint_input_chunk_vector));
//
//        _mm256_storeu_si256((__m256i*)fingerprint_diff_abs, fingerprint_diff_abs_vector);
//
//        for(int i = 0; i < 16; i++) {
//            if (fingerprint_input[fingerprint_chunk + i] == 0) {
//                continue;
//            }
//
//            double temp_diff = (double)fingerprint_diff_abs[i] / (double)fingerprint_input[fingerprint_chunk + i];
//            total_diff += temp_diff > 1 ? 1 : temp_diff;
//        }
//    }
//
//    double res = (double)1 - (total_diff / (double)fingerprint_size);
//
//    return res;
//}
//
//static void fingerprint_similarity_score_avx2_benchmark(benchmark::State& state) {
//    int precision = 256;
//    int fingerprint_size = precision * precision;
//    uint16_t *fingerprint_input, *fingerprint_datastore;
//
//    fingerprint_input = (uint16_t*)malloc(sizeof(uint16_t) * fingerprint_size);
//    fingerprint_datastore = (uint16_t*)malloc(sizeof(uint16_t) * fingerprint_size);
//
//    for(int i = 0; i < 65536; i++) {
//        fingerprint_datastore[i] = 1000;
//    }
//
//    for(int i = 0; i < 65536; i++) {
//        fingerprint_input[i] = 100;
//    }
//
//    for (auto _ : state) {
//        volatile double similarity_score = 0;
//        benchmark::DoNotOptimize((similarity_score =
//                calculate_similarity_score(fingerprint_size, fingerprint_input, fingerprint_datastore)));
//    }
//}
//
//static void BenchArguments(benchmark::internal::Benchmark* b) {
//    b->Iterations(10000)->Repetitions(10)->Threads(8)->UseRealTime();
//}
//
//BENCHMARK(fingerprint_similarity_score_avx2_benchmark)
//    ->Apply(BenchArguments);
