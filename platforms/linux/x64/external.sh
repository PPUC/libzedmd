#!/bin/bash

set -e

LIBSERIALPORT_SHA=fd20b0fc5a34cd7f776e4af6c763f59041de223b

NUM_PROCS=$(nproc)

echo "Building libraries..."
echo "  LIBSERIALPORT_SHA: ${LIBSERIALPORT_SHA}"
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
./configure
make -j${NUM_PROCS}
cp .libs/libserialport.a ../../third-party/build-libs/linux/x64
cp .libs/libserialport.so.0 ../../third-party/runtime-libs/linux/x64
cd ..