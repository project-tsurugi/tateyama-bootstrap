# Copyright 2020-2020 tsurugi project.
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

cmake_minimum_required(VERSION 3.10)

project(tateyama-bootstrap
    VERSION 0.0.1
    DESCRIPTION "tateyama bootstrap"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(ENABLE_SANITIZER "enable sanitizer on debug build" ON)
option(ENABLE_UB_SANITIZER "enable undefined behavior sanitizer on debug build" OFF)
option(ENABLE_COVERAGE "enable coverage on debug build" OFF)

if(NOT DEFINED SHARKSFIN_IMPLEMENTATION)
    set(
            SHARKSFIN_IMPLEMENTATION "memory"
            CACHE STRING
            "sharksfin target name to link"
            FORCE
    )
endif()

set(tateyama_package "tateyama-${SHARKSFIN_IMPLEMENTATION}")
set(tateyama_engine "${tateyama_package}-engine")
set(jogasaki_package "jogasaki-${SHARKSFIN_IMPLEMENTATION}")
set(jogasaki_engine "${jogasaki_package}-engine")

find_package(${tateyama_package} REQUIRED)
find_package(${jogasaki_package} REQUIRED)
find_package(takatori REQUIRED)
find_package(yugawara REQUIRED)
find_package(fpdecimal REQUIRED)
find_package(Doxygen)
find_package(glog REQUIRED)
find_package(gflags REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost
        COMPONENTS filesystem thread system container stacktrace_backtrace
        REQUIRED
        )

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CompileOptions)
include(InstallOptions)

set(export_name "tateyama-bootstrap-${SHARKSFIN_IMPLEMENTATION}")
set(package_name "tateyama-bootstrap-${SHARKSFIN_IMPLEMENTATION}")

add_subdirectory(src)
