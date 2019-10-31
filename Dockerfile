FROM ubuntu:18.04

ARG LEAF_PACKAGE

# Make sure package info is up to date
RUN apt-get update && \
    apt-get upgrade -y && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y curl && \
    rm -rf /var/lib/apt/lists/*

# Get leaf
RUN curl -o /tmp/leaf_latest.deb https://downloads.sierrawireless.com/tools/leaf/leaf_latest.deb && \
    apt-get update && \
    apt install -y /tmp/leaf_latest.deb && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    bison \
    build-essential \
    cmake \
    flex \
    gperf \
    libncurses5-dev \
    libtool \
    ninja-build \
    pkg-config \
    python-jinja2 \
    swicwe \
    swiflash \
    zip && \
    rm -rf /var/lib/apt/lists/*

RUN leaf remote add \
    --insecure \
    mangOH \
    https://downloads.sierrawireless.com/mangOH/leaf/mangOH-yellow.json

WORKDIR /leaf_workspace

RUN yes | leaf setup -p $LEAF_PACKAGE && \
    rm ~/.cache/leaf/files/*

RUN curl -o /tmp/bsec.zip \
    https://community.bosch-sensortec.com/varuj77995/attachments/varuj77995/bst_community-mems-forum/44/1/BSEC_1.4.7.2_GCC_CortexA7_20190225.zip && \
    unzip /tmp/bsec.zip && \
    rm /tmp/bsec.zip

RUN leaf env workspace --set BSEC_DIR=`pwd`/BSEC_1.4.7.2_GCC_CortexA7_20190225/algo/bin/Normal_version/Cortex_A7
