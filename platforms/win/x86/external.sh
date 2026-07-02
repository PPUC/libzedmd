#!/bin/bash

set -e

if [ -z "${MSYS2_PATH}" ]; then
   MSYS2_PATH="/c/msys64"
fi

echo "MSYS2_PATH: ${MSYS2_PATH}"
echo ""

source ./platforms/config.sh

echo "Building libraries..."
echo "  CARGS_SHA: ${CARGS_SHA}"
echo "  LIBSERIALPORT_SHA: ${LIBSERIALPORT_SHA}"
echo "  LIBFRAMEUTIL_SHA: ${LIBFRAMEUTIL_SHA}"
print_dependency_source LIBFRAMEUTIL "${LIBFRAMEUTIL_SHA}" LIBFRAMEUTIL_SOURCE_DIR
echo "  SOCKPP_SHA: ${SOCKPP_SHA}"
echo ""

rm -rf external
mkdir -p \
   external \
   third-party/include \
   third-party/build-libs/win/x86 \
   third-party/runtime-libs/win/x86
cd external

#
# build cargs and copy to external
#

curl -sL https://github.com/likle/cargs/archive/${CARGS_SHA}.tar.gz -o cargs-${CARGS_SHA}.tar.gz
tar xzf cargs-${CARGS_SHA}.tar.gz
mv cargs-${CARGS_SHA} cargs
cd cargs
cmake \
   -G "Visual Studio 18 2026" \
   -DBUILD_SHARED_LIBS=ON \
   -A Win32 \
   -B build
cmake --build build --config ${BUILD_TYPE}
cp include/cargs.h ${PROJECT_SOURCE_ROOT}/third-party/include/
cp build/${BUILD_TYPE}/cargs.lib ${PROJECT_SOURCE_ROOT}/third-party/build-libs/win/x86/
cp build/${BUILD_TYPE}/cargs.dll ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win/x86/
cd ..

#
# build libserialport and copy to platform/arch
#

curl -sL https://github.com/sigrokproject/libserialport/archive/${LIBSERIALPORT_SHA}.tar.gz -o libserialport-${LIBSERIALPORT_SHA}.tar.gz
tar xzf libserialport-${LIBSERIALPORT_SHA}.tar.gz
mv libserialport-${LIBSERIALPORT_SHA} libserialport
cd libserialport
cp libserialport.h ${PROJECT_SOURCE_ROOT}/third-party/include
CURRENT_DIR="$(pwd)"
MSYSTEM=MINGW32 "${MSYS2_PATH}/usr/bin/bash.exe" -l -c "
   cd \"${CURRENT_DIR}\" &&
   ./autogen.sh &&
   ./configure &&
   make -j\$(nproc)
"
cp .libs/libserialport.dll.a ${PROJECT_SOURCE_ROOT}/third-party/build-libs/win/x86/libserialport.lib
cp .libs/libserialport-0.dll ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win/x86/
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
   -G "Visual Studio 18 2026" \
   -A Win32 \
   -B build
cmake --build build --config ${BUILD_TYPE}
cp -r include/sockpp ${PROJECT_SOURCE_ROOT}/third-party/include/
cp build/${BUILD_TYPE}/sockpp.lib ${PROJECT_SOURCE_ROOT}/third-party/build-libs/win/x86/
cp build/${BUILD_TYPE}/sockpp.dll ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win/x86/
cd ..

#
# copy MINGW32 runtime DLLs (needed by MinGW-built DLLs)
#

MINGW32_BIN="${MSYS2_PATH}/mingw32/bin"

cp "${MINGW32_BIN}/libgcc_s_dw2-1.dll" ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win/x86/
cp "${MINGW32_BIN}/libstdc++-6.dll" ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win/x86/
cp "${MINGW32_BIN}/libwinpthread-1.dll" ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win/x86/
