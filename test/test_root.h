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

#include <string_view>
#include <iostream>
#include <fstream>

class directory_helper {
    constexpr static std::string_view base = "/tmp/";  // NOLINT

  public:
    directory_helper(std::string prefix, std::uint32_t port) : prefix_(prefix), port_(port) {
        location_ = std::string(base);
        location_ += prefix;
        location_ += "/";

        conf_ = abs_path("conf/tsurugi.conf");
    }

    std::string& abs_path(const char* child) {
        name_ = location_;
        name_ += child;
        return name_;
    }

    void set_up() {
        std::string command;

        command = "rm -rf ";
        command += location_;
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot remove directory" << std::endl;
            FAIL();
        }

        command = "mkdir -p ";
        command += abs_path("test");
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot make directory" << std::endl;
            FAIL();
        }
        command = "mkdir -p ";
        command += abs_path("log");
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot make directory" << std::endl;
            FAIL();
        }
        command = "mkdir -p ";
        command += abs_path("backup");
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot make directory" << std::endl;
            FAIL();
        }
        command = "mkdir -p ";
        command += abs_path("conf");
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot make directory" << std::endl;
            FAIL();
        }

        strm_.open(conf_, std::ios_base::out | std::ios_base::trunc);
        strm_ << "[ipc_endpoint]\ndatabase_name=" << prefix_ << "\n\n";
        strm_ << "[stream_endpoint]\nport=" << port_ << "\n\n";
        strm_ << "[fdw]\nname=" << prefix_ << "\n\n";
        strm_ << "[datastore]\nlog_location=" << abs_path("log")  << "\n";
        strm_.close();
    }

    void tear_down() {
        std::string command;

        command = "rm -rf ";
        command += location_;
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot remove directory" << std::endl;
            FAIL();
        }
    }

    std::string& conf_file_path() {
        return conf_;
    }

  private:
    std::string prefix_;
    std::uint32_t port_;
    std::string location_{};
    std::string conf_{};
    std::string name_{};

    std::ofstream strm_;
};
