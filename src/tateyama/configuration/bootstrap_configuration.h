/*
 * Copyright 2019-2021 tsurugi project.
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

#include <boost/filesystem/path.hpp>

#include <tateyama/api/configuration.h>

namespace tateyama::configuration {

static const boost::filesystem::path CONF_FILE_NAME = boost::filesystem::path("tsurugi.ini");  // NOLINT
static const std::string_view DEFAULT_PID_DIR = "/tmp";  // NOLINT and obsolete
static const std::string_view PID_FILE_PREFIX = "tsurugi";
static const char *ENV_ENTRY = "TGDIR";  // NOLINT
std::string_view default_property_for_bootstrap();

class bootstrap_configuration {
public:
    static bootstrap_configuration create_bootstrap_configuration(std::string_view file) {
        try {
            return bootstrap_configuration(file);
        } catch (std::runtime_error &e) {
            return bootstrap_configuration();
        }
    }
    std::shared_ptr<tateyama::api::configuration::whole> get_configuration() {
        return configuration_;
    }
    [[nodiscard]] boost::filesystem::path lock_file() const {
        return lock_file_;
    }
    [[nodiscard]] std::string digest() {
        return digest(boost::filesystem::canonical(conf_file_).string());
    }
    [[nodiscard]] bool valid() const {
        return valid_;
    }

private:
    boost::filesystem::path conf_file_;
    boost::filesystem::path lock_file_;
    std::shared_ptr<tateyama::api::configuration::whole> configuration_{};
    bool valid_{};

    // should create this object via create_bootstrap_configuration()
    bootstrap_configuration() = default;
    explicit bootstrap_configuration(std::string_view f) {
        // tsurugi.ini
        if (!f.empty()) {
            conf_file_ = boost::filesystem::path(std::string(f));
        } else {
            if (auto env = getenv(ENV_ENTRY); env != nullptr) {
                conf_file_ = boost::filesystem::path(env) / CONF_FILE_NAME;
            } else {
                throw std::runtime_error("no configuration file specified");
            }
        }
        // do sanity check for conf_file_
        boost::system::error_code error;
        const bool result = boost::filesystem::exists(conf_file_, error);
        if (!result || error) {
            throw std::runtime_error(std::string("cannot find configuration file: ") + conf_file_.string());
        }
        if (boost::filesystem::is_directory(conf_file_)) {
            throw std::runtime_error(conf_file_.string() + " is a directory");
        }
        configuration_ = tateyama::api::configuration::create_configuration(conf_file_.string(), default_property_for_bootstrap());

        std::string directory{DEFAULT_PID_DIR};
        if (auto system_config = configuration_->get_section("system"); system_config) {
            if (auto pid_dir = system_config->get<std::string>("pid_directory"); pid_dir) {
                directory = pid_dir.value();
            }
        }
        std::string pid_file_name(PID_FILE_PREFIX);
        pid_file_name += "-";
        pid_file_name += digest(boost::filesystem::canonical(conf_file_).string());
        pid_file_name += ".pid";
        lock_file_ = boost::filesystem::path(std::string(directory)) / boost::filesystem::path(pid_file_name);
        valid_ = true;
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