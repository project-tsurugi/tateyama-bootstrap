/*
 * Copyright 2022-2025 Project Tsurugi.
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

#include <functional>
#include <string>
#include <optional>
#include <filesystem>

#include <tateyama/api/configuration.h>

#include "tateyama/tgctl/tgctl.h"
#include <tateyama/proto/endpoint/request.pb.h>
#include "credential_handler.h"

namespace tateyama::authentication {

std::string prompt(std::string_view msg, bool display);

class authenticator {
public:
    authenticator();

    void authenticate(tateyama::api::configuration::section*);  // NOLINT(readability-redundant-declaration) only for readability

    tgctl::return_code credentials();  // NOLINT(readability-redundant-declaration) only for readability
    tgctl::return_code credentials(const std::string&);  // NOLINT(readability-redundant-declaration) only for readability

private:
    /**
     * @brief maximum length for valid username
     */
    constexpr static std::size_t MAXIMUM_USERNAME_LENGTH = 1024;

    credential_handler credential_handler_{};    

    tgctl::return_code credentials(const std::filesystem::path& path);

    static std::optional<std::string> check_username(const std::optional<std::string>& name_opt);
};

}  // tateyama::authentication
