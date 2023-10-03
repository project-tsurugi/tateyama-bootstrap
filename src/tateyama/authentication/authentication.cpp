/*
 * Copyright 2022-2023 Project Tsurugi.
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
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <gflags/gflags.h>

#include <tateyama/logging.h>

#include "authentication.h"

DEFINE_string(user, "", "user name for authentication");  // NOLINT
DEFINE_string(auth_token, "", "authentication token");  // NOLINT
DEFINE_string(credentials, "", "path to credentials.json");  // NOLINT
DEFINE_bool(auth, true, "--no-auth when authentication is not used");  // NOLINT

namespace tateyama::authentication {

constexpr std::string_view pre_defined_auth_file_name = "/tmp/auth";  // FIXME specify concrete file

void auth_options() {
    if (!FLAGS_auth) {
#ifndef NDEBUG
        std::cout << "no-auth" << std::endl;
#endif
        return;
    }
    if (!FLAGS_user.empty()) {
#ifndef NDEBUG
        std::cout << "auth user:= " << FLAGS_user << std::endl;
#endif
        return;
    }
    if (!FLAGS_auth_token.empty()) {
#ifndef NDEBUG
        std::cout << "auth token: " << FLAGS_auth_token << std::endl;
#endif
        return;
    }
    if (!FLAGS_credentials.empty()) {
#ifndef NDEBUG
        std::cout << "auth credentials: " << FLAGS_credentials << std::endl;
#endif
        return;
    }

    if (auto* token = getenv("TSURUGI_AUTH_TOKEN"); token != nullptr) {
#ifndef NDEBUG
        std::cout << "auth token fron TSURUGI_AUTH_TOKEN: " << token << std::endl;
#endif
        return;
    }
    const auto pre_defined_auth_file = std::filesystem::path(std::string(pre_defined_auth_file_name));
    if (std::filesystem::exists(pre_defined_auth_file)) {
        std::ifstream istrm{};
        istrm.open(pre_defined_auth_file, std::ios_base::in);
         
        istrm.seekg( 0, std::ios_base::end );
        std::size_t filesize = istrm.tellg();
        istrm.seekg( 0, std::ios_base::beg );

        std::string contents{};
        contents.resize(filesize);
        istrm.read(contents.data(), static_cast<std::int64_t>(contents.length()));
        istrm.close();

#ifndef NDEBUG
        std::cout << "auth token fron pre_defined_auth_file (" << pre_defined_auth_file.string() << "): " << contents << std::endl;
#endif
    }
}

}  // tateyama::bootstrap
