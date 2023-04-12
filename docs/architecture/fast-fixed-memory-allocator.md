FFMA (or Fast Fixed Memory Allocator)
=====================================

FFMA is SLAB memory allocator, from below a paragraph from Wikipedia:

> Slab allocation is a memory management mechanism intended for the efficient memory allocation of objects. Compared to
> earlier mechanisms, it reduces fragmentation caused by allocations and deallocations. The technique is used to retain
> allocated memory that contains a data object of a certain type for reuse upon subsequent allocations of objects of the
> same type. It is analogous to an object pool, but only applies to memory, not other resources."
> -- <cite>[Wikipedia - Slab Allocation][1]</cite>

A lot of different algorithms and implementations for SLAB allocators are available nowadays but most of them focus on
simply reducing memory fragmentation, sacrificing speed or introducing layers of complexity that replace an issue with
another.

The goal, for cachegrand, is to have a *fast* and *flexible* SLAB allocator to provide better performances and the
ability to collect metrics.

### Principles

The implementation relies on 3 foundational blocks:
- a slab slot represents an object, it follows the data-oriented pattern whereas the metadata are kept separated from
  the actual data providing flexibility to achieve better performance but also to keep the data cacheline-aligned and,
  if needed, page-aligned;
- a slab slice represents a block of aligned memory, the metadata themselves are held at the beginning followed by the
  metadata of the slab slots;
- a slab allocator that keeps track of the allocated slices, the available and occupied slots, the size of the objects,
  the metrics, it also allows to keep track of the allocated slices.

The terminology in place is slightly different from what can be commonly found on internet:
- a slab slice is a slab, for this implementation it matches a region of memory;
- a slab slot is an object managed by the object cache;
- a region is an aligned memory region, it can be a hugepage or a region of memory allocated via mmap.

To be able to provide a `O(1)` for the alloc/free operations FFMA relies on the aligned memory allocations, that are
the slab slices, and a double linked list used to track the available objects, kept at the beginning, and the used
objects kept at the end.

FFMA is lock-less in the hot-path and uses atomic operations in the slow-path, it's also numa-aware as all the memory is
fetched from the numa-domain executing the core.

FFMA is capable of releasing the memory allocated by the slab slices to the OS immediately after the last object is
freed, although one slab is always kept hot. This memory allocator doesn't use `madvise(MADV_DONTNEED)` or 
`madvise(MADV_FREE)` to release the memory to the OS, it uses the `munmap` because it's very efficient in releasing
the memory to the OS and therefore the extra cost of the `munmap` is amortized avoiding having to have an additional
garbage collector thread that would have to run periodically to release the memory to the OS.

In terms of security, in addition to using guard pages to catch memory overflows, another huge advantage of using FFMA
comes from a secure memory allocation, the memory allocated by FFMA is always allocated using a random address in
memory, this is achieved by using the `mmap` with the `MAP_FIXED_NOREPLACE` flag, and de-facto makes it impossible to
try to guess the address of the memory allocated by FFMA providing an additional layer of security against memory
attacks.

### Data Structures

```mermaid
classDiagram
  class ffma_t {
    double_linked_list_t *slots;
    double_linked_list_t *slices;
    queue_mpmc_t *free_ffma_slots_queue_from_other_threads;
    bool_volatile_t ffma_freed;
    uint32_t object_size;
    uint16_t metrics.slices_inuse_count;
    uint32_volatile_t metrics.objects_inuse_count;
  }
  
  class ffma_slice_t {
    double_linked_list_item_t double_linked_list_item
    void *data.padding[2]
    ffma_t *data.ffma
    void *data.page_addr
    uintptr_t data.data_addr
    bool data.available
    uint32_t data.metrics.objects_total_count
    uint32_t data.metrics.objects_initialized_count;
    uint32_t data.metrics.objects_inuse_count
    ffma_slot_t data.slots[]
  }
  
  class ffma_slot_t {
    double_linked_list_item_t double_linked_list_item
    void data.padding[2]
    void* data.memptr
    bool data.available
  }
  
  ffma_t "1" <--> "many" ffma_slice_t
  ffma_t "1" --> "many" ffma_slot_t
  ffma_slice_t "1" --> "many" ffma_slot_t
```

#### struct ffma (ffma_t)

```c
typedef struct ffma ffma_t;
struct ffma {
    double_linked_list_t *slots;
    double_linked_list_t *slices;
    queue_mpmc_t *free_ffma_slots_queue_from_other_threads;
    bool_volatile_t ffma_freed;
    uint32_t object_size;
    struct {
        uint16_t slices_inuse_count;
        uint32_volatile_t objects_inuse_count;
    } metrics;
};
```

The slab allocator is a container for the slab slices and can be used only by a specific thread for a specific object
size.

