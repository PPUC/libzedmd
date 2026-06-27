#!/bin/bash

set -e

source ./platforms/config.sh

echo "Building libraries..."
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
ppuc_print_dependency_source LIBFRAMEUTIL libframeutil "${LIBFRAMEUTIL_SHA}"
echo "  SOCKPP_SHA: ${SOCKPP_SHA}"
echo ""

if [[ $(uname) == "Linux" ]]; then
   NUM_PROCS=$(nproc)
elif [[ $(uname) == "Darwin" ]]; then
   NUM_PROCS=$(sysctl -n hw.ncpu)
else
   NUM_PROCS=1
fi

rm -rf external
mkdir -p \
   external \
   third-party/include \
   third-party/build-libs/android/arm64-v8a \
   third-party/runtime-libs/android/arm64-v8a
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
patch -p1 < ../../platforms/android/arm64-v8a/sockpp/001.patch
cmake \
   -DSOCKPP_BUILD_SHARED=ON \
   -DSOCKPP_BUILD_STATIC=OFF \
   -DCMAKE_SYSTEM_NAME=Android \
   -DCMAKE_SYSTEM_VERSION=30 \
   -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
   -DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE \
   -DCMAKE_INSTALL_RPATH="\$ORIGIN" \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/sockpp ${PPUC_SOURCE_ROOT}/third-party/include/
cp build/libsockpp.so ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/android/arm64-v8a/
cd ..
