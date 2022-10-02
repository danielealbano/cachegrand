# Copyright (C) 2018-2022 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

# Build commands
#docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
#docker buildx create --use --name cachegrand-builder
#docker buildx build --pull --push --platform linux/amd64,linux/arm64/v8 --tag gitlab.cachegrand.dev:5050/cachegrand/cachegrand-server/ubuntu-2004-gcc:latest .
#docker buildx rm --name cachegrand-builder

FROM ubuntu:22.04
LABEL maintainer="d.albano@cachegrand.io"

# General dpkg and packages settings
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Disable the man pages
COPY dpkg_cfg_d_01_nodoc /etc/dpkg/dpkg.cfg.d/01_nodoc

# Reset the permissions in case the build has run from Docker for Windows on WSL2
RUN chmod 644 /etc/dpkg/dpkg.cfg.d/01_nodoc

# Workaround for a recent docker buildx issue (https://github.com/docker/buildx/issues/495)
RUN ln -s /usr/bin/dpkg-split /usr/sbin/dpkg-split \
    && ln -s /usr/bin/dpkg-deb /usr/sbin/dpkg-deb \
    && ln -s /bin/tar /usr/sbin/tar \
    && ln -s /bin/rm /usr/sbin/rm

# Update the image and install the required deps
RUN apt-get update -y \
    && apt-get upgrade -y \
    && apt-get install -y --no-install-recommends \
        apt-utils \
    && apt-get install -y --no-install-recommends \
        curl ca-certificates \
        build-essential cmake pkg-config git \
        libnuma1 libnuma-dev \
        libcurl4-openssl-dev libcurl4 \
        libyaml-0-2 libyaml-dev \
        libmbedtls-dev libmbedtls14 \
        libpcre2-8-0 libpcre2-dev \
        libjson-c-dev \
        lcov gcovr \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
