#!/bin/bash
#
# 2021-08-11 J. Dagger
#
# Build wiredtiger at release "mongodb-5.3.1"
# 
# The release can be modified but it'll only work for ones
# after they started using cmake.
# 
# cmake is required and ninja and ccache are reccomended
# See further build reqs at https://github.com/wiredtiger/wiredtiger/blob/mongodb-5.3.1/cmake/README.md

TOP_DIR="."
WT_DIR="${TOP_DIR}/wiredtiger"
BUILD_DIR="build"
WTPERF_PATH="${WT_DIR}/${BUILD_DIR}/bench/wtperf/wtperf"
RELEASE="mongodb-5.3.1"

# clone wiredtiger if it doesn't exist
if [ -d "wiredtiger" ]; then
    echo "Using files in ${WT_DIR}"
else
    echo "Cloning wiredtiger into ${WT_DIR}"
    git clone https://github.com/wiredtiger/wiredtiger.git -b develop ${WT_DIR}
fi

# build wiredtiger if it isn't built
if [ -f "$WTPERF_PATH" ]; then
    echo "Using wtperf in ${PWD}/${WTPERF_PATH}"
else
    echo "wtperf not found. Building in ${WT_DIR}"
    cd ${WT_DIR}
    git fetch --all --tags --prune
    git checkout tags/$RELEASE -b WT-5.3.1

    mkdir $BUILD_DIR && cd $BUILD_DIR
    cmake ../.
    make
    cd -

    echo "wtperf built"
fi

cp -a $TOP_DIR/conf/wiredtiger/configs/. $WT_DIR/bench/wtperf/runners