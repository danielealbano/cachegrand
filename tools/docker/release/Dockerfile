# Copyright (C) 2018-2023 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

FROM ubuntu:22.04
LABEL maintainer="d.albano@cachegrand.io"

# General dpkg and packages settings
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Disable the man pages
COPY tools/docker/release/dpkg_cfg_d_01_nodoc /etc/dpkg/dpkg.cfg.d/01_nodoc

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
    && mkdir /var/run/cachegrand/ \
    && mkdir /var/lib/cachegrand/

VOLUME /var/lib/cachegrand/

COPY cmake-build-release/src/cachegrand-server /usr/bin
COPY etc/cachegrand.yaml.skel /etc/cachegrand/cachegrand.yaml.skel
COPY tools/docker/release/self-signed-ssl-cert-gen.sh /self-signed-ssl-cert-gen.sh
COPY tools/docker/release/startup.sh /startup.sh

RUN \
    cat /etc/cachegrand/cachegrand.yaml.skel \
        | sed -e 's/\#\(\s\+tls\:\)/\1/' \
        | sed -e 's/\#\(\s\+certificate_path\:\)/\1/' \
        | sed -e 's/\#\(\s\+private_key_path\:\)/\1/' \
        | sed -e 's/\#\(\s\+ca_certificate_chain_path\:\)/\1/' \
        | sed -e "/^\s\- type\: file$/ s|^|//|; /^\s*\- type\: file/, /);$/ s|^|#|" \
        | sed -e 's/^           tls: true/          tls: true/' \
        | sed -e 's/^           tls: false/          tls: false/' \
        > /etc/cachegrand/cachegrand.yaml \
    && rm /etc/cachegrand/cachegrand.yaml.skel \
    && chmod 700 /self-signed-ssl-cert-gen.sh \
    && chmod 700 /startup.sh

EXPOSE 6379
EXPOSE 6380
EXPOSE 9090

CMD [ "/startup.sh" ]
