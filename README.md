cachegrand
==========

cachegrand aims to be a general purpose, concurrent, distributed, almost lock-free caching system.

To be able to achieve these goals, cachegrand implements a set of components specifically tailored to provide the
performance and scale up on multi-core and multi-cpu (NUMA) servers capable of running 64bit software including the
most recent SOC platforms able to run 64bit software as well (ie. the Raspberry PI 4 with 64bit kernel).

While being similar to other platforms, like memcache or redis, there are two distinctive factors that make cachegrand
unique:
- it uses a parallel almost-lock-free and almost-atomic-free hashtable, load/store memory fences are used wherever it's
  possible to achieve better performances;
- it's natively multi-threaded, like memcache, and thanks to the patterns put in place scales vertically very welll
- It has been built with modern technologies in mind, it supports on-disk storage to take advantage of using NVME flash
  disks bringing down the costs to a fraction without losing performances.

As mentioned above, the data internally are backed by a modern designed hashtable:
- first-arrived first-served pattern is not needed to be guaranteed, it's used by a multi-threaded network server where
  there are a number of factors affecting the order of the requests, special commands (like for redis or memcache) have
  be used if the order of the operations is relevant (ie. incrementing counters), the set and delete operations are
  always serialized on a key but the get operations are not being blocked;
- the hashtable buckets are split in chunks of 14 slots (number chosen to be cache-aligned, no reference to F14) and
  each chunk has a localized spinlock to guarantee that high-contented chunks hit perform less operations;
- it takes advantage of memory fencing to guarantee that the get operation can operate independently from the set and
  delete ones, no locks or atomic operations when reading;
- uses the t1ha2 hashing algorithm to provide very high performances with a fairly distribution;
- to improve the speed uses power of twos for the hashtable size;
- fully takes advantage of the L1/L2 and L3 caches to minimize accessing the main memory when searching for an hash;
- take advantage of branch-less AVX2 and AVX hash search implementation with detection at runtime if supported by the
  hardware;
- some speed-critical parts are recompiled with multiple different optimization and the best one is chosen at runtime;
- if the keys are short enough, it uses key-inlining, it avoids to jump to another memory location to perform the 
  comparison;
- minimize the effects of the "false-sharing" caused by multiple threads trying to change data that are stored in
  cache-lines held by different hardware cores;
- uses a DOD (Data Oriented Design) keeping the hashes and keys/values data structures separated to be able to achieve
  what above mentioned;

### DOCS

It's possible to find some documentation in the docs folder, keep in mind that it's a very dynamic projects and the
written documentation is more a general reference.

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

cachegrand is still under heavy development!

The goal is to implement a PoC for the v0.1 and a basic working caching server with by the v0.3.

Here the general grand-plan:
- v0.1 
    - [x] Hashtable
        - [x] Ops
            - [x] GET, lock-free, atomic-free and wait-free
            - [x] SET, chunk-based (14 slots) locking using spin-locks
            - [x] DELETE, chunk-based (14 slots) locking using spin-locks
    - [ ] Networking
        - [x] Implement a network stack able to support multiple io libraries and multiple protocols 
        - [x] Implement a network io layer based on io_uring nad liburing
        - [x] Implement a network channel layer based on top of the network io iouring layer
        - [x] Implement network workers
            - [x] Implement an io_uring-based network worker
        - [x] Implement a redis protocol reader and writer (with pipelining support)
        - [ ] Implement a basic support for the redis protocol
            - [ ] GET
            - [ ] SET
            - [ ] DELETE
            - [ ] HELLO
            - [ ] QUIT
            - [x] PING
    - [ ] Memory Management
        - [ ] Implement a SLAB allocator
    - [ ] Configuration
        - [ ] Implement a YAML based configuration
    - [ ] Storage
        - [ ] Implement storage workers
            - [ ] Implement a storage worker based on io_uring and liburing

- v0.2
    - [ ] Logging
        - [ ] Add the ability to perform multi-threaded logging via a ring buffer per thread processed by the logger
              thread (if too many messages are submitted the caller has to wait for space in the ring).
        - [ ] Add logging to disk sink
    - [ ] Hashtable
        - [ ] Implement adaptive spinlocks
        - [ ] Implement a sliding spinlock window to release locked chunks in advance if possible
        - [ ] Add support for LRU
            - [ ] Implement a separate LRU structure to hold the necessary data for the hashtable
            - [ ] Implement an LRU linked-list
            - [ ] Implement an LRU Promote and GC worker
        - [ ] Ops
            - [ ] RESIZE
            - [ ] ITERATE
            - [ ] DELETE, when deleting move back the far-est key of the chunk usind the distance
    - [ ] Networking
        - [ ] Add support for workers that do not rely on SO_REUSEPORT (it's Linux only and performance costly)
        - [ ] Switch to use the SLAB allocator
    - [ ] Storage:
        - [ ] Implement garbage collection

- v0.3
    - [ ] Memory Management
        - [ ] Add NUMA support
    - [ ] Hashtable
        - [ ] Add NUMA support
    - [ ] Write documentation
    - [ ] Storage
        - [ ] Optmize for NVMEs (xnvme, LSMTrees?)
    - [ ] Networking
        - [ ] Add support for multiple protocols
        - [ ] Add a protobuf-based rpc based protocol
        - [ ] Implement a basic http webserver to provide general stats
        - [ ] Implement a basic http webserver to provide simple CRUD operations 

- v0.4
    - [ ] Add AARCH64 support
    - [ ] Storage
        - [ ] Add support for epoll
    - [ ] Networking
        - [ ] Add support for epoll

- v0.5
    - [ ] Authentication
    - [ ] Networking
        - [ ] Add TLS support (ktls? mbedtls? openssl?)
            - [ ] Connection encryption
            - [ ] Client authentication

- v0.6
    - [ ] Storage
        - [ ] Add transparent data encryption / decryption

- v0.7
    - [ ] Networking
        - [ ] Implement an XDP-based network worker

- Somewhere before the v1.0
    - [ ] Add support for Windows (IO via support IOCP)
    - [ ] Add support for Mac OS X (IO via epoll)
    - [ ] Add support for FreeBSD (IO via kqueue/kevent)
