/**
 * Copyright (C) 2018-2023 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cmath>
#include <cstring>
#include <numa.h>

#include <benchmark/benchmark.h>
#include <array>
#include <memory>

#include "benchmark-program-simple.hpp"

template<typename T>
class TypedArrayWithRandomValuesFixture : public benchmark::Fixture {
private:
    T* array = nullptr;
    int length = 0;

public:
    T* GetArray() {
        return this->array;
    }

    int GetLength() {
        return this->length;
    }

    void SetUp(const ::benchmark::State& state) override {
        this->length = (int)state.range(0);

        this->array = (T*)malloc(this->length * sizeof(T));
        memset(this->array, 0, this->length * sizeof(T));

        // Fill up the array with random values
        for (int i = 0; i < this->length; i++) {
            if constexpr (std::is_same_v<T, K64V64>)
            {
                ((K64V64*)this->array)[i].key = random();
            }
            else if constexpr (std::is_same_v<T, K32V32>)
            {
                ((K32V32*)this->array)[i].key = random();
            }
            else
            {
                this->array[i] = random();
            }
        }
    }

    void TearDown(const ::benchmark::State& state) override {
        free(this->array);
        this->length = 0;
    }
};

// vqsort
BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortU32Desc, uint32_t)(benchmark::State& state) {
    uint32_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortU32Desc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortU32Asc, uint32_t)(benchmark::State& state) {
    uint32_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortU32Asc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortU64Desc, uint64_t)(benchmark::State& state) {
    uint64_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortU64Desc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortU64Asc, uint64_t)(benchmark::State& state) {
    uint64_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortU64Asc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortU128Desc, uint128_t)(benchmark::State& state) {
    uint128_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortU128Desc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortU128Asc, uint128_t)(benchmark::State& state) {
    uint128_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortU128Asc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortKV64Desc, K64V64)(benchmark::State& state) {
    K64V64 *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortKV128Desc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortKV64Asc, K64V64)(benchmark::State& state) {
    K64V64 *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortKV128Asc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortKV32Desc, K32V32)(benchmark::State& state) {
    K32V32 *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortKV64Desc(array, length);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, VqsortKV32Asc, K32V32)(benchmark::State& state) {
    K32V32 *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        SortKV64Asc(array, length);
    }
}

// c qsort
static int CompareU32Desc(const void *a, const void *b) {
    if (*(const uint32_t *)a < *(const uint32_t *)b) { return 1; } else { return -1; } }
static int CompareU32Asc(const void *a, const void *b) {
    if (*(const uint32_t *)a < *(const uint32_t *)b) { return -1; } else { return 1; } }

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, QSortU32Desc, uint32_t)(benchmark::State& state) {
    uint32_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        qsort(array, length, sizeof(uint32_t), CompareU32Desc);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, QSortU32Asc, uint32_t)(benchmark::State& state) {
    uint32_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        qsort(array, length, sizeof(uint32_t), CompareU32Asc);
    }
}

static int CompareU64Desc(const void *a, const void *b) {
    if (*(const uint64_t *)a < *(const uint64_t *)b) { return 1; } else { return -1; } }
static int CompareU64Asc(const void *a, const void *b) {
    if (*(const uint64_t *)a < *(const uint64_t *)b) { return -1; } else { return 1; } }

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, QSortU64Desc, uint64_t)(benchmark::State& state) {
    uint64_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        qsort(array, length, sizeof(uint64_t), CompareU64Desc);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, QSortU64Asc, uint64_t)(benchmark::State& state) {
    uint64_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        qsort(array, length, sizeof(uint64_t), CompareU64Asc);
    }
}

static int CompareU128Desc(const void *a, const void *b) {
    if (*(const uint128_t *)a < *(const uint128_t *)b) { return 1; } else { return -1; } }
static int CompareU128Asc(const void *a, const void *b) {
    if (*(const uint128_t *)a < *(const uint128_t *)b) { return -1; } else { return 1; } }

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, QSortU128Desc, uint128_t)(benchmark::State& state) {
    uint128_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        qsort(array, length, sizeof(uint128_t), CompareU128Desc);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, QSortU128Asc, uint128_t)(benchmark::State& state) {
    uint128_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        qsort(array, length, sizeof(uint128_t), CompareU128Asc);
    }
}

// IntroSort, from https://www.programmingalgorithms.com/algorithm/intro-sort/c/ with just some very light modifications
// and mostly code cleanup.
// It's very likely that the code is not optimized at all but it's just to have an high level idea of the performance.
void InsertionSort(uint64_t* data, size_t length) {
    for (size_t i = 1; i < length; ++i) {
        size_t j = i;

        while (j > 0) {
            if (data[j - 1] > data[j]) {
                data[j - 1] ^= data[j];
                data[j] ^= data[j - 1];
                data[j - 1] ^= data[j];

                --j;
            } else {
                break;
            }
        }
    }
}

void MaxHeapify(uint64_t* data, size_t heapSize, size_t index) {
    size_t left = (index + 1) * 2 - 1;
    size_t right = (index + 1) * 2;
    size_t largest;

    if (left < heapSize && data[left] > data[index]) {
        largest = left;
    } else {
        largest = index;
    }

    if (right < heapSize && data[right] > data[largest]) {
        largest = right;
    }

    if (largest != index) {
        uint64_t temp = data[index];
        data[index] = data[largest];
        data[largest] = temp;

        MaxHeapify(data, heapSize, largest);
    }
}

void HeapSort(uint64_t* data, size_t count) {
    size_t heapSize = count;

    for (int64_t p = (int64_t)((heapSize - 1) / 2); p >= 0; --p) {
        MaxHeapify(data, heapSize, p);
    }

    for (size_t i = count - 1; i > 0; --i) {
        uint64_t temp = data[i];
        data[i] = data[0];
        data[0] = temp;

        --heapSize;
        MaxHeapify(data, heapSize, 0);
    }
}

size_t Partition(uint64_t* data, size_t left, size_t right) {
    uint64_t pivot = data[right];
    size_t i = left;

    for (size_t j = left; j < right; ++j) {
        if (data[j] <= pivot) {
            uint64_t temp = data[j];
            data[j] = data[i];
            data[i] = temp;
            i++;
        }
    }

    data[right] = data[i];
    data[i] = pivot;

    return i;
}

void QuickSortRecursive(uint64_t* data, size_t left, size_t right) {
    if (left < right) {
        size_t q = Partition(data, left, right);
        QuickSortRecursive(data, left, q - 1);
        QuickSortRecursive(data, q + 1, right);
    }
}

void IntroSort(uint64_t* data, size_t length) {
    size_t partitionSize = Partition((uint64_t*)data, 0, length - 1);

    if (partitionSize < 16) {
        InsertionSort(data, length);
    } else if (partitionSize > (size_t)(2 * log(length))) {
        HeapSort(data, length);
    } else {
        QuickSortRecursive(data, 0, length - 1);
    }
}

BENCHMARK_TEMPLATE_DEFINE_F(TypedArrayWithRandomValuesFixture, IntroSortU64Asc, uint64_t)(benchmark::State& state) {
    uint64_t *array = this->GetArray();
    int length = this->GetLength();

    for (auto _ : state) {
        IntroSort(array, length);
    }
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b
        ->RangeMultiplier(10)
        ->Range(1000, 100000)
        ->Iterations(10000);
}

BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, VqsortU32Asc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, VqsortU64Asc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, VqsortU128Asc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, VqsortKV64Asc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, VqsortKV32Asc)->Apply(BenchArguments);

BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, QSortU32Asc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, QSortU64Asc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, QSortU128Asc)->Apply(BenchArguments);

// Only the uint64_t version is implemented for now as it's the primary use case. The type is currently hardcoded and
// doesn't support other types (e.g. as qsort does).
BENCHMARK_REGISTER_F(TypedArrayWithRandomValuesFixture, IntroSortU64Asc)->Apply(BenchArguments);
