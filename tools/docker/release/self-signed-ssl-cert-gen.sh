#!/bin/bash

# Forked from https://github.com/stakater/dockerfile-ssl-certs-generator/blob/master/generate-certs

echo "> Generating SSL certificate"

export OPENSSL_CA_KEY=${OPENSSL_CA_KEY-"/etc/cachegrand/ca.key"}
export OPENSSL_CA_CERT=${OPENSSL_CA_CERT-"/etc/cachegrand/ca.pem"}
export OPENSSL_CA_SUBJECT=${OPENSSL_CA_SUBJECT:-"cachegrand-selfsigned-ca"}
export OPENSSL_CA_EXPIRE=${OPENSSL_CA_EXPIRE:-"365"}
export OPENSSL_CA_SIZE=${OPENSSL_CA_SIZE:-"2048"}

export OPENSSL_CERT_CONFIG=${OPENSSL_CERT_CONFIG:-"openssl.cnf"}
export OPENSSL_CERT_KEY=${OPENSSL_CERT_KEY:-"/etc/cachegrand/certificate.key"}
export OPENSSL_CERT_CSR=${OPENSSL_CERT_CSR:-"/etc/cachegrand/certificate.csr"}
export OPENSSL_CERT_CERT=${OPENSSL_CERT_CERT:-"/etc/cachegrand/certificate.pem"}
export OPENSSL_CERT_SIZE=${OPENSSL_CERT_SIZE:-"2048"}
export OPENSSL_CERT_EXPIRE=${OPENSSL_CERT_EXPIRE:-"365"}
export OPENSSL_CERT_SUBJECT=${OPENSSL_CERT_SUBJECT:-"cachegrand"}

echo "--> Certificate Authority"

if [[ -e "${OPENSSL_CA_KEY}" ]]; then
    echo "====> Using existing CA Key ${OPENSSL_CA_KEY}"
else
    echo "====> Generating new CA key ${OPENSSL_CA_KEY}"
    openssl genrsa -out \
      "${OPENSSL_CA_KEY}" \
      ${OPENSSL_CA_SIZE} \
      >/dev/null 2>&1 \
      || exit 1
fi

if [[ -e "${OPENSSL_CA_CERT}" ]]; then
    echo "====> Using existing CA Certificate ${OPENSSL_CA_CERT}"
else
    echo "====> Generating new CA Certificate ${OPENSSL_CA_CERT}"
    openssl req -x509 -new -nodes \
      -key "${OPENSSL_CA_KEY}" \
      -days ${OPENSSL_CA_EXPIRE} \
      -out "${OPENSSL_CA_CERT}"\
      -subj "/CN=${OPENSSL_CA_SUBJECT}" \
      >/dev/null 2>&1 \
      || exit 1
fi

echo "====> Generating new config file ${OPENSSL_CERT_CONFIG}"
cat > ${OPENSSL_CERT_CONFIG} <<EOM
[req]
req_extensions = v3_req
distinguished_name = req_distinguished_name
[req_distinguished_name]
[ v3_req ]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage = clientAuth, serverAuth
EOM

echo "--> Certificate"

echo "====> Generating new SSL KEY ${OPENSSL_CERT_KEY}"
openssl genrsa -out \
  "${OPENSSL_CERT_KEY}" \
  ${OPENSSL_CERT_SIZE} \
  >/dev/null 2>&1 \
  || exit 1

echo "====> Generating new SSL CSR ${OPENSSL_CERT_CSR}"
openssl req -new \
  -key "${OPENSSL_CERT_KEY}" \
  -out "${OPENSSL_CERT_CSR}" \
  -subj "/CN=${OPENSSL_CERT_SUBJECT}" \
  -config ${OPENSSL_CERT_CONFIG} \
  >/dev/null 2>&1 \
  || exit 1

echo "====> Generating new SSL CERT ${OPENSSL_CERT_CERT}"
openssl x509 -req \
  -in "${OPENSSL_CERT_CSR}" \
  -CA "${OPENSSL_CA_CERT}" \
  -CAkey "${OPENSSL_CA_KEY}" \
  -CAcreateserial \
  -out ${OPENSSL_CERT_CERT} \
  -days ${OPENSSL_CERT_EXPIRE} \
  -extensions v3_req \
  -extfile ${OPENSSL_CERT_CONFIG} \
  >/dev/null 2>&1 \
  || exit 1
