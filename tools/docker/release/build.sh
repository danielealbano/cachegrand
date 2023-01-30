#!/bin/bash

# Requires docker buildx plugin to run
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
docker buildx rm cachegrand-server
docker buildx create --use --name cachegrand-server --bootstrap
docker buildx build --pull --push --platform linux/amd64,linux/arm64/v8 --tag cachegrand/cachegrand-server:latest --tag cachegrand/cachegrand-server:v0.1.5 .
docker buildx rm cachegrand-server
