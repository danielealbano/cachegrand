#### Build - Requirements

| Package | Min. Version |   |
| - | - | - |
| pkg-config | | **mandatory** |
| kernel | >= 5.7.0 | **mandatory** |
| openssl | >= 1.1 | optional |

cachegrand depends on io_uring and liburing for the I/O and the networking therefore it's necessary to have recent
kernel (>=5.7.0), liburing is instead built internally as cmake target, it's built transparently.

The networking support for io_uring has been introduced around the kernel 5.4.0 but it's missing extremely important
functionalities introduced within the kernel 5.5, 5.6 and 5.7, there fore the minimum kernel version has been set to
5.7.0.

On Ubuntu it's possible to use [ubuntu-mainline-kernel.sh](https://github.com/pimlie/ubuntu-mainline-kernel.sh) to
update the kernel to an official build for the 5.7.0 or newer version. 
