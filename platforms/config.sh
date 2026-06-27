#!/bin/bash

set -e

CARGS_SHA=0698c3f90333446d0fc2745c1e9ce10dd4a9497a
LIBSERIALPORT_SHA=21b3dfe5f68c205be4086469335fd2fc2ce11ed2
LIBFRAMEUTIL_SHA=28f2bae0dabcbd5c599e6f62211f009e078c1f96
SOCKPP_SHA=e6c4688a576d95f42dd7628cefe68092f6c5cd0f

PPUC_SOURCE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PPUC_LOCAL_DEPS_ROOT="${PPUC_LOCAL_DEPS_ROOT:-$(cd "${PPUC_SOURCE_ROOT}/.." && pwd)}"
PPUC_USE_LOCAL_DEPS="${PPUC_USE_LOCAL_DEPS:-1}"

ppuc_local_dependency_dir() {
   local name="$1"
   local dir="${PPUC_LOCAL_DEPS_ROOT}/${name}"

   if [ "${PPUC_USE_LOCAL_DEPS}" != "0" ] && [ -d "${dir}" ] && [ "${dir}" != "${PPUC_SOURCE_ROOT}" ]; then
      echo "${dir}"
   fi
}

ppuc_print_dependency_source() {
   local label="$1"
   local name="$2"
   local sha="$3"
   local local_dir

   local_dir="$(ppuc_local_dependency_dir "${name}")"
   if [ -n "${local_dir}" ]; then
      echo "  ${label}_SOURCE: local ${local_dir}"
   else
      echo "  ${label}_SOURCE: archive ${sha}"
   fi
}

ppuc_prepare_dependency_source() {
   local name="$1"
   local sha="$2"
   local url="$3"
   local archive_type="${4:-tar}"
   local local_dir

   local_dir="$(ppuc_local_dependency_dir "${name}")"
   if [ -n "${local_dir}" ]; then
      echo "Using local ${name}: ${local_dir}"
      ln -s "${local_dir}" "${name}"
   elif [ "${archive_type}" = "zip" ]; then
      curl -sL "${url}" -o "${name}.zip"
      unzip "${name}.zip"
      mv "${name}-${sha}" "${name}"
   else
      curl -sL "${url}" -o "${name}-${sha}.tar.gz"
      tar xzf "${name}-${sha}.tar.gz"
      mv "${name}-${sha}" "${name}"
   fi
}


if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

echo "Build type: ${BUILD_TYPE}"
echo ""
