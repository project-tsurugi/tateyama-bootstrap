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
#include <boost/filesystem/path.hpp>

#include <tateyama/api/configuration.h>

namespace tateyama::bootstrap::utils {

static const boost::filesystem::path CONF_FILE_NAME = boost::filesystem::path("tsurugi.ini");
static const char *ENV_ENTRY = "TGDIR";  // NOLINT

class bootstrap_configuration {
public:
    bootstrap_configuration(std::string_view d) {
        // tsurugi.ini
        if (!d.empty()) {
            directory_ = boost::filesystem::path(std::string(d).c_str());
        } else {
            if (auto env = getenv(ENV_ENTRY); env != nullptr) {
                directory_ = boost::filesystem::path(env);
            } else {
                directory_ = std::string("");
                property_directory_absent_ = true;
            }
        }
    }
    std::shared_ptr<tateyama::api::configuration::whole> create_configuration() {
        if (!property_directory_absent_) {
            return tateyama::api::configuration::create_configuration((directory_ / CONF_FILE_NAME).string());
        }
        return nullptr;
    }

private:
    boost::filesystem::path directory_;
    std::shared_ptr<tateyama::api::configuration::whole> configuration_;
    bool property_directory_absent_{};
};

} // namespace tateyama::bootstrap::utils
