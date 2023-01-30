#!/bin/bash

# Generates the self signed certificate at bootstrap if they don't exist already
/self-signed-ssl-cert-gen.sh

# Starts cachegrand
/usr/bin/cachegrand-server -c /etc/cachegrand/cachegrand.yaml
