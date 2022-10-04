Build from source
=================

cachegrand requires gcc and cmake to be built and have only a few external dependencies.

cachegrand also requires a kernel v5.7, you can check the current version of your kernel using
```shell
uname -r
```

Example output
```
5.10.15-051015-generic
```

## Required packages

| Package       | Min. Version |               |
|---------------|--------------|---------------|
| pkg-config    |              | **mandatory** |
| libnuma       | \>= 1.1      | **mandatory** |
| libyaml       | \>= 1.1      | **mandatory** |
| libmbedtls    | \>= 2.28     | **mandatory** |
| libatomic1    |              | **mandatory** |
| openssl       | \>= 2.0      | **mandatory** |
| curl          | \>= 7.0      | **mandatory** |
| libpcre2      |              | **mandatory** |
| libjson-c-dev |              | **mandatory** |

## Install the required packages

### Ubuntu 22.04

`libatomic1` is not included directly as it's as dependency of `libatomic` which is a dependency itself of 
`build-essential`.

```shell
sudo apt install \
    build-essential cmake pkg-config git \
    libnuma1 libnuma-dev \
    libcurl4-openssl-dev libcurl4 \
    libyaml-0-2 libyaml-dev \
    libmbedtls-dev libmbedtls14 \
    libpcre2-8-0 libpcre2-dev \
    libjson-c-dev
```

### Ubuntu 20.04

`libatomic1` is not included directly as it's as dependency of `libatomic` which is a dependency itself of
`build-essential`.

```shell
sudo apt install \
    build-essential cmake pkg-config git \
    libnuma1 libnuma-dev \
    libcurl4-openssl-dev libcurl4 \
    libyaml-0-2 libyaml-dev \
    libmbedtls-dev libmbedtls12 \
    libpcre2-8-0 libpcre2-dev \
    libjson-c-dev
```

### Debian 11

`libatomic1` is not included directly as it's as dependency of `libatomic` which is a dependency itself of
`build-essential`.

```shell
sudo apt install \
    build-essential cmake pkg-config git \
    libnuma1 libnuma-dev \
    libcurl4-openssl-dev libcurl4 \
    libyaml-0-2 libyaml-dev \
    libmbedtls-dev libmbedtls12
```

### Fedora 36

```shell
sudo dnf install \
    gcc gcc-c++ cmake pkg-config git \
    openssl-libs openssl-devel \
    numactl-libs numactl-devel \
    libcurl libcurl-devel \
    libyaml libyaml-devel \
    mbedtls mbedtls-devel \
    libatomic
```

## How to build it

### Checkout the code

```shell
git clone https://github.com/danielealbano/cachegrand.git
cd cachegrand
git submodule update --init --recursive
```

### Debug build

```shell
mkdir cmake-build-debug
cd cmake-build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_HASH_ALGORITHM_T1HA2=1
make
```

The binary will be available in `cmake-build-debug/src`

### Debug build with tests

For development or testing purposes you can enable the tests, which are built using Catch2, adding a flag

```shell
mkdir cmake-build-debug
cd cmake-build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_HASH_ALGORITHM_T1HA2=1 -DBUILD_TESTS=1
make
```

The `cachegrand-server` binary will be available in `cmake-build-debug/src`, instead the `cachegrand-tests` binary will
be available in `cmake-build-debug/tests`

### Release build

```shell
mkdir cmake-build-release
cd cmake-build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_HASH_ALGORITHM_T1HA2=1
make -j8
```

The cachegrand-server binary will be available in `cmake-build-release/src`

### Release build with benchmarks

```shell
mkdir cmake-build-release
cd cmake-build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_HASH_ALGORITHM_T1HA2=1 -DBUILD_INTERNAL_BENCHES=1
make -j8
```

The cachegrand-server binary will be available in `cmake-build-release/src` meanwhile for the benchmarks a binary will
be produced for each one and they will be available in `cmake-build-release/benches` folder.

*Please be aware that the benchmark suite is built for developers and require code modifications to be properly
configured for the hardware on which it has to run.*

*Also, please be aware that the benchmark suite is not built as part of the PR validation process and therefore breaking
changes might get merged, the benchmark suite is used for ad-hoc evaluations and it's normally used to benchmark
architectural changes or new internal functionalities or components.*
