FROM ubuntu:20.04

RUN set -ex \
    && apt-get update \
    && apt-get install -y \
                 gcc \
                 make \
                 git \
                 binutils \
                 libc6-dev \
                 vim-tiny
RUN mkdir -p /var/compilerbook
WORKDIR /var/compilerbook
