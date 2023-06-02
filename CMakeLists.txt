cmake_minimum_required(VERSION 3.18)

option(USE_OSXARM "macOS arm64" OFF)
option(USE_OSXINTEL "macOS x86_64" OFF)
option(USE_IOS "ios" OFF)
option(USE_TVOS "tvos" OFF)
option(USE_ANDROID "android" OFF)
option(USE_WIN32 "Win32" OFF)

if(USE_IOS)
   set(CMAKE_SYSTEM_NAME iOS)
elseif(USE_TVOS)
   set(CMAKE_SYSTEM_NAME tvOS)
elseif(USE_ANDROID)
   set(CMAKE_SYSTEM_NAME Android)
   set(CMAKE_SYSTEM_VERSION 30)
   set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
endif()

file(READ src/ZeDMD.h version)
string(REGEX MATCH "ZEDMD_VERSION_MAJOR[ ]+([0-9]+)" _tmp ${version})
set(VERSION_MAJOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "ZEDMD_VERSION_MINOR[ ]+([0-9]+)" _tmp ${version})
set(VERSION_MINOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "ZEDMD_VERSION_PATCH[ ]+([0-9]+)" _tmp ${version})
set(VERSION_PATCH "${CMAKE_MATCH_1}")

project(zedmd VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
	DESCRIPTION "Cross-platform library for communicating with ZeDMD.")

set(LIB_NAME "zedmd")
if(USE_OSXARM)
   set(CMAKE_OSX_ARCHITECTURES arm64)
elseif(USE_OSXINTEL)
   set(CMAKE_OSX_ARCHITECTURES x86_64)
elseif(USE_IOS OR USE_TVOS)
  set(CMAKE_OSX_DEPLOYMENT_TARGET 16.0)
  set(CMAKE_OSX_ARCHITECTURES arm64)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set(CMAKE_C_FLAGS -fembed-bitcode)
  set(CMAKE_CXX_FLAGS -fembed-bitcode)
elseif(USE_WIN32)
  add_compile_definitions(WIN32)
elseif(USE_ANDROID)
else()
   set(LIB_NAME "zedmd64")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(MINIZ_SOURCES
   src/miniz/miniz.h
   src/miniz/miniz.c
)

if(USE_ANDROID)
else()
    set(SERIALIB_SOURCES
      src/serialib/serialib.h
      src/serialib/serialib.cpp
    )
endif()

set(LIBRARY_SOURCES
   ${MINIZ_SOURCES}
   ${SERIALIB_SOURCES}
   src/SerialPort.h
   src/SerialPort.cpp
   src/ZeDMDComm.h
   src/ZeDMDComm.cpp
   src/ZeDMD.h
   src/ZeDMD.cpp
)

set(INCLUDE_DIRS
   src
)

include_directories(AFTER src)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT USE_IOS AND NOT USE_TVOS)
       add_library(zedmd_shared SHARED ${LIBRARY_SOURCES})
       set_target_properties(zedmd_shared PROPERTIES
          OUTPUT_NAME ${LIB_NAME}
          VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
          LINK_FLAGS_RELEASE -s
       )
       target_include_directories(zedmd_shared PUBLIC ${INCLUDE_DIRS})
       install(TARGETS zedmd_shared
          LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
       )
    endif()


    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        add_library(zedmd_static STATIC ${LIBRARY_SOURCES})
        set_target_properties(zedmd_static PROPERTIES OUTPUT_NAME "zedmd")
        target_include_directories(zedmd_static PUBLIC ${INCLUDE_DIRS})
        install(TARGETS zedmd_static
           LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        )
        install(FILES src/ZeDMD.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
    endif()

else()
    include_directories(AFTER src)
    add_library(ZeDMD ${LIBRARY_SOURCES})
endif()
