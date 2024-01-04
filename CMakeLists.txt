cmake_minimum_required(VERSION 3.25)

set(PLATFORM "win" CACHE STRING "Platform")
set(ARCH "x64" CACHE STRING "Arch")

option(BUILD_SHARED "Option to build shared library" ON)
option(BUILD_STATIC "Option to build static library" ON)

message(STATUS "PLATFORM: ${PLATFORM}")
message(STATUS "ARCH: ${ARCH}")

message(STATUS "BUILD_SHARED: ${BUILD_SHARED}")
message(STATUS "BUILD_STATIC: ${BUILD_STATIC}")

if(PLATFORM STREQUAL "ios")
   set(CMAKE_SYSTEM_NAME iOS)
elseif(PLATFORM STREQUAL "tvos")
   set(CMAKE_SYSTEM_NAME tvOS)
elseif(PLATFORM STREQUAL "android")
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

if(PLATFORM STREQUAL "win")
   if(ARCH STREQUAL "x86")
      add_compile_definitions(WIN32)
   endif()
elseif(PLATFORM STREQUAL "macos")
   set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
   if (ARCH STREQUAL "arm64")
      set(CMAKE_OSX_ARCHITECTURES arm64)
   elseif(ARCH STREQUAL "x64")
      set(CMAKE_OSX_ARCHITECTURES x86_64)
   endif()
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   set(CMAKE_INSTALL_RPATH "@executable_path")
elseif(PLATFORM STREQUAL "ios" OR PLATFORM STREQUAL "tvos")
   set(CMAKE_OSX_DEPLOYMENT_TARGET 16.0)
   set(CMAKE_OSX_ARCHITECTURES arm64)
elseif(PLATFORM STREQUAL "linux" OR PLATFORM STREQUAL "android")
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 99)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(ZEDMD_SOURCES
   src/ZeDMDComm.h
   src/ZeDMDComm.cpp
   src/ZeDMDWiFi.h
   src/ZeDMDWiFi.cpp
   src/ZeDMD.h
   src/ZeDMD.cpp
   third-party/include/miniz/miniz.h
   third-party/include/miniz/miniz.c
)

set(ZEDMD_INCLUDE_DIRS
   src
   third-party/include
)

if(BUILD_SHARED)
   add_library(zedmd_shared SHARED ${ZEDMD_SOURCES})

   target_include_directories(zedmd_shared PUBLIC ${ZEDMD_INCLUDE_DIRS})

   if(PLATFORM STREQUAL "win")
      target_link_directories(zedmd_shared PUBLIC
         third-party/build-libs/${PLATFORM}/${ARCH}
         third-party/runtime-libs/${PLATFORM}/${ARCH}
      )
      target_link_libraries(zedmd_shared PUBLIC libserialport ws2_32)
   elseif(PLATFORM STREQUAL "macos")
      target_link_directories(zedmd_shared PUBLIC
         third-party/runtime-libs/${PLATFORM}/${ARCH}
      )
      target_link_libraries(zedmd_shared PUBLIC serialport)
   elseif(PLATFORM STREQUAL "linux")
      target_link_directories(zedmd_shared PUBLIC
         third-party/runtime-libs/${PLATFORM}/${ARCH}
      )
      target_link_libraries(zedmd_shared PUBLIC -l:libserialport.so.0)
   endif()

   if(PLATFORM STREQUAL "win" AND ARCH STREQUAL "x64")
      set(ZEDMD_OUTPUT_NAME "zedmd64")
   else()
      set(ZEDMD_OUTPUT_NAME "zedmd")
   endif()

   set_target_properties(zedmd_shared PROPERTIES
      OUTPUT_NAME ${ZEDMD_OUTPUT_NAME}
      VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
   )

   install(TARGETS zedmd_shared
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
   )
   install(FILES src/ZeDMD.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

   if(PLATFORM STREQUAL "win" OR PLATFORM STREQUAL "macos" OR PLATFORM STREQUAL "linux")
      add_executable(zedmd_test
         src/test.cpp
      )

      target_link_libraries(zedmd_test PUBLIC zedmd_shared)

      if(PLATFORM STREQUAL "win")
         add_custom_command(TARGET zedmd_test POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/build-libs/${PLATFORM}/${ARCH}/libserialport.lib" "$<TARGET_FILE_DIR:zedmd_test>"
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/runtime-libs/${PLATFORM}/${ARCH}/libserialport.dll" "$<TARGET_FILE_DIR:zedmd_test>"
         )
      elseif(PLATFORM STREQUAL "macos")
         add_custom_command(TARGET zedmd_test POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/runtime-libs/${PLATFORM}/${ARCH}/libserialport.dylib" "$<TARGET_FILE_DIR:zedmd_test>"
         )
      elseif(PLATFORM STREQUAL "linux")
         add_custom_command(TARGET zedmd_test POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/runtime-libs/${PLATFORM}/${ARCH}/libserialport.so.0" "$<TARGET_FILE_DIR:zedmd_test>"
         )
      endif()
   endif()
endif()

if(BUILD_STATIC)
   add_library(zedmd_static STATIC ${ZEDMD_SOURCES})

   target_include_directories(zedmd_static PUBLIC ${ZEDMD_INCLUDE_DIRS})

   if(PLATFORM STREQUAL "win")
      set_target_properties(zedmd_static PROPERTIES
         OUTPUT_NAME "zedmd_static"
      )
   else()
      set_target_properties(zedmd_static PROPERTIES
         OUTPUT_NAME "zedmd"
      )
   endif()

   install(TARGETS zedmd_static
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
   )
   install(FILES src/ZeDMD.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

   if(PLATFORM STREQUAL "win" OR PLATFORM STREQUAL "macos" OR PLATFORM STREQUAL "linux")
      add_executable(zedmd_test_s
         src/test.cpp
      )

      if(PLATFORM STREQUAL "win")
         target_link_directories(zedmd_test_s PUBLIC
            third-party/build-libs/${PLATFORM}/${ARCH}
            third-party/runtime-libs/${PLATFORM}/${ARCH}
         )
         target_link_libraries(zedmd_test_s PUBLIC zedmd_static libserialport ws2_32)

         add_custom_command(TARGET zedmd_test_s POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/build-libs/${PLATFORM}/${ARCH}/libserialport.lib" "$<TARGET_FILE_DIR:zedmd_test_s>"
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/runtime-libs/${PLATFORM}/${ARCH}/libserialport.dll" "$<TARGET_FILE_DIR:zedmd_test_s>"
         )
      elseif(PLATFORM STREQUAL "macos")
         target_link_directories(zedmd_test_s PUBLIC
            third-party/runtime-libs/${PLATFORM}/${ARCH}
         )
         target_link_libraries(zedmd_test_s PUBLIC zedmd_static serialport)

         add_custom_command(TARGET zedmd_test_s POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/runtime-libs/${PLATFORM}/${ARCH}/libserialport.dylib" "$<TARGET_FILE_DIR:zedmd_test_s>"
         )
      elseif(PLATFORM STREQUAL "linux")
         target_link_directories(zedmd_test_s PUBLIC
            third-party/runtime-libs/${PLATFORM}/${ARCH}
         )
         target_link_libraries(zedmd_test_s PUBLIC zedmd_static -l:libserialport.so.0)

         add_custom_command(TARGET zedmd_test_s POST_BUILD
           COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_SOURCE_DIR}/third-party/runtime-libs/${PLATFORM}/${ARCH}/libserialport.so.0" "$<TARGET_FILE_DIR:zedmd_test_s>"
         )
      endif()
   endif()
endif()
