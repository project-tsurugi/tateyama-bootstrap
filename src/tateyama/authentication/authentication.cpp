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
#include <sys/ioctl.h>
#include <asm/termbits.h>

#include <gflags/gflags.h>

#include <tateyama/logging.h>

#include <tateyama/proto/endpoint/request.pb.h>
#include "tateyama/tgctl/runtime_error.h"
#include "rsa.h"
#include "base64.h"
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
        std::cout << "auth token fron pre_defined_auth_file (" << pre_defined_auth_file.string() << "): " << contents << '\n' << std::flush;
#endif
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


void add_credential(tateyama::proto::endpoint::request::ClientInformation& information, const std::function<std::optional<std::string>()>& func) {
    if (!FLAGS_auth) {
        return;
    }
    if (!FLAGS_user.empty()) {
        auto key_opt = func();
        if (key_opt) {
            rsa_encrypter rsa{key_opt.value()};

            std::string u{};
            rsa.encrypt(FLAGS_user, u);

            std::string p{};
            rsa.encrypt(prompt("password: "), p);

            (information.mutable_credential())->set_encrypted_credential(base64_encode(u) + "." + base64_encode(p));
        }
        return;
    }
    if (!FLAGS_auth_token.empty()) {
        (information.mutable_credential())->set_remember_me_credential(FLAGS_auth_token);
        return;
    }
    if (!FLAGS_credentials.empty()) {
#ifndef NDEBUG
        std::cout << "auth credentials: " << FLAGS_credentials << '\n' << std::flush;
#endif
        return;
    }
    if (auto* token = getenv("TSURUGI_AUTH_TOKEN"); token != nullptr) {
        (information.mutable_credential())->set_remember_me_credential(token);
        return;
    }
}

}  // tateyama::authentication
