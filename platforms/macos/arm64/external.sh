#!/bin/bash

set -e

LIBSERIALPORT_SHA=fd20b0fc5a34cd7f776e4af6c763f59041de223b
LIBFRAMEUTIL_SHA=93c0a6fe7f952c9c404613a983d66e1a73396d9f

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
