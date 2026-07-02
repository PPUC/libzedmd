#!/bin/bash

set -e

source ./platforms/config.sh

echo "Building libraries..."
echo "  CARGS_SHA: ${CARGS_SHA}"
echo "  LIBSERIALPORT_SHA: ${LIBSERIALPORT_SHA}"
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
print_dependency_source LIBFRAMEUTIL "${LIBFRAMEUTIL_SHA}" LIBFRAMEUTIL_SOURCE_DIR
echo "  SOCKPP_SHA: ${SOCKPP_SHA}"
echo ""

NUM_PROCS=$(nproc)

rm -rf external
mkdir -p \
   external \
   third-party/include \
   third-party/build-libs/linux/x64 \
   third-party/runtime-libs/linux/x64
cd external

#
# build cargs and copy to external
#

curl -sL https://github.com/likle/cargs/archive/${CARGS_SHA}.tar.gz -o cargs-${CARGS_SHA}.tar.gz
tar xzf cargs-${CARGS_SHA}.tar.gz
mv cargs-${CARGS_SHA} cargs
cd cargs
cmake \
   -DBUILD_SHARED_LIBS=ON \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp include/cargs.h ${PROJECT_SOURCE_ROOT}/third-party/include/
cp build/libcargs.so ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/linux/x64/
cd ..

#
# build libserialport and copy to platform/arch
#

curl -sL https://github.com/sigrokproject/libserialport/archive/${LIBSERIALPORT_SHA}.tar.gz -o libserialport-${LIBSERIALPORT_SHA}.tar.gz
tar xzf libserialport-${LIBSERIALPORT_SHA}.tar.gz
mv libserialport-${LIBSERIALPORT_SHA} libserialport
cd libserialport
cp libserialport.h ${PROJECT_SOURCE_ROOT}/third-party/include
./autogen.sh
./configure
make -j${NUM_PROCS}
cp .libs/libserialport.a ${PROJECT_SOURCE_ROOT}/third-party/build-libs/linux/x64
cp -a .libs/libserialport.{so,so.*} ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/linux/x64
cd ..

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
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/sockpp ${PROJECT_SOURCE_ROOT}/third-party/include/
cp -a build/libsockpp.{so,so.*} ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/linux/x64/
cd ..
