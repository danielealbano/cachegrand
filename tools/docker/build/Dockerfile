FROM ubuntu:focal
MAINTAINER d.albano@cachegrand.io

# General dpkg and packages settings
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Disable the man pages
COPY dpkg_cfg_d_01_nodoc /etc/dpkg/dpkg.cfg.d/01_nodoc

# Reset the permissions in case the build has run from Docker for Windows on WSL2
RUN chmod 644 /etc/dpkg/dpkg.cfg.d/01_nodoc

# Update the image and install the required deps
RUN apt update -y \
    && apt upgrade -y \
    && apt install -y --no-install-recommends build-essential cmake git pkg-config libssl1.1 libssl-dev lcov \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*