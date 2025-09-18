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

#include <tateyama/logging.h>

#include <tateyama/proto/endpoint/request.pb.h>
#include "tateyama/transport/transport.h"
#include "tateyama/tgctl/runtime_error.h"
#include "rsa.h"
#include "authentication.h"

DEFINE_string(user, "", "user name for authentication");  // NOLINT
DEFINE_string(auth_token, "", "authentication token");  // NOLINT
DEFINE_string(credentials, "", "path to credentials");  // NOLINT
DEFINE_bool(_auth, true, "--no-auth when authentication is not used");  // NOLINT  false when --no-auth is specified
DEFINE_bool(overwrite, false, "overwrite the credential file");  // NOLINT  for --overwrite
DEFINE_bool(_overwrite, true, "overwrite the credential file");  // NOLINT  for --no-overwrite
DEFINE_int32(expiration, 90, "number of days until credentials expire");  // NOLINT

namespace tateyama::authentication {

constexpr static int FORMAT_VERSION = 1;
constexpr static int MAX_EXPIRATION = 365;  // Maximum expiration date that can be specified with tgctl credentials

void auth_options() {
    if (!FLAGS__auth) {
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

struct termio* saved{nullptr};  // NOLINT
static void sigint_handler([[maybe_unused]] int sig) {
    if (saved) {
        ioctl(STDIN_FILENO, TCSETAF, saved);  // NOLINT
    }
    throw tgctl::runtime_error(tateyama::monitor::reason::interrupted, "key input has been interrupted by the user");
}

static std::string prompt(std::string_view msg, bool display = false)
{
    struct termio tty{};
    struct termio tty_save{};

    if (!display) {
        if (signal(SIGINT, sigint_handler) == SIG_ERR) {  // NOLINT
            LOG(ERROR) << "cannot register signal handler";
        }
        ioctl(STDIN_FILENO, TCGETA, &tty);  // NOLINT
        tty_save = tty;
        saved = &tty_save;

        tty.c_lflag &= ~ECHO;   // NOLINT
        tty.c_lflag |= ECHONL;  // NOLINT

        ioctl(STDIN_FILENO, TCSETAF, &tty);  // NOLINT
    }

    if (isatty(STDIN_FILENO) == 1) {
        std::cout << msg << std::flush;
    }
    std::string rtnv{};
    while(true) {
        int chr = getchar();
        if (chr == '\n') {
            break;
        }
        rtnv.append(1, static_cast<char>(chr));
    }

    if (!display) {
        ioctl(STDIN_FILENO, TCSETAF, &tty_save);  // NOLINT
        if (signal(SIGINT, SIG_DFL) == SIG_ERR) {  // NOLINT
            LOG(ERROR) << "cannot register signal handler";
        }
    }

    return rtnv;
}

std::optional<std::filesystem::path> default_credential_path() {
    if (auto* name = getenv("HOME"); name != nullptr) {
        std::filesystem::path path{name};
        path /= ".tsurugidb";
        path /= "credentials.key";
        return path;
    }
    return std::nullopt;
}

static void add_credential(tateyama::proto::endpoint::request::ClientInformation& information, const std::filesystem::path& path) {
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

class credential_helper_class {
public:
    std::string get_json_text(const std::string& user, const std::string& password) {
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

    [[nodiscard]] std::string expiration_date() const noexcept {
        return expiration_date_string_;
    }

    // for tgctl credentials
    void set_expiration(std::int32_t expiration) noexcept {
        expiration_ = std::chrono::hours(expiration * 24);
    }

    bool check_not_more_than_one() {
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

private:
    std::chrono::minutes expiration_{300}; // 5 minutes for connecting a normal session.
    std::string expiration_date_string_{};

    std::string& expiration() {
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
};

static credential_helper_class credential_helper{};  // NOLINT

void add_credential(tateyama::proto::endpoint::request::ClientInformation& information, const std::function<std::optional<std::string>()>& key_func) {
    if (!credential_helper.check_not_more_than_one()) {
        throw tgctl::runtime_error(tateyama::monitor::reason::authentication_failure, "more than one credential options are specified");
    }
    if (!FLAGS__auth) {
        return;
    }
    if (!FLAGS_user.empty()) {
        auto key_opt = key_func();
        if (key_opt) {
            rsa_encrypter rsa{key_opt.value()};

            std::string c{};
            rsa.encrypt(credential_helper.get_json_text(FLAGS_user, prompt("password: ")), c);
            std::string encrypted_credential = base64_encode(c);
            (information.mutable_credential())->set_encrypted_credential(encrypted_credential);
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

static tgctl::return_code credentials(const std::filesystem::path& path) {
    if (FLAGS_expiration < 0 || FLAGS_expiration > MAX_EXPIRATION) {
        std::cerr << "--expiration should be greater then or equal to 0 and less than or equal to " << MAX_EXPIRATION << '\n' << std::flush;
        return tateyama::tgctl::return_code::err;
    }
    if (!FLAGS_credentials.empty()) {
        std::cerr << "--credentials option is invalid for credentials subcommand\n" << std::flush;
        return tateyama::tgctl::return_code::err;
    }
    if (!FLAGS_auth_token.empty()) {
        std::cerr << "--auth_token option is invalid for credentials subcommand\n" << std::flush;
        return tateyama::tgctl::return_code::err;
    }

    // --overwrite      x  x  o  o
    // FLAGS_overwrite  f  f  t  t
    // --no-overwrite   x  o  x  o
    // FLAGS__overwrite t  f  t  f
    // overwrite?       x  x  o  -
    //
    if (!FLAGS__overwrite && FLAGS_overwrite) {
        std::cerr << "both --overwrite and --no-overwrite are specified\n" << std::flush;
        return tateyama::tgctl::return_code::err;
    }

    bool overwrite = FLAGS_overwrite && FLAGS__overwrite;
    if (!overwrite && std::filesystem::exists(path)) {
        std::cerr << "file '" << path.string() << "' already exists\n" << std::flush;
        return tateyama::tgctl::return_code::err;
    }
    if (FLAGS_user.empty()) {
        FLAGS_user = prompt("user: ", true);
    }

    try {
        credential_helper.set_expiration(FLAGS_expiration);
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_routing);  // service_id is meaningless here

        const std::string& encrypted_credential = transport->encrypted_credential(); // Valid while the transport is alive
        if (encrypted_credential.empty()) {
            std::cerr << "cannot obtain encrypted credential\n" << std::flush;
            return tateyama::tgctl::return_code::err;
        }
        std::ofstream ofs(path.string());
        if (!ofs) {
            std::cerr << "cannot open '" << path.string() << "'\n" << std::flush;
            return tateyama::tgctl::return_code::err;
        }

        ofs << encrypted_credential << '\n';
        if (auto d = credential_helper.expiration_date(); !d.empty()) {
            ofs << d << '\n';
        }
        ofs.close();
        std::error_code ec{};
        std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        if (!ec) {
            return tateyama::tgctl::return_code::ok;
        }
        std::cerr << "cannot set permission to '" << path.string() << "'\n" << std::flush;
        std::filesystem::remove(path);
        return tateyama::tgctl::return_code::err;
    } catch (std::runtime_error &ex) {
        std::cerr << "cannot establish session with the user and the password: " << ex.what() << "\n" << std::flush;
        return tateyama::tgctl::return_code::err;
    }
}

tgctl::return_code credentials() {
    if (auto cp_opt = default_credential_path(); cp_opt) {
        auto& cp = cp_opt.value();
        auto parent = cp.parent_path();
        if (!std::filesystem::exists(parent)) {
            std::filesystem::create_directory(parent);
        }
        if (!std::filesystem::is_directory(parent)) {
            std::cerr << "'" << parent.string() << "' is not a directory\n" << std::flush;
            return tateyama::tgctl::return_code::err;
        }
        return credentials(cp_opt.value());
    }
    std::cerr << "the environment variable HOME is not defined\n" << std::flush;
    return tateyama::tgctl::return_code::err;
}

tgctl::return_code credentials(const std::string& file_name) {
    std::filesystem::path path{file_name};

    if (std::filesystem::exists(path) && !std::filesystem::is_regular_file(path)) {
        std::cerr << "'" << file_name << "' is not a regular file\n" << std::flush;
        return tateyama::tgctl::return_code::err;
    }

    return tateyama::authentication::credentials(path);
}

}  // tateyama::authentication