Because FFMA uses per-thread separation, the memory will be allocated in the numa domain of the thread, therefore
it's better, although not required, to bound the thread to a core of the cpu to get better performances.

The structure contains a double linked list of slots, sorted per availability where the available slots are
kept at the head and the in use slots are kept at the tail.
In this way, when it's necessary to fetch a slot it's possible to fetch it directly from the head if available.
If no slots are available, FFMA requests to the component that handles the cache of regions to provide a new one, once
received it initializes a slab slice out of the region and update the list of available slots.
A region can easily contain tens of thousands of 16 bytes objects, so the price is very well amortized for the small
objects, more explanation on the calculations are provided in the [ffma_slice_t](#struct-ffma_slice-ffma_slice_t)
section.

A queue called `free_ffma_slots_queue_from_other_threads` exists in case memory allocated by one thread gets freed by
another, the thread that doesn't own the memory passes it to the thread that ones it via a mpmc queue that uses atomic
operations to maintain a correct state without relying on more expensive locking operations.
In case FFMA has to fetch a slot but no more pre-allocated slots are available, then the queue is checked
to see if a thread any other thread has returned any object and, in case, it uses it. The object fetched from the queue
require fewer operations to be used.

The struct also contains a double linked list of slices, sorted per availability as well. When all the slots are freed
in a slice and this is returned to the regions cache to be reused.

This approach provides cache-locality and O(1) access in the best and average case - e.g. if there are pre-allocated
slots available.

#### struct ffma_slice (ffma_slice_t)

```c
typedef union {
    double_linked_list_item_t double_linked_list_item;
    struct {
        void *padding[2];
        ffma_t *ffma;
        void *page_addr;
        uintptr_t data_addr;
        bool available;
        struct {
            uint32_t objects_total_count;
            uint32_t objects_initialized_count;
            uint32_t objects_inuse_count;
        } metrics;
        ffma_slot_t slots[];
    } __attribute__((aligned(64))) data;
} ffma_slice_t;
```

A slab slice is a basically an aligned region of memory, the data of the structure is contained at the very beginning of
it.

A union is used to reduce memory waste, double_linked_list_item_t has a `data` field that would be wasted, in this case
the pointer to the `double_linked_list_item` can be cast back to `ffma_slice_t`.

The field `page_addr` points to the beginning of the slice and although it's a duplication,
`double_linked_list_item` it's the beginning of the slice itself, there is currently enough space for it and to improve
the code readability it's better to have it. This field is mostly used in the pointer math used to calculate the slot /
object memory address in `ffma_mem_free`.

The field `data_addr` points instead to the beginning of the slots, to `slots[0]`, and it's used as well in the pointer
math to calculate the slot / object index in `ffma_mem_free`. It's also worth to note that `data_addr`, for
performance reasons, is kept **always** page aligned.

The field `metrics.objects_total_count` is calculated using the following code
```c
size_t page_size = REGION_SIZE;
size_t usable_page_size = page_size - os_page_size - sizeof(ffma_slice_t);
size_t ffma_slot_size = sizeof(ffma_slot_t);
uint32_t item_size = ffma->object_size + ffma_slot_size;
uint32_t slots_count = (int)(usable_page_size / item_size);
```

that can be simplified to
```
(region size - page size - sizeof(ffma_slice_t)) / (object size + sizeof(ffma_slot_t))
```

Where `page size` is the size of page, usually 4kb, then `sizeof(ffma_slice_t)` is `64` bytes on 64bit architectures and
`sizeof(ffma_slot_t)` is `32` bytes. These last two structs are checked in the tests to ensure that the size matches the
expectation.

For `128` bytes objects a slice using the formula above `(2MB - 4Kb - 64) / (128 + 32)` can contain `13081` objects.

The `double_linked_list_item` is an item of the `ffma->slices` double linked list. As explained above, the
available slices are kept at the head meanwhile the in use ones at the tail.

Here an example of the memory layout of a slice

![FFMA - Memory layout](../images/ffma_2.png)

*(the schema hasn't been updated after renaming the memory allocator to FFMA)*

#### struct ffma_slot (ffma_slot_t)

```c
typedef union {
    double_linked_list_item_t double_linked_list_item;
    struct {
        void *padding[2];
        void *memptr;
#if DEBUG==1
        bool available:1;
        int32_t allocs:31;
        int32_t frees:31;
#else
        bool available;
#endif
    } data;
} ffma_slot_t;
```

This is the actual slot, a union with the struct representing the item of the double linked list at the beginning.

The field `memptr` contains the pointer to the memory, in the slice, assigned to this slot calculated as follows
```c
ffma_slot->data.memptr = (void*)(ffma_slice->data.data_addr + (index * ffma->object_size));
```

The field `available` is marked true on creation and gets marked false when allocated or back to true when freed.

The `double_linked_list_item` is an item of the `ffma->thread_metadata[i]->slots` double linked list. As
explained above, the available slices are kept at the head meanwhile the in use ones at the tail.

### Benchmarks

The benchmarks below have been generated on an EPYC 7551 with 256GB of RAM @2666MHz and on Ubuntu 22.04.2 LTS. 

![FFMA - Benchmarks](../images/ffma_3.png)

CPU information
```
$ cat /proc/cpuinfo
processor	: 0
vendor_id	: AuthenticAMD
cpu family	: 23
model		: 1
model name	: AMD EPYC 7551 32-Core Processor
stepping	: 2
microcode	: 0x800126e
cpu MHz		: 2000.000
cache size	: 512 KB
physical id	: 0
siblings	: 64
core id		: 0
cpu cores	: 32
apicid		: 0
initial apicid	: 0
fpu		: yes
fpu_exception	: yes
cpuid level	: 13
wp		: yes
flags		: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 ht syscall nx mmxext fxsr_opt pdpe1gb rdtscp lm constant_tsc rep_good nopl nonstop_tsc cpuid extd_apicid amd_dcm aperfmperf rapl pni pclmulqdq monitor ssse3 fma cx16 sse4_1 sse4_2 movbe popcnt aes xsave avx f16c rdrand lahf_lm cmp_legacy svm extapic cr8_legacy abm sse4a misalignsse 3dnowprefetch osvw skinit wdt tce topoext perfctr_core perfctr_nb bpext perfctr_llc mwaitx cpb hw_pstate ssbd ibpb vmmcall fsgsbase bmi1 avx2 smep bmi2 rdseed adx smap clflushopt sha_ni xsaveopt xsavec xgetbv1 xsaves clzero irperf xsaveerptr arat npt lbrv svm_lock nrip_save tsc_scale vmcb_clean flushbyasid decodeassists pausefilter pfthreshold avic v_vmsave_vmload vgif overflow_recov succor smca
bugs		: sysret_ss_attrs null_seg spectre_v1 spectre_v2 spec_store_bypass retbleed
bogomips	: 3999.53
TLB size	: 2560 4K pages
clflush size	: 64
cache_alignment	: 64
address sizes	: 48 bits physical, 48 bits virtual
power management: ts ttp tm hwpstate cpb eff_freq_ro [13] [14]
...
```

Memory information
```
$ sudo lshw -short -C memory
[sudo] password for daalbano: 
H/W path                      Device        Class          Description
======================================================================
...
/0/21/0                                     memory         32GiB DIMM DDR4 Synchronous Registered (Buffered) 2667 MHz (0.4 ns)
/0/21/1                                     memory         32GiB DIMM DDR4 Synchronous Registered (Buffered) 2667 MHz (0.4 ns)
/0/21/2                                     memory         32GiB DIMM DDR4 Synchronous Registered (Buffered) 2667 MHz (0.4 ns)
/0/21/3                                     memory         32GiB DIMM DDR4 Synchronous Registered (Buffered) 2667 MHz (0.4 ns)
/0/21/4                                     memory         32GiB DIMM DDR4 Synchronous Registered (Buffered) 2667 MHz (0.4 ns)
/0/21/5                                     memory         32GiB DIMM DDR4 Synchronous Registered (Buffered) 2667 MHz (0.4 ns)
/0/21/6                                     memory         32GiB DIMM DDR4 Synchronous Registered (Buffered) 2667 MHz (0.4 ns)
/0/21/7                                     memory         32GiB DIMM DDR4 Synchronous Registered (Buffered) 2667 MHz (0.4 ns)
...
```

Distribution information
```
$ lsb_release -a
No LSB modules are available.
Distributor ID:	Ubuntu
Description:	Ubuntu 22.04.2 LTS
Release:	22.04
Codename:	jammy
```

Packages information
```
$ dpkg -l | grep tcmalloc
ii  libtcmalloc-minimal4:amd64            2.9.1-0ubuntu3                          amd64        efficient thread-caching malloc
$ dpkg -l | grep jemalloc2
ii  libjemalloc2:amd64                    5.2.1-4ubuntu1                          amd64        general-purpose scalable concurrent malloc(3) implementation
$ dpkg -l | grep libc6:
ii  libc6:amd64                           2.35-0ubuntu3.1                         amd64        GNU C Library: Shared libraries
$ cd cachegrand/3rdparty/mimalloc && git log --oneline HEAD~1..HEAD
28cf67e5 (HEAD, tag: v2.0.9) bump version to 2.0.9
```

[1]: https://en.wikipedia.org/wiki/Slab_allocation
