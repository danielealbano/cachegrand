#### Build - Requirements

| Package | Min. Version |   |
| - | - | - |
| pkg-config | | **mandatory** |
| kernel | >= 5.7.0 | **mandatory** |
| liburing | >= 0.7 | **mandatory** |
| openssl | >= 1.1 | optional |

cachegrand depends (will depend) on liburing for the I/O and the networking therefore to build the code it's necessary
to have an up-to-date kernel (5.7.0 minimum) and an up-to-date liburing (0.7) installed in the system.

Although it's probably matter of days, the liburing 0.7 version hasn't been released yet, so it's necessary to compile
it from the repository and install the package locally.

On Ubuntu 20.04 it's possible to use
 - [ubuntu-mainline-kernel.sh](https://github.com/pimlie/ubuntu-mainline-kernel.sh) to update the kernel to an official 
   build for the 5.7.6 version
 - the below snippet to download, patch, compile and install liburing from the repository 

```bash
# Install dh-make to be able to build the deb packages
sudo apt -y install dh-make

# Create a temporary directory and move inside it
mkdir temp && cd temp

# Clone the repo and switch to the wanted commit id, the commit id can be changed as needed
git clone https://github.com/axboe/liburing
cd liburing
git checkout 20a7c0108734f01264fb4edadefe7bc96ac58855 # Bump version to 1.0.7 - https://github.com/axboe/liburing/commit/94ba6378bea8db499bedeabb54ab20fcf41555cf

# An updated version of the build settings is needed for ubuntu
mv debian debian_old
wget http://archive.ubuntu.com/ubuntu/pool/universe/libu/liburing/liburing_0.6-3.debian.tar.xz
tar -xf liburing_0.6-3.debian.tar.xz
cp debian_old/compat debian/compat

# No need for this setting, compat has been copied from the original debian folder
sed "/debhelper\-compat/d" -i debian/control 

# Update the version of the changelog to 0.7-1, just to get the version we want
sed -e "s/liburing (0.6-3)/liburing (0.7-1)/" -i debian/changelog

# Fix debian/liburing1.symbols to include the new version
sed -e "/LIBURING_0.6@LIBURING_0.6 0.6/ a \ LIBURING_0.7@LIBURING_0.7 0.7-1" -i debian/liburing1.symbols

# Now it's possible to build but lets skip the tests
DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -b -us -uc -rfakeroot

# In the parent directory there will be now a number of deb files that can be installed
# To install the packages it's necessary to use sudo
cd ..
sudo dpkg -i liburing-dev_0.7-1_amd64.deb liburing1_0.7-1_amd64.deb
```

For pratical reasons liburing is internally marked as optional but once the network layer will be implemented it will
become required so during the build process double check to see if the build system will report that it has been found
and that the version is the correct one, an example below.
```
-- Checking for module 'liburing>=0.7'
--   Found liburing, version 0.7
```
