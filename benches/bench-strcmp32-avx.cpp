/**
 * Copyright (C) 2018-2022 Daniele Salvatore Albano
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 **/

#include <cstring>
#include <numa.h>

#include <benchmark/benchmark.h>

#include "benchmark-program.hpp"

#include "utils_string.h"

class SameStringComparisonFixture : public benchmark::Fixture {
private:
    char *str_a = nullptr;
    char *str_b = nullptr;
    int length = 0;

public:
    char* GetA() {
        return this->str_a;
    }

    char* GetB() {
        return this->str_b;
    }

    int GetLength() {
        return this->length;
    }

    void SetUp(const ::benchmark::State& state) override {
        this->length = (int)state.range(0);

        this->str_a = (char*)malloc(this->length - (this->length % 32) + 32 + 1);
        this->str_b = (char*)malloc(this->length - (this->length % 32) + 32 + 1);

        memset(this->str_a, 'A', this->length);
        this->str_a[this->length + 1] = 0;

        memset(this->str_b, 'A', this->length);
        this->str_b[this->length + 1] = 0;
    }

    void TearDown(const ::benchmark::State& state) override {
        free(this->str_a);
        free(this->str_b);
        this->length = 0;
    }
};

class DifferentString50pComparisonFixture : public benchmark::Fixture {
private:
    char *str_a = nullptr;
    char *str_b = nullptr;
    int length = 0;

public:
    char* GetA() {
        return this->str_a;
    }

    char* GetB() {
        return this->str_b;
    }

    int GetLength() {
        return this->length;
    }

    void SetUp(const ::benchmark::State& state) override {
        this->length = (int)state.range(0);

        this->str_a = (char*)malloc(this->length - (this->length % 32) + 32 + 1);
        this->str_b = (char*)malloc(this->length - (this->length % 32) + 32 + 1);

        memset(this->str_a, 'A', this->length);
        this->str_a[this->length + 1] = 0;

        int first_half = this->length / 2;
        int second_half = this->length - first_half;
        memset(this->str_b, 'A', this->length);
        memset(this->str_b + first_half, 'B', second_half);
        str_b[this->length] = 0;
    }

    void TearDown(const ::benchmark::State& state) override {
        free(this->str_a);
        free(this->str_b);
        this->length = 0;
    }
};

BENCHMARK_DEFINE_F(SameStringComparisonFixture, StringCaseCmpAvx2)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(UTILS_STRING_NAME_IMPL(casecmp_eq_32, avx2)(
                this->GetA(),
                this->GetLength(),
                this->GetB(),
                this->GetLength()));
    }
}

BENCHMARK_DEFINE_F(DifferentString50pComparisonFixture, StringCaseCmpAvx2)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(UTILS_STRING_NAME_IMPL(casecmp_eq_32, avx2)(
                this->GetA(),
                this->GetLength(),
                this->GetB(),
                this->GetLength()));
    }
}

BENCHMARK_DEFINE_F(SameStringComparisonFixture, StringCaseCmpLibc)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(strncasecmp(
                this->GetA(),
                this->GetB(),
                this->GetLength()));
    }
}

BENCHMARK_DEFINE_F(DifferentString50pComparisonFixture, StringCaseCmpLibc)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(strncasecmp(
                this->GetA(),
                this->GetB(),
                this->GetLength()));
    }
}

BENCHMARK_DEFINE_F(SameStringComparisonFixture, StringCmpAvx2)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(UTILS_STRING_NAME_IMPL(cmp_eq_32, avx2)(
                this->GetA(),
                this->GetLength(),
                this->GetB(),
                this->GetLength()));
    }
}

BENCHMARK_DEFINE_F(DifferentString50pComparisonFixture, StringCmpAvx2)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(UTILS_STRING_NAME_IMPL(cmp_eq_32, avx2)(
                this->GetA(),
                this->GetLength(),
                this->GetB(),
                this->GetLength()));
    }
}

BENCHMARK_DEFINE_F(SameStringComparisonFixture, StringCmpLibc)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(strncmp(
                this->GetA(),
                this->GetB(),
                this->GetLength()));
    }
}

BENCHMARK_DEFINE_F(DifferentString50pComparisonFixture, StringCmpLibc)(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(strncmp(
                this->GetA(),
                this->GetB(),
                this->GetLength()));
    }
}

static void BenchArguments(benchmark::internal::Benchmark* b) {
    b->DenseRange(5, 100, 5)->Iterations(10000000);
}

BENCHMARK_REGISTER_F(SameStringComparisonFixture, StringCaseCmpAvx2)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(SameStringComparisonFixture, StringCaseCmpLibc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(DifferentString50pComparisonFixture, StringCaseCmpAvx2)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(DifferentString50pComparisonFixture, StringCaseCmpLibc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(SameStringComparisonFixture, StringCmpAvx2)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(SameStringComparisonFixture, StringCmpLibc)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(DifferentString50pComparisonFixture, StringCmpAvx2)->Apply(BenchArguments);
BENCHMARK_REGISTER_F(DifferentString50pComparisonFixture, StringCmpLibc)->Apply(BenchArguments);
