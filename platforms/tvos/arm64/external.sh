#!/bin/bash

set -e

source ./platforms/config.sh

echo "Building libraries..."
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
ppuc_print_dependency_source LIBFRAMEUTIL libframeutil "${LIBFRAMEUTIL_SHA}"
echo "  SOCKPP_SHA: ${SOCKPP_SHA}"
echo ""

NUM_PROCS=$(sysctl -n hw.ncpu)

rm -rf external
mkdir -p \
   external \
   third-party/include \
   third-party/build-libs/tvos/arm64 \
   third-party/runtime-libs/tvos/arm64
cd external

#
# copy libframeutil
#

ppuc_prepare_dependency_source libframeutil "${LIBFRAMEUTIL_SHA}" "https://github.com/ppuc/libframeutil/archive/${LIBFRAMEUTIL_SHA}.tar.gz"
cp libframeutil/include/* ${PPUC_SOURCE_ROOT}/third-party/include

#
# build sockpp and copy to external
#

curl -sL https://github.com/fpagliughi/sockpp/archive/${SOCKPP_SHA}.tar.gz -o sockpp-${SOCKPP_SHA}.tar.gz
tar xzf sockpp-${SOCKPP_SHA}.tar.gz
mv sockpp-${SOCKPP_SHA} sockpp
cd sockpp
cmake \
   -DSOCKPP_BUILD_SHARED=OFF \
   -DSOCKPP_BUILD_STATIC=ON \
   -DCMAKE_SYSTEM_NAME=tvOS \
   -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
   -DCMAKE_OSX_ARCHITECTURES=arm64 \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/sockpp ${PPUC_SOURCE_ROOT}/third-party/include/
cp build/libsockpp.a ${PPUC_SOURCE_ROOT}/third-party/build-libs/tvos/arm64/
cd ..
