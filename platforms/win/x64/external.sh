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
ppuc_print_dependency_source LIBFRAMEUTIL libframeutil "${LIBFRAMEUTIL_SHA}"
echo "  SOCKPP_SHA: ${SOCKPP_SHA}"
echo ""

rm -rf external
mkdir -p \
   external \
   third-party/include \
   third-party/build-libs/win/x64 \
   third-party/runtime-libs/win/x64
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
cp include/cargs.h ${PPUC_SOURCE_ROOT}/third-party/include/
cp build/${BUILD_TYPE}/cargs64.lib ${PPUC_SOURCE_ROOT}/third-party/build-libs/win/x64/
cp build/${BUILD_TYPE}/cargs64.dll ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/win/x64/
cd ..

#
# build libserialport and copy to platform/arch
#

curl -sL https://github.com/sigrokproject/libserialport/archive/${LIBSERIALPORT_SHA}.tar.gz -o libserialport-${LIBSERIALPORT_SHA}.tar.gz
tar xzf libserialport-${LIBSERIALPORT_SHA}.tar.gz
mv libserialport-${LIBSERIALPORT_SHA} libserialport
cd libserialport
cp libserialport.h ${PPUC_SOURCE_ROOT}/third-party/include
sed -i.bak 's/libserialport\.la/libserialport64.la/g; s/libserialport_la/libserialport64_la/g' Makefile.am
CURRENT_DIR="$(pwd)"
MSYSTEM=UCRT64 "${MSYS2_PATH}/usr/bin/bash.exe" -l -c "
   cd \"${CURRENT_DIR}\" &&
   ./autogen.sh &&
   ./configure &&
   make -j\$(nproc)
"
cp .libs/libserialport64.dll.a ${PPUC_SOURCE_ROOT}/third-party/build-libs/win/x64/libserialport64.lib
cp .libs/libserialport64-0.dll ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/win/x64/
cd ..

#
# copy libframeutil
#

ppuc_prepare_dependency_source libframeutil "${LIBFRAMEUTIL_SHA}" "https://github.com/ppuc/libframeutil/archive/${LIBFRAMEUTIL_SHA}.tar.gz"
cp libframeutil/include/* ${PPUC_SOURCE_ROOT}/third-party/include

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
cp -r include/sockpp ${PPUC_SOURCE_ROOT}/third-party/include/
cp build/${BUILD_TYPE}/sockpp64.lib ${PPUC_SOURCE_ROOT}/third-party/build-libs/win/x64/
cp build/${BUILD_TYPE}/sockpp64.dll ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/win/x64/
cd ..

#
# copy UCRT64 runtime DLLs (needed by MinGW-built DLLs)
#

UCRT64_BIN="${MSYS2_PATH}/ucrt64/bin"

cp "${UCRT64_BIN}/libgcc_s_seh-1.dll" ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/win/x64/
cp "${UCRT64_BIN}/libstdc++-6.dll" ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/win/x64/
cp "${UCRT64_BIN}/libwinpthread-1.dll" ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/win/x64/
