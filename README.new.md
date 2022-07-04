[![Build & Test](https://github.com/danielealbano/cachegrand/actions/workflows/build_and_test.yml/badge.svg)](https://github.com/danielealbano/cachegrand/actions/workflows/build_and_test.yml) [![codecov](https://codecov.io/gh/danielealbano/cachegrand/branch/main/graph/badge.svg?token=H4W0N0F7MT)](https://codecov.io/gh/danielealbano/cachegrand)

cachegrand
==========

cachegrand is an open-source fast, scalable and modular Key-Value store designed from the ground up to take advantage of
modern hardware, able to provide better performance when compared to similar projects but also capable to provide a
great flexibility in terms of management and developer experience.

Key features:
- [Modular](docs/architecture/modules.md) architecture to support widely used protocols, e.g.
  [Redis](docs/architecture/modules/redis.md) (in progress), [Memcache](docs/architecture/modules/memcache.md) (todo),
- etc., or add support for monitoring, e.g. [Prometheus](docs/architecture/modules/modules/prometheus.md) (todo),
  [DataDog](docs/architecture/modules/modules/datadog.md) (todo), etc..
- [Time-series database](docs/architecture/timeseries-db.md) (in progress) for fast data writes and retrieval with
  primitives built to handle different data types (e.g. small strings, large blobs, jsons, etc.)
- [Hashtable](docs/architecture/hashtable.md) GET Lock-free and Wait-free operations, SET and DELETE use
  localized spinlocks, the implementation is capable to digest 2.1 billion records per second on a 1x AMD EPYC 7502 (see
  [benches](docs/benchmarks/hashtable.md))
- Linear vertical scalability when using the in-memory database, 2x cpus means 2x requests (see
  [benches](docs/benchmarks/linear-vertical-scalability.md))
- [Built for flash memories](docs/architecture/timeseries-db.md#flash-memories) (in progress), able to saturate IOPS
  in modern DC NVMEs and SSDs
- [WebAssembly](docs/architecture/webassembly.md) (todo) to provide AOT-compiled
  [User Defined Functions](docs/architecture/webassembly/user-defined-functions.md) (todo),
  [event hooks](docs/architecture/webassembly/event-hooks.md) (todo), implement
  [modules](docs/architecture/webassembly.md#modules) (todo), you can use your preferred language to perform operations
  server side
- [Replication groups](docs/architecture/replication.md#replication-groups) (todo) and
  [replica tags](docs/architecture/replication.md#replica-tags) (todo), tag data client side or use server side events
  to tag the data and determine how they will be replicated;
- [Active-Active](docs/architecture/replication.md#active-active) (todo)
  [last-write-wins](docs/architecture/replication.md#last-write-wins) (todo) data replication, it's a cache, write to
  any node of a replication group to which the replication tags are assigned, no need to think worry it;

It's possible to find more information in the [documentation](docs/readme.md).

The platform is written in C, validated via unit tests, Valgrind and integration tests, it's also built with a set of
compiler options to fortify the builds ([#85](https://github.com/danielealbano/cachegrand/issues/85) (in progress)).

Currently, it runs only on Linux, on Intel or AMD cpus and requires a kernel v5.7 or newer, will be ported to other
platforms once will become more feature complete.

#### Please be aware that

cachegrand is not production ready and not feature complete, plenty of basic functionalities are being implemented,
the documentation is lacking as well as it's being re-written, please don't open issues for missing documentation.

The status of the project is tracked via GitHub using the project management board.

### Issues & contributions

Please if you find any bug, malfunction or regression feel free to open an issue or to fork the repository and submit
your PRs! If you do open an Issue for a crash, if possible please enable sentry.io in the configuration file and try to
reproduce the crash, a minidump will be automatically uploaded on sentry.io.
Also, if you have built cachegrand from the source, please attach the compiled binary to the issue as well as sentry.io
knows nothing of your own compiled binaries.

### Performances

The platform is regularly benchmarked as part of the development process to ensure that no regressions slip through,
it's possibile to find more details in the [documentation](docs/benchmarks.md).

### How to install

Packages are currently not available, they are planned to be created for the v0.3 milestone.

#### Build from source

Instructions on how to build cachegrand from the sources are available in the
[documentation](docs/build-from-source.md)

### Configuration

cachegrand comes with a default configuration but for production use please review the
[documentation](docs/configuration.md) to ensure an optimal deployment.

