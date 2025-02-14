#!/bin/bash

set -e

CARGS_SHA=0698c3f90333446d0fc2745c1e9ce10dd4a9497a
LIBSERIALPORT_SHA=21b3dfe5f68c205be4086469335fd2fc2ce11ed2
LIBFRAMEUTIL_SHA=30048ca23d41ca0a8f7d5ab75d3f646a19a90182
SOCKPP_SHA=e6c4688a576d95f42dd7628cefe68092f6c5cd0f

echo "Building libraries..."
echo "  CARGS_SHA: ${CARGS_SHA}"
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
# build cargs and copy to external
#

curl -sL https://github.com/likle/cargs/archive/${CARGS_SHA}.zip -o cargs.zip
unzip cargs.zip
cd cargs-${CARGS_SHA}
cmake \
   -G "Visual Studio 17 2022" \
   -DBUILD_SHARED_LIBS=ON \
   -A Win32 \
   -B build
cmake --build build --config ${BUILD_TYPE}
cp include/cargs.h ../../third-party/include/
cp build/${BUILD_TYPE}/cargs.lib ../../third-party/build-libs/win/x86/
cp build/${BUILD_TYPE}/cargs.dll ../../third-party/runtime-libs/win/x86/
cd ..

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

#
# build sockpp and copy to external
#

curl -sL https://github.com/fpagliughi/sockpp/archive/${SOCKPP_SHA}.zip -o sockpp.zip
unzip sockpp.zip
cd sockpp-$SOCKPP_SHA
cmake \
   -G "Visual Studio 17 2022" \
   -A Win32 \
   -B build
cmake --build build --config ${BUILD_TYPE}
cp -r include/sockpp ../../third-party/include/
cp build/${BUILD_TYPE}/sockpp.lib ../../third-party/build-libs/win/x86/
cp build/${BUILD_TYPE}/sockpp.dll ../../third-party/runtime-libs/win/x86/
cd ..
