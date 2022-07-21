/*
 * Copyright 2022-2022 tsurugi project.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <gtest/gtest.h>
#include <glog/logging.h>

constexpr const char* location = "/tmp/tsurugi/";

static std::string name;

static const char* dir_name(const char* child) {
        name = location;
        name += child;
        return name.c_str();
}

static void set_up() {
    if (system("rm -rf /tmp/tsurugi") != 0) {
        std::cerr << "cannot remove directory" << std::endl;
        FAIL();
    }

    std::string command;

    command = "mkdir -p ";
    command += dir_name("test");
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot make directory" << std::endl;
        FAIL();
    }
    command = "mkdir -p ";
    command += dir_name("log");
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot make directory" << std::endl;
        FAIL();
    }
    command = "mkdir -p ";
    command += dir_name("backup");
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot make directory" << std::endl;
        FAIL();
    }
}

static void tear_down() {
    if (system("rm -rf /tmp/tsurugi") != 0) {
        std::cerr << "cannot remove directory" << std::endl;
    }
}
