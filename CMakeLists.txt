# Copyright 2018-2026 Project Tsurugi.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(tateyama-bootstrap
    VERSION 1.0.0
    DESCRIPTION "tateyama bootstrap"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(ENABLE_SANITIZER "enable sanitizer on debug build" ON)
option(ENABLE_UB_SANITIZER "enable undefined behavior sanitizer on debug build" OFF)
option(ENABLE_COVERAGE "enable coverage on debug build" OFF)
option(BUILD_TESTS "Build test programs" ON)
option(BUILD_DOCUMENTS "build documents" ON)
option(BUILD_STRICT "build with option strictly determine of success" ON)
option(OGAWAYAMA "activate ogawayama brigde" OFF)
option(ENABLE_JEMALLOC "use jemalloc instead of default malloc" OFF)
option(ENABLE_ALTIMETER "enable altimeter logging" OFF)

if(NOT DEFINED SHARKSFIN_IMPLEMENTATION)
    set(
            SHARKSFIN_IMPLEMENTATION "shirakami"
            CACHE STRING
            "sharksfin target name to link"
            FORCE
    )
endif()

set(tateyama_package "tateyama-${SHARKSFIN_IMPLEMENTATION}")
set(tateyama_engine "${tateyama_package}-engine")
set(tateyama_api "${tateyama_package}-api")
set(jogasaki_package "jogasaki-${SHARKSFIN_IMPLEMENTATION}")
set(jogasaki_engine "${jogasaki_package}-engine")

find_package(${tateyama_package} REQUIRED)
find_package(${jogasaki_package} REQUIRED)
find_package(takatori REQUIRED)
find_package(yugawara REQUIRED)
find_package(sharksfin REQUIRED)
find_package(Doxygen)
find_package(glog REQUIRED)
find_package(gflags REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost
        COMPONENTS filesystem thread system container regex
        REQUIRED
        )
find_package(Protobuf REQUIRED)
find_package(mpdecpp REQUIRED)

if (ENABLE_JEMALLOC)
    find_package(jemalloc REQUIRED)
endif (ENABLE_JEMALLOC)

if (ENABLE_ALTIMETER)
    find_package(altimeter REQUIRED)
    find_package(Boost
        COMPONENTS container
        REQUIRED
    )
    find_package(fmt REQUIRED)
    find_package(Threads REQUIRED)
endif()

if (BUILD_TESTS)
  add_subdirectory(third_party) # should be before enable_testing()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)
include(InstallOptions)

if (BUILD_TESTS OR BUILD_EXAMPLES)
    enable_testing()
endif()

set(export_name "tateyama-bootstrap-${SHARKSFIN_IMPLEMENTATION}")
set(package_name "tateyama-bootstrap-${SHARKSFIN_IMPLEMENTATION}")

add_library(api INTERFACE)

target_include_directories(api
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/tateyama>
)

add_subdirectory(src)
if(BUILD_TESTS)
    add_subdirectory(test)
endif()
if (BUILD_DOCUMENTS)
    add_subdirectory(doxygen)
endif()
