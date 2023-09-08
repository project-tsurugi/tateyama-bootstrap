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
#include <sstream>
#include <string>
#include <filesystem>

#include <boost/property_tree/ptree.hpp>
#define BOOST_BIND_GLOBAL_PLACEHOLDERS  // to retain the current behavior
#include <boost/property_tree/json_parser.hpp>

class directory_helper {
    constexpr static std::string_view base = "/tmp/";  // NOLINT

  public:
    directory_helper(std::string prefix, std::uint32_t port, bool direct) : prefix_(prefix), port_(port) {
        location_ = std::string(base);
        if (direct) {
            location_ += prefix;
            location_ += "/";
            conf_ = abs_path("tsurugi.ini");
        } else {
            location_ += prefix;
            location_ += std::to_string(getpid());
            location_ += "/";
            conf_ = abs_path("conf/tsurugi.ini");
        }
    }
    directory_helper(std::string prefix, std::uint32_t port) : directory_helper(prefix, port, false) {}

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

    void confirm_started() {
        std::string command;
        for (std::size_t i = 0; i < 10; i++) {
            command = "tgctl status --conf ";
            command += conf_file_path();
            command += " --monitor ";
            command += abs_path("test/confirming.log");
            if (system(command.c_str()) != 0) {
                std::cerr << "cannot tgctl status" << std::endl;
                FAIL();
            }

            FILE *fp;
            command = "grep starting ";
            command += abs_path("test/confirming.log");
            command += " | wc -l";
            if((fp = popen(command.c_str(), "r")) == nullptr){
                std::cerr << "cannot wc" << std::endl;
            }
            int l, rv;
            rv = fscanf(fp, "%d", &l);
            if (l == 0 && rv == 1) {
                return;
            }
            usleep(5 * 1000);
        }
        FAIL();
    }

private:
    std::string prefix_;
    std::uint32_t port_;
    std::string location_{};
    std::string conf_{};
    std::string name_{};

    std::ofstream strm_;
};

static bool validate_json(std::filesystem::path file)
{
    bool result = true;
    std::ifstream strm;
    std::stringstream ss;
    std::string line;

    strm.open(file, std::ios_base::in);
    while (getline(strm, line)){
        ss << line;
        try {
            boost::property_tree::ptree pt;
            boost::property_tree::read_json(ss, pt);
        } catch (boost::property_tree::json_parser_error) {
            result = false;
        }
        ss.str("");
        ss.clear(std::stringstream::goodbit);
    }
    strm.close();

    return result;
}
