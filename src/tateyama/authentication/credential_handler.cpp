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
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <optional>
#include <sys/ioctl.h>
#include <asm/termbits.h>
#include <csignal>

#include <gflags/gflags.h>
#include <nlohmann/json.hpp>
#include <boost/regex.hpp>

#include <tateyama/logging.h>

#include <tateyama/proto/endpoint/request.pb.h>
#include "tateyama/transport/transport.h"
#include "tateyama/tgctl/runtime_error.h"
#include "tateyama/configuration/bootstrap_configuration.h"

#include "rsa.h"
#include "client.h"
#include "token_handler.h"
#include "authenticator.h"

DEFINE_string(user, "", "user name for authentication");  // NOLINT
DEFINE_string(auth_token, "", "authentication token");  // NOLINT
DEFINE_string(credentials, "", "path to credentials");  // NOLINT
DEFINE_bool(_auth, true, "--no-auth when authentication is not used");  // NOLINT  false when --no-auth is specified
DECLARE_string(conf);  // NOLINT

namespace tateyama::authentication {

constexpr static int FORMAT_VERSION = 1;

void credential_handler::auth_options() {
    auto whole = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf).get_configuration();
    auto authentication_section = whole->get_section("authentication");
    auto enabled_opt = authentication_section->get<bool>("enabled");
    if (!enabled_opt || !enabled_opt.value()) {
        set_disabled();
        return;
    }

    if (!check_not_more_than_one()) {
        throw tgctl::runtime_error(tateyama::monitor::reason::authentication_failure, "more than one credential options are specified");
    }

    if (!FLAGS__auth) {
        set_no_auth();
        return;
    }
    if (!FLAGS_user.empty()) {
        set_user_password(FLAGS_user, prompt("password: ", false));
        return;
    }
    if (!FLAGS_auth_token.empty()) {
        set_auth_token(FLAGS_auth_token);
        return;
    }
    if (!FLAGS_credentials.empty()) {
        set_file_credential(std::filesystem::path(FLAGS_credentials));
        return;
    }

    if (auto* token = getenv("TSURUGI_AUTH_TOKEN"); token != nullptr) {
        set_auth_token(token);
        return;
    }
    if (auto cred_opt = default_credential_path(); cred_opt) {
        set_file_credential(cred_opt.value());
    }
}

void credential_handler::set_disabled() {
    type_ = credential_type::disabled;
}    

void credential_handler::set_no_auth() {
    type_ = credential_type::no_auth;
}

void credential_handler::set_user_password(const std::string& user, const std::string& password) {
    type_ = credential_type::user_password;
    json_text_ = get_json_text(user, password);
}

void credential_handler::set_auth_token(const std::string& auth_token) {
    type_ = credential_type::auth_token;
    auth_token_ = auth_token;
}

void credential_handler::set_file_credential(const std::filesystem::path& path) {
    type_ = credential_type::file;        
    std::ifstream file(path.string().c_str());
    if (!file.is_open()) {
        return;
    }

    std::string encrypted_credential{};
    std::getline(file, encrypted_credential);  // use first line only
    file.close();

    if (!encrypted_credential.empty()) {
        set_encrypted_credential(encrypted_credential);
    }
}

std::string credential_handler::expiration_date() const noexcept {
    return expiration_date_string_;
}

void credential_handler::set_expiration(std::int32_t expiration) noexcept {
    expiration_ = std::chrono::hours(expiration * 24);
}

void credential_handler::add_credential(tateyama::proto::endpoint::request::ClientInformation& information,
                                        const std::function<std::optional<std::string>()>& key_func) {
    switch (type_) {
    case credential_type::disabled:
        break;
    case credential_type::no_auth:
        break;
    case credential_type::user_password:
    {
        auto key_opt = key_func();
        if (key_opt) {
            rsa_encrypter rsa{key_opt.value()};
            std::string c{};
            rsa.encrypt(json_text_, c);
            std::string encrypted_credential = base64_encode(c);
            (information.mutable_credential())->set_encrypted_credential(encrypted_credential);
            break;
        }
        throw tgctl::runtime_error(tateyama::monitor::reason::authentication_failure, "error in get encryption key");
    }
    case credential_type::auth_token:
    {
        (information.mutable_credential())->set_remember_me_credential(auth_token_);
        break;
    }
    case credential_type::file:
    {
        (information.mutable_credential())->set_encrypted_credential(encrypted_credential_);
        break;
    }
    default:
        throw tgctl::runtime_error(tateyama::monitor::reason::authentication_failure, "no credential specified");
    }
}

std::optional<std::filesystem::path> credential_handler::default_credential_path() {
    if (auto* name = getenv("HOME"); name != nullptr) {
        std::filesystem::path path{name};
        path /= ".tsurugidb";
        path /= "credentials.key";
        return path;
    }
    return std::nullopt;
}

std::string credential_handler::get_json_text(const std::string& user, const std::string& password) {
    nlohmann::json j;
    std::stringstream ss;

    j["format_version"] = FORMAT_VERSION;
    j["user"] = user;
    j["password"] = password;
    if (expiration_.count() > 0) {
        j["expiration_date"] = expiration();
    }
    j["password"] = password;

    ss << j;
    return ss.str();
}

std::string& credential_handler::expiration() {
    std::stringstream r;

    const auto t = std::chrono::system_clock::now() + expiration_;
    const auto& ct = std::chrono::system_clock::to_time_t(t);
    const auto gt = std::gmtime(&ct);
    r << std::put_time(gt, "%Y-%m-%dT%H:%M:%S");

    const auto full = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch());
    const auto sec  = std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch());
    r << "." << std::fixed << std::setprecision(3) << (full.count() - (sec.count() * 1000))
      << "Z";

    expiration_date_string_ = r.str();
    return expiration_date_string_;
}

void credential_handler::set_encrypted_credential(const std::string& encrypted_credential) {
    type_ = credential_type::file;
    encrypted_credential_ = encrypted_credential;
}

bool credential_handler::check_not_more_than_one() {
    if (!FLAGS_user.empty()) {
        if (!FLAGS_auth_token.empty() || !FLAGS_credentials.empty() || !FLAGS__auth) {
            return false;
        }
    }
    if (!FLAGS_auth_token.empty()) {
        if (!FLAGS_credentials.empty() || !FLAGS__auth) {
            return false;
        }
    }
    if (!FLAGS_credentials.empty() && !FLAGS__auth) {
        return false;
    }
    return true;
}

}  // tateyama::authentication
