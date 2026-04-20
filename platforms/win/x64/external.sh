#!/bin/bash

set -e

source ./platforms/config.sh

echo "Building libraries..."
echo "  CARGS_SHA: ${CARGS_SHA}"
echo "  LIBSERIALPORT_SHA: ${LIBSERIALPORT_SHA}"
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
echo "  SOCKPP_SHA: ${SOCKPP_SHA}"
echo ""

rm -rf external
mkdir external
cd external

#
# build cargs and copy to external
#

curl -sL https://github.com/likle/cargs/archive/${CARGS_SHA}.tar.gz -o cargs-${CARGS_SHA}.tar.gz
tar xzf cargs-${CARGS_SHA}.tar.gz
mv cargs-${CARGS_SHA} cargs
cd cargs
sed -i.bak 's/set_target_properties(cargs PROPERTIES DEFINE_SYMBOL CAG_EXPORTS)/set_target_properties(cargs PROPERTIES DEFINE_SYMBOL CAG_EXPORTS)\nset_target_properties(cargs PROPERTIES OUTPUT_NAME cargs64)/' CMakeLists.txt
cmake \
   -G "Visual Studio 18 2026" \
   -DBUILD_SHARED_LIBS=ON \
   -B build
cmake --build build --config ${BUILD_TYPE}
cp include/cargs.h ../../third-party/include/
cp build/${BUILD_TYPE}/cargs64.lib ../../third-party/build-libs/win/x64/
cp build/${BUILD_TYPE}/cargs64.dll ../../third-party/runtime-libs/win/x64/
cd ..

#
# build libserialport and copy to platform/arch
#

curl -sL https://github.com/sigrokproject/libserialport/archive/${LIBSERIALPORT_SHA}.tar.gz -o libserialport-${LIBSERIALPORT_SHA}.tar.gz
tar xzf libserialport-${LIBSERIALPORT_SHA}.tar.gz
mv libserialport-${LIBSERIALPORT_SHA} libserialport
cd libserialport
cp libserialport.h ../../third-party/include
msbuild.exe libserialport.sln \
   -p:PlatformToolset=v143 \
   -p:TargetName=libserialport64 \
   -p:Platform=x64 \
   -p:Configuration=Release
cp x64/Release/libserialport64.lib ../../third-party/build-libs/win/x64
cp x64/Release/libserialport64.dll ../../third-party/runtime-libs/win/x64
cd ..

#
# copy libframeutil
#

curl -sL https://github.com/ppuc/libframeutil/archive/${LIBFRAMEUTIL_SHA}.tar.gz -o libframeutil-${LIBFRAMEUTIL_SHA}.tar.gz
tar xzf libframeutil-${LIBFRAMEUTIL_SHA}.tar.gz
mv libframeutil-${LIBFRAMEUTIL_SHA} libframeutil
cd libframeutil
cp include/* ../../third-party/include
cd ..

#
# build sockpp and copy to external
#

curl -sL https://github.com/fpagliughi/sockpp/archive/${SOCKPP_SHA}.tar.gz -o sockpp-${SOCKPP_SHA}.tar.gz
tar xzf sockpp-${SOCKPP_SHA}.tar.gz
mv sockpp-${SOCKPP_SHA} sockpp
cd sockpp
sed -i.bak 's/set(SOCKPP_SHARED_LIBRARY sockpp)/set(SOCKPP_SHARED_LIBRARY sockpp64)/' CMakeLists.txt
cmake \
   -G "Visual Studio 18 2026" \
   -B build
cmake --build build --config ${BUILD_TYPE}
cp -r include/sockpp ../../third-party/include/
cp build/${BUILD_TYPE}/sockpp64.lib ../../third-party/build-libs/win/x64/
cp build/${BUILD_TYPE}/sockpp64.dll ../../third-party/runtime-libs/win/x64/
cd ..
