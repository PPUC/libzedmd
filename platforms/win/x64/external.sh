#!/bin/bash

set -e

LIBSERIALPORT_SHA=fd20b0fc5a34cd7f776e4af6c763f59041de223b
LIBFRAMEUTIL_SHA=4da694f9518a01fcf13105c33c2ae1a396ec0aea

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
patch libserialport.vcxproj < ../../platforms/win/x64/libserialport.patch
msbuild.exe libserialport.sln -p:Configuration=Release -p:Platform=x64
cp x64/Release/libserialport64.lib ../../third-party/build-libs/win/x64
cp x64/Release/libserialport64.dll ../../third-party/runtime-libs/win/x64
cd ..

#
# copy libframeutil
#

curl -sL https://github.com/ppuc/libframeutil/archive/${LIBFRAMEUTIL_SHA}.zip -o libframeutil.zip
unzip libframeutil.zip
cd libframeutil-$LIBFRAMEUTIL_SHA
cp include/* ../../third-party/include
cd ..
