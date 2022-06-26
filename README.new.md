[![Build & Test](https://github.com/danielealbano/cachegrand/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/danielealbano/cachegrand/actions/workflows/build_and_test.yml) [![codecov](https://codecov.io/gh/danielealbano/cachegrand/branch/main/graph/badge.svg?token=H4W0N0F7MT)](https://codecov.io/gh/danielealbano/cachegrand)

cachegrand
==========

cachegrand is an open-source fast, scalable and modular Key-Value store designed from the ground
up to take advantage of modern hardware, able to provide better performance when compared to
similar projects but also capable to provide a great flexibility in termis of management and
developer experience.

Key features:
- [Modular](docs/architecture/modules.md) architecture to support widely used protocols, e.g.
  [Redis](docs/architecture/modules/redis.md), [Kafka](docs/architecture/kafka.md), etc., or
  extend the base functionalities, e.g.
  [Prometheus](docs/architecture/modules/modules/prometheus.md),
  [DataDog](docs/architecture/modules/modules/datadog.md), etc..
- [Write-Ahead-Log auto-sharding database](docs/architecture/wal-db.md) for fast data writes and
  retrival with primitives built to handle different data types (e.g. small strings, large blobs,
  jsons, etc.)
- [Hashtable](docs/architecture/hashtable.md) GET Lock-free and Wait-free operations, SET and
  DELETE use localized spinlocks, the implementation is capable to digest 2.1 billion records
  per second on a 1x AMD EPYC 7502 (see
  [benches](docs/benchmarks/hashtable.md))
- Linear vertical scalability when using the in-memory database, 2x cpus means 2x requests (see
  [benches](docs/benchmarks/linear-vertical-scalability.md))
- [Built for flash memories](docs/architecture/wal-db.md#flash-memories), able to saturate IOPS
  in modern DC NVMEs and SSDs
- [WebAssembly](docs/architecture/webassembly.md) to provide AOT-compiled
  [User Defined Functions](docs/architecture/webassembly/user-defined-functions.md),
  [event hooks](docs/architecture/webassembly/event-hooks.md), implement
  [modules](docs/architecture/webassembly.md#modules), you can use your preferred language to
  perform operations server side
- [Replication groups](docs/architecture/replication.md#replication-groups) and
  [replica tags](docs/architecture/replication.md#replica-tags), tag data client side or use server
  side events to tag the
  data and determine how they will be replicated;
- [Active-Active](docs/architecture/replication.md#active-active)
  [last-write-wins](docs/architecture/replication.md#last-write-wins) data replication, it's a
  cache, write to any node of a replication group to which the replication tags are assigned,
  no need to think worry it;

It's possible to find more information in the [documentation](docs/readme.md).

The platform is written in C, validated via more than 1000 tests and Valgrind, which is natively
integrated, and built with a set of compiler options to fortify the builds.

Currently it runs only on Linux and requires a Kernel 5.8 or newer, will be ported to other
platforms once will become more feature complete.

*cachegrand is not production ready nor feature complete, the documention is being re-written as
well, please don't open issues for these. The status of the project is tracked via GitHub using
the project management board.*

### Issues & contributions

Please if you find any bug, malfunction or regression and you want to contribute feel free to fork,
code and submit your PRs!

### Performances

The platform is regularly benchmarked as part of the development process to ensure that no
regressions slip through, it's possibile to find more details in the
[documentation](docs/benchmarks.md).

### How to install

Packages are available for a number of different distributions.

#### Ubuntu

```
# TODO add source
apt install cachegrand-server
```

#### Debian

```
# TODO
apt install cachegrand-server
```

#### Fedora

```
# TODO
yum install cachegrand-server
```

#### Arch

```
# TODO
pkg install cachegrand-server
```

#### Docker

```
docker run cachegrand/cachegrand-server
```

#### Build from source

Instructions on how to built cachegrand from the sources are available in the
[documentation](docs/build-from-source.md)

### Configuration

cachegrand comes with a default configuration but for production use please review the [documentation](docs/configuration.md) to ensure an optimal deployment.
