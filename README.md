[![Build & Test](https://github.com/danielealbano/cachegrand/workflows/Build%20&%20Test/badge.svg)](https://github.com/danielealbano/cachegrand/actions?query=branch%3Amaster+workflow%3A%22Build+%26+Test%22)
[![Code Coverage](https://img.shields.io/codecov/c/gh/danielealbano/cachegrand?label=code%20coverage)](https://codecov.io/gh/danielealbano/cachegrand)

cachegrand
==========

cachegrand aims to be a general purpose, concurrent, distributed, lock-free, in-memory and on-disk caching system.

To be able to achieve these goals, cachegrand implements a set of components specifically tailored to provide the
performance and scale up on multi-core and multi-cpu (NUMA) servers capable of running 64bit software, taking advantage
of functionality exposed by the more recent hardware without excluding SOC platforms (ie. the Raspberry PI 4 with 64bit
kernel).

While being similar to other platforms, like memcache or redis, there are two distinctive factors that make cachegrande
unique:
- it uses a concurrent lock-free and almost atomic-free hashtable, load/store memory fences are used wherever it's
  possible to prove better performances;
- it's natively multi-threaded, like memcache, and thanks to the patterns put in place scales vertically very welll
- It has been built with modern technologies in mind, it supports on-disk storage to take advantage of using NVME flash
  disks bringing down the costs to a fraction without losing performances.

As mentioned above, the data internally are backed by a modern designed hashtable:
- first-arrived first-served pattern is not needed to be guaranteed, it's used by a multi-threaded network server where
  there are a number of factors affecting the order of the requests, special commands (like for redis or memcache) have
  be used if the order of the operations is relevant (ie. incrementing counters);
- it's lock-free and almost atomic free, as mentioned above it takes advantage of memory fencing to ensure that the cpu
  doesn't execute the operations in an order that would cause the algorithm to fail;
- uses the t1ha2 hashing algorithm to provide very high performances with a fairly distribution;
- to improve the hashes distribution the hashtable uses prime numbers for its size, apart from the initial value of **42**
  buckets;
- fully takes advantage of the L1/L2 and L3 caches to minimize accessing the main memory when searching for a key;
- minimize the effects of the "false-sharing" caused by multiple threads trying to change data that are stored in
  cachelines held by different hardware cores;
- uses a DOD (Data Oriented Design) and a neighborhood approach when searching for the buckets, the neighborhood is
  automatically sized to guarantee a average max load factor of 0.75 but these setting can be easily changed and tuned
  for the specific workload if needed.

### DOCS

It's possible to find some documentation in the docs folder, keep in mind that it's a very dynamic projects and the
written documentation is more a general reference than a detailed pinpointing of the functionalities and implementations.

### HOW TO

#### Checkout

```bash
git clone https://github.com/danielealbano/cachegrand.git
git submodule update --init --recursive
```

#### Build - Requirements

For more information about the build requirements check [docs/build-requirements.md](docs/build-requirements.md).

#### Build

```bash
mkdir cmake-build-debug
cd cmake-build-debug
cmake .. -DUSE_HASHTABLE_HASH_ALGORITHM_T1HA2=1
make cachegrand
```



#### Run tests
```bash
mkdir cmake-build-debug
cd cmake-build-debug
cmake .. -DUSE_HASHTABLE_HASH_ALGORITHM_T1HA2=1 -DBUILD_TESTS=1
make cachegrand-tests
make test
```

#### Run benchmarks

Before compiling and running the benchmarks it's currently necessary to revise the amount of threads the benchmark will
spawn for the testing, they are defined per set of operations and currently the only benchmark taking advantage of the
hashtable concurrency are implemented in [benches/bench-hashtable-op-set.cpp](benches/bench-hashtable-op-set.cpp).

The benchmarking is built on top of the google-benchmark library that gets automatically downloaded and compiled by
the **cachegrand-benches** target.

```bash
mkdir cmake-build-debug
cd cmake-build-debug
cmake .. -DUSE_HASHTABLE_HASH_ALGORITHM_T1HA2=1 -BUILD_INTERNAL_BENCHES=1
make cachegrand-benches
./benches/cachegrand-benches
```

### TODO

cachegrand is still under heavy development, the goals for the 0.1 milestone are the following:
- implement the lock-free, fixed queue, auto-scalable threadpool;
- implement the data backing layer, with per-thread sharding in-memory and on-disk: 
    - preadv/pwritev/splice;
    - append only;
    - block-based with a variable block size;
    - uses an LSMTree approach where the first ring is always kept in memory and when it grows it's flushed to the disk,
      data are written directly to the disk if bigger then the allowed threshold to be NVME friendly;  
- implement the network layer:
    - plain epoll, read/write, sendfile, socket sharded per thread;
    - threadpool to manage the network, each thread in the pool will manage it's own backlog, socket set and will have
      it's own slab allocator;
    - use simple array to store the data structures, better to use more memory and spawn a new thread if really needed
      that doing atomic operations continuously;
- implement a basic support for the redis protocol for the GET and the SET operations;
- write documentation coverage;
- improve code testing & coverage. 

### FUTURE TODO

Because it aims to be a modern platform, the goal is to have the features mentioned below by the time of it's first
stable release:
- Add support for Windows and Mac OS X, the code is already being written with this goal in mind;
- Commands batching, to be able to perform multiple set of operations at the same time;
- TLS, to encrypt the data on-transit;
- Authentication and ACLs, to limit who has access and which data are accessible;
- On-memory and On-disk data encryption;
- Multi database.
