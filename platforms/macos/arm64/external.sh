#!/bin/bash

set -e

LIBSERIALPORT_SHA=21b3dfe5f68c205be4086469335fd2fc2ce11ed2
LIBFRAMEUTIL_SHA=30048ca23d41ca0a8f7d5ab75d3f646a19a90182

NUM_PROCS=$(sysctl -n hw.ncpu)

echo "Building libraries..."
echo "  LIBSERIALPORT_SHA: ${LIBSERIALPORT_SHA}"
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
echo "  NUM_PROCS: ${NUM_PROCS}"
echo ""

rm -rf external
mkdir external
cd external

#
# build libserialport and copy to platform/arch
#

curl -sL https://github.com/sigrokproject/libserialport/archive/${LIBSERIALPORT_SHA}.zip -o libserialport.zip
unzip libserialport.zip
cd libserialport-$LIBSERIALPORT_SHA
cp libserialport.h ../../third-party/include
./autogen.sh
./configure --host=aarch64-apple-darwin CFLAGS="-arch arm64" LDFLAGS="-Wl,-install_name,@rpath/libserialport.dylib"
make -j${NUM_PROCS}
cp .libs/libserialport.a ../../third-party/build-libs/macos/arm64
cp .libs/libserialport.dylib ../../third-party/runtime-libs/macos/arm64
cd ..

#
# copy libframeutil
#

curl -sL https://github.com/ppuc/libframeutil/archive/${LIBFRAMEUTIL_SHA}.zip -o libframeutil.zip
unzip libframeutil.zip
cd libframeutil-$LIBFRAMEUTIL_SHA
cp include/* ../../third-party/include
cd ..
