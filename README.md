[![Build & Test](https://github.com/danielealbano/cachegrand/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/danielealbano/cachegrand/actions/workflows/build_and_test.yml) [![codecov](https://codecov.io/gh/danielealbano/cachegrand/branch/main/graph/badge.svg?token=H4W0N0F7MT)](https://codecov.io/gh/danielealbano/cachegrand)

cachegrand
==========

cachegrand is an open-source fast, scalable and secure Key-Value store able to act as Redis drop-in replacement, designed
from the ground up to take advantage of modern hardware vertical scalability, able to provide better performance and a
larger cache at lower cost, without losing focus on distributed systems.

To be able to achieve these goals, cachegrand implements a set of components tailored to provide the needed performance
and the ability to scale-up on multi-core and multi-cpu (NUMA) servers capable of running 64bit software including the
most recent ARM-based SoC platforms (e.g. AWS Graviton, Raspberry PI 4 with 64bit kernel, etc.).

Among the distinctive features:
- Uses a custom-tailored KeyValue store able to perform GET operations without using locks or wait-loops;
- The KeyValue store also takes advantage of SSE4.1/AVX/AVX2 if available;
- Threads are pinned to specific cores and use fibers bound to the thread to control what runs on which core, to take
  advantage of the cache-locality and being numa-aware;
- Uses a zero-copy approach whenever possible and implements sliding window (streams) algorithms to process the data;
- Relies on the newer io_uring kernel component to provide efficient network and disk io;
- Stores the data on the disks, instead of the memory, using a flash-friendly algorithm to minimize the COW domino
  effect.

Although it's still in heavy development and therefore the current version does not provide all the required
functionalities to use it, some benchmarking has been carried out:
- The in-memory hashtable is able to insert up to **2.1 billions** new keys per second on an **1 x AMD EPYC 7502P** and
  **192 GB RAM DDR4** using **2048 threads**;
- The networking layer has been tested under different conditions:
    - on a **1 x AMD EPYC 7502P** with a **2 x 25Gbps** links cachegrand was able to handle an almost fully saturated
      network bandwidth (**45gbit/s**) using 6 **1 x AMD EPYC 7402P** to generate load with **memtier_benchmark** with
      small payloads
    - on a **1 x AMD EPYC 7402P** with a **2 x 10Gbps** links cachegrand was able to handle up to 5 million GET/SET
      commands per second using three **1 x AMD EPYC 7402P** to generate load with **memtier_benchmark** 
    - on a **2 x Intel Xeon E5-2690 v4** with a **1 x 40Gbps** link cachegrand was able to handle about the same number
      of requests, about 4.5 million GET/SET commands per second and saturate the load

It's possible to find out more information in the benchmarks section.

### DOCS

It's possible to find some documentation in the docs' folder, keep in mind that it's a very dynamic projects and the
written documentation is a more general reference.

### HOW TO

#### Checkout

```bash
git clone https://github.com/danielealbano/cachegrand.git
cd cachegrand
git submodule update --init --recursive
```

#### Build - Requirements

For more information about the build requirements check [docs/build-requirements.md](docs/build-requirements.md).

#### Build

```bash
mkdir cmake-build-release
cd cmake-build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_HASH_ALGORITHM_T1HA2=1
make cachegrand-server
```

#### Run

To run cachegrand a number of settings may need to be tuned.

##### Max lockable memory and Opened files limit

cachegrand uses io_uring, a standard kernel component introduced with the kernel 5.1 and became stable for disk i/o
around 5.4 and for network i/o around 5.7, to handle both the storage and the network.

This component shares some memory between the user space process using it and the kernel side, it locks the shared
memory region to avoid swapping, otherwise the kernel may access some other data.

The maximum amount of lockable memory of a process or a user is managed via the standard limits on linux, it's
possible to use the `ulimit -a` command to check the value of `max locked memory`, a value of 65536 (65mb) or higher
should be set.

The `max_clients` is directly proportional to the amount of lockable memory required, setting it to a value too high
without having enough lockable memory may cause failures. Other softwares using lockable memory (notably gnome, kde,
firefox, chrome, electron-based software, softwares that use gpu acceleration, etc.) may be affected as well if all the
lockable memory is used as they wouldn't be able to allocate triggering crashes or unexpected behaviour (e.g.
gnome crashes, chrome and electron-based software freeze, etc.).
Especially in a desktop environment it's important to increase the lockable memory enough to accomodate for all the
required softwares and cachegrand, keep the `max_clients` value low enough or run cachegrand with a different user.

It is also important to have a value high enough for the `open files` as in Linux the network connections are counted
towards this limit as well, to manage 10000 connections the open files limit must be greater than 10000, it is
usually safe to set it to unlimited or to a very high value.

```text
$ ulimit -a
...
max locked memory       (kbytes, -l) 64
...
open files                      (-n) 1024
...
```

In this case, both the `max locked memory` and the `open files` limits are too low and need to be increased.

It's a common approach to use `ulimit` directly to change these limits as root but to set in a permanent manner check
the documentation of your distribution.

##### Hugepages

The SLAB Allocator in cachegrand currently relies on 2 MB hugepages, it's necessary to enable them otherwise it will
fail-back to a dummy malloc-based allocator that will perform extremely badly.

A simple way to enable them is to set /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages to the desired number of
hugepages to allocate, bear in mind that each page is 2 MB.

Here an example on how initialize 512 hugepages, 1 GB fo memory will be reserved.
```shell
echo 512 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
```

#### Run in docker

When using docker to run cachegrand or the cachegrand tests, it's necessary to
- enable IPv6, as it is used by the test suite
- allow enough lockable memory
- allow enough open files
- enable the hugepages

To enable IPv6 in docker it's enough to define the following two keys in the `/etc/docker/daemon.json` config file
```json 
{
  "ipv6": true,
  "fixed-cidr-v6": "2001:db8:1::/64"
}
```

To allow enough lockable memory and enough open files, two override files can be created for systemd, one for containerd
and one for the docker engine

Docker override config file `/etc/systemd/system/docker.service.d/override.conf`
```text
[Service]
LimitNOFILE=1048576
LimitMEMLOCK=167772160
```

containerd override config file `/etc/systemd/system/containerd.service.d/override.conf`
```text
[Service]
LimitNOFILE=1048576
LimitMEMLOCK=167772160
```

After having created them, it's necessary to reload the systemd configuration from the disk and reload/restart the
services
```shell
sudo systemctl daemon-reload
sudo systemctl restart containerd
sudo systemctl restart docker
```

The hugepages are a system-wide setting and once they are configured on the host machine no special settings are
necessary to use them in docker.

One the system has been configured it's possible to build the container using `docker build`
```shell
cd tools/docker/build && docker build --tag cachegrand-build-test:latest .
```

As cachegrand is still in development, the image is not available on the public registry.

#### Run tests

```bash
mkdir cmake-build-debug
cd cmake-build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_HASH_ALGORITHM_T1HA2=1 -DBUILD_TESTS=1
make cachegrand-tests
make test
```

The tests are verbose, a number of error messages are expected when running them as failing conditions are expressly
verified.

To run the tests in docker
```shell
docker run --volume "$PWD":/code cachegrand-build-test:latest /bin/bash -c "cd /code && mkdir cmake-build-debug && cd cmake-build-debug && cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_HASH_ALGORITHM_T1HA2=1 -DBUILD_TESTS=1 && make cachegrand-tests && make test"
```

#### Run benchmarks

Before compiling and running the benchmarks it's currently necessary to revise the amount of threads the benchmark will
spawn for the testing, they are defined per set of operations and currently the only benchmark taking advantage of the
hashtable concurrency is implemented in [benches/bench-hashtable-op-set.cpp](benches/bench-hashtable-op-set.cpp).

The benchmarking is built on top of the google-benchmark library that gets automatically downloaded and compiled by
the **cachegrand-benches** target.

```bash
mkdir cmake-build-release
cd cmake-build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_HASH_ALGORITHM_T1HA2=1 -DBUILD_INTERNAL_BENCHES=1
make cachegrand-benches
./benches/cachegrand-benches
```

It's strongly discouraged to run benchmarks into a docker container.

It's possible to find a setup script for the benchmarks in the following
[gist](https://gist.github.com/danielealbano/0c1547ff85c2c22438ce1a904f9dcafe), it's the script used to run the
benchmarking. In the following [gist](https://gist.github.com/danielealbano/0f994405ea6ba7a271172731c6cc5554) is instead
possible to find the typical **memtier_benchmark** commands used, they require the latest version from git.

#### Run the software

cachegrand is still under heavy development.

The goal is to implement a PoC for the v0.1 and a basic working caching server with by the v0.3.

Until the v0.1 will be reached cachegrand will be missing essential features to make it fully usable, although it's
already possible to run some benchmarks.

### TODO

General milestones grand-plan:
- v0.1 
    - [x] Hashtable
        - [x] Ops
            - [x] GET, lock-free, atomic-free and wait-free
            - [x] SET, chunk-based (14 slots) locking using spin-locks
            - [x] DELETE, chunk-based (14 slots) locking using spin-locks
    - [X] Networking
        - [X] Add support for multiple protocols
        - [x] Implement a network stack able to support multiple io libraries and multiple protocols 
        - [x] Implement a network io layer based on io_uring nad liburing
        - [x] Implement a network channel layer based on top of the network io iouring layer
        - [x] Implement network workers
            - [x] Implement an io_uring-based network worker
        - [x] Implement a redis protocol reader and writer (with pipelining support)
        - [X] Implement a basic support for the redis protocol
            - [X] GET (basic)
            - [X] SET
            - [X] DEL
            - [X] HELLO
            - [X] QUIT
            - [x] PING
    - [X] Memory Management
        - [X] Implement a SLAB allocator
        - [X] Add NUMA support
        - [X] Implement a malloc-based fallback for the SLAB Allocator
    - [X] Configuration
        - [X] Implement a YAML based configuration
        - [X] Implement YAML schema validation
        - [X] Implement an argument parser to support basic settings via command line
        - [X] Update the implement code to rely on the dynamic configuration instead of the hardcoded settings
    - [X] Logging
        - [X] Add logging to disk sink
    - [ ] Storage (WIP)
        - [ ] Implement storage workers (WIP)
            - [ ] Implement a memory-based storage worker (WIP)
            - [ ] Implement a disk-based storage worker based on io_uring and liburing
    
- v0.2
    - [ ] General
        - [ ] Switch to fibers (implement light swapcontext, no need for signals or SIMD registers support)
    - [ ] Logging
        - [ ] Add the ability to perform multi-threaded logging via a double ring buffer per thread processed by the
              logger thread (if too many messages are submitted the caller has to wait for space in the ring).
    - [ ] Hashtable
        - [ ] Test-out adaptive spinlocks
        - [ ] Add support for data eviction policies
        - [ ] Ops
            - [ ] RESIZE
            - [ ] ITERATE
            - [ ] DELETE, when deleting move back the far-est key of the chunk using the distance
    - [ ] Networking
        - [ ] Add support for workers that do not rely on SO_REUSEPORT (it's Linux only and performance costly)
        - [ ] Switch to use the SLAB allocator
        - [ ] Extend redis protocol support
            - [ ] APPEND, SETNX, INCR, INCRBY, INCRBYFLOAT, DECR, DECRBY
            - [ ] RPUSH, RPUSHX, LPUSH, LINDEX, LLEN, LPOP, LSET, RPOP
    - [ ] Storage:
        - [ ] Implement garbage collection

- v0.3
    - [ ] General
        - [ ] Switch to use co-routines
    - [ ] Hashtable
        - [ ] Add NUMA support (set_mempolicy for mmap)
    - [ ] Write documentation
    - [ ] Storage
        - [ ] General optimization for flash (NVME and SSD) disks
    - [ ] Networking
        - [ ] Extend redis protocol support
            - [ ] SADD, SREM, SISMEMBER, SMEMBERS, SMOVE
            - [ ] HGET, HGETALL, HSET, HMSET, HDEL, HEXISTS, HKEYS, HLEN, HVALS
            - [ ] EXPIRE, TTL
        - [ ] Implement a basic http webserver to provide general stats
        - [ ] Implement a basic http webserver to provide simple CRUD operations 

- v0.4
    - [ ] Add AARCH64 support
        - [ ] Switch to a AARCH64-optimized hash function
        - [ ] Drop or replace x86/x64 intrinsic usage
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
        - [ ] Add a protobuf-based rpc based protocol
        - [ ] Implement an XDP-based network worker

- Somewhere before the v1.0
    - [ ] Rework the SLAB Allocator to not be dependent on hugepages and support multiple platforms
    - [ ] ACLs for commands / data access
        - [ ] Redis protocol support
        - [ ] HTTPS protocol support
    - [ ] Add support for Mac OS X
        - [ ] IO via kqueue/kevent
    - [ ] Add support for FreeBSD
        - [ ] IO via kqueue/kevent
