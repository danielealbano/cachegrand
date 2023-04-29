#!/bin/bash

# Requires docker buildx plugin to run
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
docker buildx create --name cachegrand-builder --platform linux/amd64 --use
docker buildx create --name cachegrand-builder --append --platform linux/arm64,linux/arm/v8 ssh://cg-server-arm64-01
docker buildx build --no-cache --pull --platform linux/amd64,linux/arm64/v8 --tag cachegrand/cachegrand-server:latest --tag cachegrand/cachegrand-server:v0.2.1 .
docker buildx build --push --platform linux/amd64,linux/arm64/v8 --tag cachegrand/cachegrand-server:latest --tag cachegrand/cachegrand-server:v0.2.1 .
docker buildx rm --name cachegrand-builder
