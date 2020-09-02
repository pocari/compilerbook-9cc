FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
RUN set -ex \
    && apt-get update \
    && apt-get install -y \
                 build-essential \
                 gdb \
                 git \
                 binutils \
                 libc6-dev \
                 vim-tiny
RUN mkdir -p /var/compilerbook
RUN echo "set disassembly-flavor intel" > /root/.gdbinit
WORKDIR /var/compilerbook
