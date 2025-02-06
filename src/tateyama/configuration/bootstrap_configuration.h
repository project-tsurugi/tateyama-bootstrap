/*
 * Copyright 2019-2023 Project Tsurugi.
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

#include <string_view>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <sstream>
#include <filesystem>

#include <tateyama/api/configuration.h>

#include "tateyama/tgctl/runtime_error.h"

namespace tateyama::configuration {

static const std::string_view PID_FILE_PREFIX = "tsurugi";
static const char *ENV_CONF = "TSURUGI_CONF";  // NOLINT
static const char *ENV_HOME = "TSURUGI_HOME";  // NOLINT
static const std::filesystem::path HOME_CONF_FILE = std::filesystem::path("var/etc/tsurugi.ini");  // NOLINT
std::string_view default_property_for_bootstrap();

class bootstrap_configuration {
public:
    static bootstrap_configuration create_bootstrap_configuration(std::string_view file) {
        try {
            return bootstrap_configuration(file);
        } catch (tgctl::runtime_error &e) {
            return {};
        }
    }
    std::shared_ptr<tateyama::api::configuration::whole> get_configuration() {
        return configuration_;
    }
    [[nodiscard]] std::filesystem::path lock_file() const {
        return lock_file_;
    }
    [[nodiscard]] std::string digest() {
        return digest(std::filesystem::canonical(conf_file_).string());
    }
    [[nodiscard]] bool valid() const {
        return valid_;
    }
    [[nodiscard]] std::filesystem::path conf_file() const {  // for test purpose
        return conf_file_;
    }

private:
    std::filesystem::path conf_file_;
    std::filesystem::path lock_file_;
    std::shared_ptr<tateyama::api::configuration::whole> configuration_{};
    bool valid_{};

    // should create this object via create_bootstrap_configuration()
    bootstrap_configuration() = default;
    explicit bootstrap_configuration(std::string_view fname) {
        auto* env_home = getenv(ENV_HOME);

        // tsurugi.ini
        if (!fname.empty()) {
            conf_file_ = std::filesystem::path(std::string(fname));
        } else {
            if (auto env_conf = getenv(ENV_CONF); env_conf != nullptr) {
                conf_file_ = std::filesystem::path(env_conf);
            } else {
                if (env_home != nullptr) {
                    conf_file_ = std::filesystem::path(env_home) / HOME_CONF_FILE;
                } else {
                    throw tgctl::runtime_error(monitor::reason::internal, "no configuration file specified");
                }
            }
        }
        // do sanity check for conf_file_
        std::error_code error;
        const bool result = std::filesystem::exists(conf_file_, error);
        if (!result || error) {
            throw tgctl::runtime_error(monitor::reason::internal, std::string("cannot find configuration file: ") + conf_file_.string());
        }
        if (std::filesystem::is_directory(conf_file_)) {
            throw tgctl::runtime_error(monitor::reason::internal, conf_file_.string() + " is a directory");
        }
        configuration_ = tateyama::api::configuration::create_configuration(conf_file_.string(), default_property_for_bootstrap());

        if (env_home != nullptr) {
            configuration_->base_path(std::filesystem::path(env_home));
        }
        if (auto system_config = configuration_->get_section("system"); system_config) {
            if (auto pid_dir = system_config->get<std::filesystem::path>("pid_directory"); pid_dir) {
                std::filesystem::path directory = pid_dir.value();

                std::string pid_file_name(PID_FILE_PREFIX);
                pid_file_name += "-";
                pid_file_name += digest(std::filesystem::canonical(conf_file_).string());
                pid_file_name += ".pid";
                lock_file_ = directory / std::filesystem::path(pid_file_name);
                valid_ = true;
                return;
            }
        }
        throw tgctl::runtime_error(monitor::reason::internal, "error in lock file location");
    }
    std::string digest(const std::string& path_string) {
        auto hash = std::hash<std::string>{}(path_string);
        std::ostringstream sstream;
        sstream << std::hex << std::setfill('0')
                << std::setw(sizeof(hash) * 2) << hash;
        return sstream.str();
    }
};

} // namespace tateyama::configuration
