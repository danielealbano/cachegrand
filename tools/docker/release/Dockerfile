# Copyright (C) 2018-2023 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

FROM ubuntu:22.04 AS builder

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
        libmbedtls-dev libmbedtls14 liblzf1 liblzf-dev \
        lcov gcovr \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN cd /root \
    && mkdir build \
    && cd build \
    && git clone https://github.com/danielealbano/cachegrand.git \
    && cd cachegrand \
    && git submodule update --init --recursive \
    && mkdir cmake-build-release \
    && cd cmake-build-release \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_HASH_ALGORITHM_T1HA2=1 \
    && make cachegrand-server

FROM ubuntu:22.04
LABEL maintainer="d.albano@cachegrand.io"

# General dpkg and packages settings
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Disable the man pages
COPY dpkg_cfg_d_01_nodoc /etc/dpkg/dpkg.cfg.d/01_nodoc

# Install the required deps
RUN apt-get update -y \
    && apt-get upgrade -y \
    && apt-get install -y --no-install-recommends \
        apt-utils \
    && apt-get install -y --no-install-recommends \
        libnuma1 libyaml-0-2 libcurl4 libmbedtls14 libatomic1 openssl liblzf1 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN \
    mkdir /etc/cachegrand \
    && mkdir /var/run/cachegrand/

COPY --from=builder /root/build/cachegrand/cmake-build-release/src/cachegrand-server /usr/bin
COPY --from=builder /root/build/cachegrand/etc/cachegrand.yaml.skel /etc/cachegrand/cachegrand.yaml.skel
COPY --from=builder /root/build/cachegrand/tools/docker/release/self-signed-ssl-cert-gen.sh /self-signed-ssl-cert-gen.sh
COPY --from=builder /root/build/cachegrand/tools/docker/release/startup.sh /startup.sh

RUN \
    cat /etc/cachegrand/cachegrand.yaml.skel \
        | sed -e 's/\#\(\s\+tls\:\)/\1/' \
        | sed -e 's/\#\(\s\+certificate_path\:\)/\1/' \
        | sed -e 's/\#\(\s\+private_key_path\:\)/\1/' \
        | sed -e "/^\s\- type\: file$/ s|^|//|; /^\s*\- type\: file/, /);$/ s|^|#|" \
        | sed -e 's/^           tls: true/          tls: true/' \
        | sed -e 's/^           tls: false/          tls: false/' \
        > /etc/cachegrand/cachegrand.yaml \
    && rm /etc/cachegrand/cachegrand.yaml.skel \
    && chmod 700 /self-signed-ssl-cert-gen.sh \
    && chmod 700 /startup.sh

EXPOSE 6379
EXPOSE 6380

CMD [ "/startup.sh" ]
