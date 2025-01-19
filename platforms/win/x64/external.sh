#!/bin/bash

set -e

LIBSERIALPORT_SHA=21b3dfe5f68c205be4086469335fd2fc2ce11ed2
LIBFRAMEUTIL_SHA=30048ca23d41ca0a8f7d5ab75d3f646a19a90182
SOCKPP_SHA=e6c4688a576d95f42dd7628cefe68092f6c5cd0f

echo "Building libraries..."
echo "  LIBSERIALPORT_SHA: ${LIBSERIALPORT_SHA}"
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
echo "  SOCKPP_SHA: ${SOCKPP_SHA}"
echo ""

if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

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
patch libserialport.vcxproj < ../../platforms/win/x64/libserialport/001.patch
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

#
# build sockpp and copy to external
#

curl -sL https://github.com/fpagliughi/sockpp/archive/${SOCKPP_SHA}.zip -o sockpp.zip
unzip sockpp.zip
cd sockpp-$SOCKPP_SHA
patch -p1 < ../../platforms/win/x64/sockpp/001.patch
cmake \
   -G "Visual Studio 17 2022" \
   -B build
cmake --build build --config ${BUILD_TYPE}
cp -r include/sockpp ../../third-party/include/
cp build/${BUILD_TYPE}/sockpp64.lib ../../third-party/build-libs/win/x64/
cp build/${BUILD_TYPE}/sockpp64.dll ../../third-party/runtime-libs/win/x64/
cd ..
