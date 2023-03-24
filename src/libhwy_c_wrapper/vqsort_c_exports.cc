// Copyright 2023 Albano Daniele Salvatore
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/highway.h"
#include "hwy/contrib/sort/vqsort.h"

#define DLL_EXPORT_C extern "C" __attribute__((__visibility__("default")))
#define SORTER_ASCENDING_BODY_IMPL { \
  Sorter()(keys, n, SortAscending()); \
}
#define SORTER_DESCENDING_BODY_IMPL { \
  Sorter()(keys, n, SortDescending()); \
}

namespace hwy {

    DLL_EXPORT_C void vqsort_kv64_asc(K32V32* keys, size_t n) SORTER_ASCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_kv64_desc(K32V32* keys, size_t n) SORTER_DESCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_kv128_asc(K64V64* keys, size_t n) SORTER_ASCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_kv128_desc(K64V64* keys, size_t n) SORTER_DESCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_u16_asc(uint16_t* keys, size_t n) SORTER_ASCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_u16_desc(uint16_t* keys, size_t n) SORTER_DESCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_u32_asc(uint32_t* keys, size_t n) SORTER_ASCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_u32_desc(uint32_t* keys, size_t n) SORTER_DESCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_u64_asc(uint64_t* keys, size_t n) SORTER_ASCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_u64_desc(uint64_t* keys, size_t n) SORTER_DESCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_u128_asc(uint128_t* keys, size_t n) SORTER_ASCENDING_BODY_IMPL
    DLL_EXPORT_C void vqsort_u128_desc(uint128_t* keys, size_t n) SORTER_DESCENDING_BODY_IMPL

}
