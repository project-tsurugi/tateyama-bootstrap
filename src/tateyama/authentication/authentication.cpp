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

#include <gflags/gflags.h>

#include <tateyama/logging.h>

#include <tateyama/proto/endpoint/request.pb.h>
#include "tateyama/tgctl/runtime_error.h"
#include "rsa.h"
#include "authentication.h"

DEFINE_string(user, "", "user name for authentication");  // NOLINT
DEFINE_string(auth_token, "", "authentication token");  // NOLINT
DEFINE_string(credentials, "", "path to credentials");  // NOLINT
DEFINE_bool(auth, true, "--no-auth when authentication is not used");  // NOLINT

namespace tateyama::authentication {

void auth_options() {
    if (!FLAGS_auth) {
#ifndef NDEBUG
        std::cout << "no-auth\n" << std::flush;
#endif
        return;
    }
    if (!FLAGS_user.empty()) {
#ifndef NDEBUG
        std::cout << "auth user:= " << FLAGS_user << '\n' << std::flush;
#endif
        return;
    }
    if (!FLAGS_auth_token.empty()) {
#ifndef NDEBUG
        std::cout << "auth token: " << FLAGS_auth_token << '\n' << std::flush;
#endif
        return;
    }
    if (!FLAGS_credentials.empty()) {
#ifndef NDEBUG
        std::cout << "auth credentials: " << FLAGS_credentials << '\n' << std::flush;
#endif
        return;
    }

    if (auto* token = getenv("TSURUGI_AUTH_TOKEN"); token != nullptr) {
#ifndef NDEBUG
        std::cout << "auth token fron TSURUGI_AUTH_TOKEN: " << token << '\n' << std::flush;
#endif
        return;
    }
}

static std::string prompt(std::string_view msg)
{
    struct termio tty{};
    struct termio tty_save{};

    ioctl(STDIN_FILENO, TCGETA, &tty);  // NOLINT
    tty_save = tty;

    tty.c_lflag &= ~ECHO;   // NOLINT
    tty.c_lflag |= ECHONL;  // NOLINT

    ioctl(STDIN_FILENO, TCSETAF, &tty);  // NOLINT

    std::cout << msg << std::flush;
    std::string rtnv{};
    while(true) {
        int chr = getchar();
        if (chr == '\n') {
            break;
        }
        rtnv.append(1, static_cast<char>(chr));
    }
    ioctl(STDIN_FILENO, TCSETAF, &tty_save);  // NOLINT

    return rtnv;
}

std::optional<std::filesystem::path> default_credential_path() {
    if (auto* name = getenv("TSURUGI_HOME"); name != nullptr) {
        std::filesystem::path path{name};
        path /= ".tsurugidb";
        path /= "credentials.key";
        return path;
    }
    return std::nullopt;
}

void add_credential(tateyama::proto::endpoint::request::ClientInformation& information, const std::filesystem::path& path) {
    std::ifstream file(path.string().c_str());
    if (!file.is_open()) {
        return;
    }

    std::string encrypted_credential{};
    std::getline(file, encrypted_credential);  // use first line only
    file.close();

    if (!encrypted_credential.empty()) {
        (information.mutable_credential())->set_encrypted_credential(encrypted_credential);
    }
}

void add_credential(tateyama::proto::endpoint::request::ClientInformation& information, const std::function<std::optional<std::string>()>& key_func) {
    if (!FLAGS_auth) {
        return;
    }
    if (!FLAGS_user.empty()) {
        auto key_opt = key_func();
        if (key_opt) {
            rsa_encrypter rsa{key_opt.value()};

            std::string c{};
            std::string p = prompt("password: ");
            rsa.encrypt(FLAGS_user + "\n" + p, c);
            (information.mutable_credential())->set_encrypted_credential(base64_encode(c));
        }
        return;
    }
    if (!FLAGS_auth_token.empty()) {
        (information.mutable_credential())->set_remember_me_credential(FLAGS_auth_token);
        return;
    }
    if (!FLAGS_credentials.empty()) {
        add_credential(information, std::filesystem::path(FLAGS_credentials));
        return;
    }

    if (auto* token = getenv("TSURUGI_AUTH_TOKEN"); token != nullptr) {
        (information.mutable_credential())->set_remember_me_credential(token);
        return;
    }
    if (auto cred_opt = default_credential_path(); cred_opt) {
        add_credential(information, cred_opt.value());
    }
}

tgctl::return_code credentials() {
    if (auto* name = getenv("TSURUGI_HOME"); name != nullptr) {
        std::filesystem::path path{name};
        path /= ".tsurugidb";
        path /= "credentials.json";
        return credentials(path);
    }
    std::cerr << "the environment variable TSURUGI_HOME is not defined\n" << std::flush;
    return tateyama::tgctl::return_code::err;
}

tgctl::return_code credentials([[maybe_unused]] const std::filesystem::path& path) {
    std::cerr << "not implemented yet\n" << std::flush;
    return tateyama::tgctl::return_code::err;
}

}  // tateyama::authentication
