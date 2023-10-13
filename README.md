[![License](https://img.shields.io/github/license/danielealbano/cachegrand)](https://github.com/danielealbano/cachegrand/blob/main/LICENSE)
[![Build & Test](https://github.com/danielealbano/cachegrand/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/danielealbano/cachegrand/actions/workflows/build_and_test.yml)
[![codecov](https://codecov.io/gh/danielealbano/cachegrand/branch/main/graph/badge.svg?token=H4W0N0F7MT)](https://codecov.io/gh/danielealbano/cachegrand)
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fdanielealbano%2Fcachegrand.svg?type=shield)](https://app.fossa.com/projects/git%2Bgithub.com%2Fdanielealbano%2Fcachegrand?ref=badge_shield)

<p align="center">
  <a href="https://cachegrand.io">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/danielealbano/cachegrand/main/docs/images/logo-dark.png">
      <img alt="cachegrand logo" src="https://raw.githubusercontent.com/danielealbano/cachegrand/main/docs/images/logo-light.png">
    </picture>
  </a>
</p>

### Table of Content

- [What is cachegrand?](#what-is-cachegrand)
- [Benchmarks](#benchmarks)
- [Quick Start](#quick-start)
- [Configuration](#configuration)
- [Running cachegrand](#running-cachegrand)
- [License](#license)
- [Contributing](#contributing)

### What is cachegrand?

cachegrand is a high-performance key-value store designed to optimize performance on modern hardware. It offers
efficient data handling, processing, and retrieval capabilities through standard protocols and APIs. With its impressive
scalability, cachegrand can handle millions of operations per second, all completed in less than a millisecond.

Underneath its robust architecture, cachegrand employs its own database engine that seamlessly works with various data
types to to enable a versatile data ingestion and retrieval. Additionally, cachegrand supports WebAssembly, empowering
server-side data processing.

One of cachegrand's notable features is its compatibility with widely adopted protocols and interfaces, including Kafka,
Redis, and Memcache. This compatibility allows developers to leverage their existing familiarity with SDKs and
frameworks.

cachegrand is still in development and some of the features are still in the works, but it's already possible to use it
as a high-performance key-value store with the Redis protocol.

#### Use cases

cachegrand simplifies real-time operations by providing an integrated solution, eliminating the need for stitching
together multiple technologies. It offers a streamlined approach to manage and process data, enabling efficient
real-time scenarios.

With cachegrand, you can:

- Ingest data using familiar streaming methods like Kafka or Redis.
- Process data using WebAssembly-compatible languages like Python or Rust.
- Leverage the internal database to store intermediate and final results.
- Deliver processed data using supported protocols such as Kafka, Redis, HTTP(s), or Memcached.

But it's also possible just to use single components of cachegrand, like the Key-Value store via the Redis protocol,
or the Kafka protocol, both with or without the WebAssembly support, depending on your needs.

#### Implemented Key features

Currently cachegrand supports only the Redis interface to interact with the Key-Value store, support for the Kafka
interface and the WebAssembly support are still in the works.

- [Redis](https://github.com/danielealbano/cachegrand/blob/main/docs/architecture/modules/redis.md) protocol support
- [Prometheus](https://github.com/danielealbano/cachegrand/blob/main/docs/architecture/modules/prometheus.md) endpoint
  for monitoring
- Blazing fast [hashtable](https://github.com/danielealbano/cachegrand/blob/main/docs/architecture/hashtable.md) capable
  of digesting 2.1 billion records per second on a 1x AMD EPYC 7502
- [Scales vertically](#benchmarks), 2x cpus means ~2x requests
- In-memory and on-disk storage
- Supports different collection data types (Streams, Lists, Hashsets, Sorted Sets, etc.) (WIP)

#### Planned Key Features

- More modules for additional platforms compatibility, e.g. Kafka, Memcache, HTTPS, AWS S3, DataDog, etc.
- And ad-hoc UDP message-based (Homa-like) network stack based on Linux XDP (eXpress Data Path)
- [WebAssembly](https://github.com/danielealbano/cachegrand/blob/main/docs/architecture/webassembly.md) to provide
  [User Defined Functions](https://github.com/danielealbano/cachegrand/blob/main/docs/architecture/webassembly/user-defined-functions.md),
  [event hooks](https://github.com/danielealbano/cachegrand/blob/main/docs/architecture/webassembly/event-hooks.md),
  implement [modules](https://github.com/danielealbano/cachegrand/blob/main/docs/architecture/webassembly.md#modules),
  you can use your preferred language to perform operations server side
- Automatic load balancing, Replication groups, Eventual Consistency

It's possible to find more information in the [docs'](https://github.com/danielealbano/cachegrand/blob/main/docs/)
folder.

cachegrand runs on Linux on x86-64 (Intel and AMD) and aarch64 (ARMv8, e.g. Raspberry PI 4, Orange PI 5, etc.), we are
planning to port it to more hardware (e.g. RISC) once will become more feature complete.

### Benchmarks

#### Internal Key-Value stor via the Redis interface

The benchmarks are regularly carried out on an **AMD EPYC 7502P** with **2 x 25Gbit** network links using
**Ubuntu 22.04** and two other servers, with the same hardware, to generate load using memtier_benchmark.

<p align="center">
  <img alt="GET/SET latest benchmarks" width="48%" src="https://github.com/cachegrand/cachegrand-benchmarks/raw/main/images/latest-benchmarks-get-set.png">
  &nbsp;
  <img alt="GET/SET with batching latest benchmarks" width="48%" src="https://github.com/cachegrand/cachegrand-benchmarks/raw/main/images/latest-benchmarks-get-set-pipelining.png">
</p>

_[Benchmarks source](https://github.com/cachegrand/cachegrand-benchmarks)_

### Quick Start

Simply run

```shell
docker run \
  --ulimit memlock=-1:-1 \
  --ulimit nofile=262144:262144 \
  -p 6379:6379 \
  -p 6380:6380 \
  -p 9090:9090 \
  -it \
  --rm \
  cachegrand/cachegrand-server:latest
```

it comes with a default config file with Redis on port 6379, TLS-enabled Redis on port 6380 and Prometheus on port 9090
with the metrics endpoint on `/metrics`.

The certificate will be generated on each start, to use an ad-hoc SSL certificate, instead of the auto-generated one,
it's possible to mount the required certificate and key using the following command

```shell
docker run \
  -v /path/to/certificate.pem:/etc/cachegrand/cachegrand.pem \
  -v /path/to/certificate.key:/etc/cachegrand/cachegrand.key \
  --ulimit memlock=-1:-1 \
  --ulimit nofile=262144:262144 \
  -p 6379:6379 \
  -p 6380:6380 \
  -p 9090:9090 \
  -it \
  --rm \
  cachegrand/cachegrand-server:latest
```

if you want to use a custom configuration, you can download the default configuration file using the following command

```shell
curl https://raw.githubusercontent.com/danielealbano/cachegrand/main/etc/cachegrand.yaml.skel -o /path/to/cachegrand.yaml
```

Edit it and then use it with the cachegrand's docker image using the following command

```shell
docker run \
  -v /path/to/cachegrand.yaml:/etc/cachegrand/cachegrand.yaml \
  --ulimit memlock=-1:-1 \
  --ulimit nofile=262144:262144 \
  -p 6379:6379 \
  -p 6380:6380 \
  -p 9090:9090 \
  -it \
  --rm \
  cachegrand/cachegrand-server:latest
```

### Configuration

cachegrand comes with a default configuration but for production use please review the
[documentation](https://github.com/danielealbano/cachegrand/blob/main/docs/configuration.md) to ensure an optimal deployment.

### Running cachegrand

#### Build from source

Instructions on how to build cachegrand from the sources are available in the
[documentation](https://github.com/danielealbano/cachegrand/blob/main/docs/build-from-source.md)

cachegrand doesn't need to run as root but please review the configuration section to ensure that enough lockable memory
has been allowed, enough files can be opened and that the slab allocator has been enabled and enough huge pages have been provided

Before trying to start cachegrand, take a look to the
[performance tips](https://github.com/danielealbano/cachegrand/blob/main/docs/performance-tips.md) available in the
docs' section as they might provide a valuable help!

#### Help

```shell
$ ./cachegrand-server --help
Usage: cachegrand-server [OPTION...]

  -c, --config-file=FILE     Config file (default config file
                             /usr/local/etc/cachegrand/cachegrand.conf )
  -l, --log-level=LOG LEVEL  log level (error, warning, info, verbose, debug)
  -?, --help                 Give this help list
      --usage                Give a short usage message

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```

#### Start it locally

Once cachegrand has been [built from the sources](#build-from-source), it's possible to run it with the following command

```shell
/path/to/cachegrand-server -c /path/to/cachegrand.yaml.skel
[2023-04-08T18:25:54Z][INFO       ][program_startup_report] cachegrand-server version v0.3.0 (built on 2023-05-08T18:19:31Z)
[2023-04-08T18:25:54Z][INFO       ][program_startup_report] > Release build, compiled using gcc v11.3.0
[2023-04-08T18:25:54Z][INFO       ][program_startup_report] > Hashing algorithm in use t1ha2
[2023-04-08T18:25:54Z][INFO       ][program_startup_report] > Running on Linux bf2b94ff7fa5 5.19.0-38-generic #39-Ubuntu SMP PREEMPT_DYNAMIC Fri Mar 17 17:33:16 UTC 2023 x86_64
[2023-04-08T18:25:54Z][INFO       ][program_startup_report] > Memory: 128722 MB total, 1955 MB swap total
[2023-04-08T18:25:54Z][INFO       ][program_startup_report] > TLS: mbed TLS 2.28.0 (kernel offloading enabled)
[2023-04-08T18:25:54Z][INFO       ][program_startup_report] > Realtime clock source <POSIX>, resolution <4 ms>
[2023-04-08T18:25:54Z][INFO       ][program_startup_report] > Monotonic clock source <Hardware (TSC)> (estimated cpu cycles per second <4.20 GHz>), resolution <1 ms>
[2023-04-08T18:25:54Z][INFO       ][config] Loading the configuration from /etc/cachegrand/cachegrand.yaml
[2023-04-08T18:25:54Z][INFO       ][program] Starting <32> workers
[2023-04-08T18:25:54Z][INFO       ][worker][id: 00][cpu: 00][module_redis_snapshot_load] Snapshot file </var/lib/cachegrand/dump.rdb> does not exist
[2023-04-08T18:25:55Z][INFO       ][program] Ready to accept connections
```

### License

cachegrand is Open Source and licensed under the
[BSD 3-Clause License](https://github.com/danielealbano/cachegrand/blob/main/LICENSE), all the files under the
repository are licensed under the same license unless otherwise specified (for example, but not limited, via a README or
a LICENSE in a sub folder or as header in the source files).

cachegrand uses a number of different components all licensed under compatible licenses but if you spot any that is not
compatible with the BSD 3-Clause License please open an issue, we will be happy to fix it promptly!

### Contributing

Please if you find any bug, malfunction or regression feel free to open an issue or to fork the repository and submit
your PRs! If you do open an Issue for a crash, if possible please enable sentry.io in the configuration file and try to
reproduce the crash, a minidump will be automatically uploaded on sentry.io.

Also, if you have built cachegrand from the source, please attach the compiled binary to the issue as well as sentry.io
knows nothing of your own compiled binaries.
