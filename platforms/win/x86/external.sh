#!/bin/bash

set -e

LIBSERIALPORT_SHA=fd20b0fc5a34cd7f776e4af6c763f59041de223b
LIBFRAMEUTIL_SHA=93c0a6fe7f952c9c404613a983d66e1a73396d9f

echo "Building libraries..."
echo "  LIBSERIALPORT_SHA: ${LIBSERIALPORT_SHA}"
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
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
msbuild.exe libserialport.sln -p:Configuration=Release -p:Platform=x86
cp Release/libserialport.lib ../../third-party/build-libs/win/x86
cp Release/libserialport.dll ../../third-party/runtime-libs/win/x86
cd ..

#
# copy libframeutil
#

curl -sL https://github.com/ppuc/libframeutil/archive/${LIBFRAMEUTIL_SHA}.zip -o libframeutil.zip
unzip libframeutil.zip
cd libframeutil-$LIBFRAMEUTIL_SHA
cp include/* ../../third-party/include
cd ..
