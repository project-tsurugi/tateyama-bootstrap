/*
 * Copyright 2023-2025 Project Tsurugi.
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
#pragma once

#include <exception>
#include <filesystem>
#include <sstream>

#include <gflags/gflags.h>

#include <tateyama/api/configuration.h>

DECLARE_string(conf);

namespace tateyama::datastore {

class path_util {
public:
    explicit path_util(std::filesystem::path backup_location) : backup_location_(std::move(backup_location)) {
        auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
        if (!bst_conf.valid()) {
            throw std::runtime_error("error in create_bootstrap_configuration()");
        }
        auto conf = bst_conf.get_configuration();
        if (!conf) {
            throw std::runtime_error("error in get_configuration()");
        }
        auto ds = conf->get_section("datastore");
        if (!ds) {
            throw std::runtime_error("error in get_section()");
        }
        auto opt = ds->get<std::filesystem::path>("log_location");
        if (!opt) {
            throw std::runtime_error("error in get<std::filesystem::path>");
        }
        log_location_ = opt.value();
    }

    std::filesystem::path omit(const std::filesystem::path& file) {
        std::filesystem::path::iterator file_itr = file.begin();
        for(auto itr = log_location_.begin(); itr != log_location_.end(); itr++, file_itr++) {
            if(*itr != *file_itr) {
                throw std::runtime_error("invalid log_location");
            }
        }
        std::filesystem::path rv{};
        while(file_itr != file.end()) {
            rv /= *file_itr;
            file_itr++;
        }
        return rv;
    }

    void create_directories(const std::filesystem::path& dir) {
        auto directory = backup_location_ / dir;
        if (!std::filesystem::exists(directory)) {
            std::filesystem::create_directories(directory);
        }
        if (!std::filesystem::is_directory(directory)) {
            std::ostringstream ss{};
            ss << directory << " is not directory";
            throw std::runtime_error(ss.str());
        }
    }

private:
    std::filesystem::path log_location_{};
    std::filesystem::path backup_location_;
};

} // namespace tateyama::datastore
