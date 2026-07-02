#!/bin/bash

set -e

source ./platforms/config.sh

echo "Building libraries..."
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
print_dependency_source LIBFRAMEUTIL "${LIBFRAMEUTIL_SHA}" LIBFRAMEUTIL_SOURCE_DIR
echo "  SOCKPP_SHA: ${SOCKPP_SHA}"
echo ""

NUM_PROCS=$(sysctl -n hw.ncpu)

rm -rf external
mkdir -p \
   external \
   third-party/include \
   third-party/build-libs/ios/arm64 \
   third-party/runtime-libs/ios/arm64
cd external

#
# copy libframeutil
#

prepare_dependency_source libframeutil "${LIBFRAMEUTIL_SHA}" "https://github.com/ppuc/libframeutil/archive/${LIBFRAMEUTIL_SHA}.tar.gz" tar LIBFRAMEUTIL_SOURCE_DIR
cp libframeutil/include/* ${PROJECT_SOURCE_ROOT}/third-party/include

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
   -DCMAKE_SYSTEM_NAME=iOS \
   -DCMAKE_OSX_ARCHITECTURES=arm64 \
   -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/sockpp ${PROJECT_SOURCE_ROOT}/third-party/include/
cp build/libsockpp.a ${PROJECT_SOURCE_ROOT}/third-party/build-libs/ios/arm64/
cd ..

