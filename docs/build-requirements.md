# Build

## Requirements

| Package | Min. Version |   |
| - | - | - |
| pkg-config | | **mandatory** |
| kernel | >= 5.7.0 | **mandatory** |
| libnuma | >= 1.1 | **mandatory** |
| openssl | >= 2.0 | **mandatory** |

cachegrand depends on io_uring and liburing for the I/O and the networking therefore it's necessary to have recent
kernel (>=5.7.0), liburing is instead built internally as cmake target, it's built transparently.

The networking support for io_uring has been introduced around the kernel 5.4.0 but it's missing extremely important
functionalities introduced within the kernel 5.5, 5.6 and 5.7, therefore the minimum kernel version has been set to
5.7.0.

### Ubuntu

The kernel 5.8 or newer is provided for Ubuntu 20.10 and 20.04.2 LTS, for older versions if an update is not available
it's possible to use [ubuntu-mainline-kernel.sh](https://github.com/pimlie/ubuntu-mainline-kernel.sh) to update the 
kernel to an official build for the version 5.7.0 or newer.

It's possible to check the kernel version using
```shell
uname -r
```

Example output
```
5.10.15-051015-generic
```
 

Command line to install the non-kernel dependencies
```shell
apt-get install built-essential cmake pkg-config libnuma-dev libssl-dev
```
