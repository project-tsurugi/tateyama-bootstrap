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
#include "authentication.h"

DEFINE_string(user, "", "user name for authentication");  // NOLINT
DEFINE_string(auth_token, "", "authentication token");  // NOLINT
DEFINE_string(credentials, "", "path to credentials");  // NOLINT
DEFINE_bool(_auth, true, "--no-auth when authentication is not used");  // NOLINT  false when --no-auth is specified
DEFINE_bool(overwrite, false, "overwrite the credential file");  // NOLINT  for --overwrite
DEFINE_bool(_overwrite, true, "overwrite the credential file");  // NOLINT  for --no-overwrite
DEFINE_int32(expiration, 90, "number of days until credentials expire");  // NOLINT
DECLARE_string(conf);  // NOLINT

namespace tateyama::authentication {

constexpr static int FORMAT_VERSION = 1;
constexpr static int MAX_EXPIRATION = 365;  // Maximum expiration date that can be specified with tgctl credentials

struct termio* saved{nullptr};  // NOLINT

static void sigint_handler([[maybe_unused]] int sig) {
    if (saved) {
        ioctl(STDIN_FILENO, TCSETAF, saved);  // NOLINT
    }
    throw tgctl::runtime_error(tateyama::monitor::reason::interrupted, "key input has been interrupted by the user");
}

static std::string prompt(std::string_view msg, bool display = false) {
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

enum class credential_type {
    not_defined = 0,
    no_auth,
    user_password,
    auth_token,
    file,
    disabled
};

class credential_helper_class {
public:
    void set_disabled() {
        type_ = credential_type::disabled;
    }    
    void set_no_auth() {
        type_ = credential_type::no_auth;
    }
    void set_user_password(const std::string& user, const std::string& password) {
        type_ = credential_type::user_password;
        json_text_ = get_json_text(user, password);
    }
    void set_auth_token(const std::string& auth_token) {
        type_ = credential_type::auth_token;
        auth_token_ = auth_token;
    }
    void set_file_credential(const std::filesystem::path& path) {
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

    std::optional<std::filesystem::path> default_credential_path() {
        if (auto* name = getenv("HOME"); name != nullptr) {
            std::filesystem::path path{name};
            path /= ".tsurugidb";
            path /= "credentials.key";
            return path;
        }
        return std::nullopt;
    }

    void add_credential(tateyama::proto::endpoint::request::ClientInformation& information, const std::function<std::optional<std::string>()>& key_func) {
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

    static std::optional<std::string> check_username(const std::optional<std::string>& name_opt) {
        if (!name_opt) {
            return std::nullopt;
        }
        const std::string& name = name_opt.value();

        if(regex_match(name, boost::regex(R"(\s.*)"))) {
            throw authentication_exception("invalid user name (begin with whitespace)", tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR);
        }
        if(regex_match(name, boost::regex(R"(.*\s)"))) {
            throw authentication_exception("invalid user name (end with whitespace)", tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR);
        }
        if (name.length() > MAXIMUM_USERNAME_LENGTH) {
            throw authentication_exception("invalid user name (too long)", tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR);
        }
        if(regex_match(name, boost::regex(R"([\x20-\x7E\x80-\xFF]*)"))) {
            return name_opt;
        }
        throw authentication_exception("invalid user name (includes invalid charactor)", tateyama::proto::diagnostics::Code::AUTHENTICATION_ERROR);
    }

private:
    credential_type type_{};
    std::string json_text_{};
    std::string auth_token_{};
    std::string encrypted_credential_{};

    std::chrono::minutes expiration_{300}; // 5 minutes for connecting a normal session.
    std::string expiration_date_string_{};

    /**
     * @brief maximum length for valid username
     */
    constexpr static std::size_t MAXIMUM_USERNAME_LENGTH = 1024;

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

    void set_encrypted_credential(const std::string& encrypted_credential) {
        type_ = credential_type::file;
        encrypted_credential_ = encrypted_credential;
    }
};

static credential_helper_class credential_helper{};  // NOLINT


//
// implement tgctl credentials
//
void auth_options() {
    auto whole = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf).get_configuration();
    auto authentication_section = whole->get_section("authentication");
    auto enabled_opt = authentication_section->get<bool>("enabled");
    if (!enabled_opt || !enabled_opt.value()) {
        credential_helper.set_disabled();
        return;
    }

    if (!credential_helper.check_not_more_than_one()) {
        throw tgctl::runtime_error(tateyama::monitor::reason::authentication_failure, "more than one credential options are specified");
    }

    if (!FLAGS__auth) {
        credential_helper.set_no_auth();
        return;
    }
    if (!FLAGS_user.empty()) {
        credential_helper.set_user_password(FLAGS_user, prompt("password: "));
        return;
    }
    if (!FLAGS_auth_token.empty()) {
        credential_helper.set_auth_token(FLAGS_auth_token);
        return;
    }
    if (!FLAGS_credentials.empty()) {
        credential_helper.set_file_credential(std::filesystem::path(FLAGS_credentials));
        return;
    }

    if (auto* token = getenv("TSURUGI_AUTH_TOKEN"); token != nullptr) {
        credential_helper.set_auth_token(token);
        return;
    }
    if (auto cred_opt = credential_helper.default_credential_path(); cred_opt) {
        credential_helper.set_file_credential(cred_opt.value());
    }
}

void add_credential(tateyama::proto::endpoint::request::ClientInformation& information, const std::function<std::optional<std::string>()>& key_func) {
    credential_helper.add_credential(information, key_func);
}


//
// implement tgctl credentials
//
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
        auth_options();

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
    if (auto cp_opt = credential_helper.default_credential_path(); cp_opt) {
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


// for authenticate()

class url_parser {
public:
    explicit url_parser(const std::string& url) {
        boost::regex ex("(http|https)://([^/ :]+):?([^/ ]*)(/?[^ #?]*)\\x3f?([^ #]*)#?([^ ]*)");
        boost::cmatch what;

        if(regex_match(url.c_str(), what, ex)) {
            protocol = std::string(what[1].first, what[1].second);
            domain   = std::string(what[2].first, what[2].second);
            port     = std::string(what[3].first, what[3].second);
            path     = std::string(what[4].first, what[4].second);
            query    = std::string(what[5].first, what[5].second);
        }
    }

    std::string& get_domain() { return domain; };
    std::string& get_port() { return port; };
    std::string& get_path() { return path; };

private:
    std::string protocol{};
    std::string domain{};
    std::string port{};
    std::string path{};
    std::string query{};
};

void authenticate(tateyama::api::configuration::section* section) {
    if (auto enabled_opt = section->get<bool>("enabled"); enabled_opt) {
        if (!enabled_opt.value()) {
            return; // OK
        }
    }

    auto request_timeout_opt = section->get<int>("request_timeout");
    auto url_opt = section->get<std::string>("url");

    if (!request_timeout_opt || !url_opt) {
        throw tgctl::runtime_error(tateyama::monitor::reason::server, "error in config file");
    }

    url_parser url(url_opt.value());

    std::string& port = url.get_port();
    auto auth_client = std::make_unique<client>(url.get_domain(),
                                                port.empty() ? 80 : stoi(port), url.get_path(),
                                                static_cast<std::chrono::milliseconds>(lround(request_timeout_opt.value() * 1000)));
    try {
        authentication::auth_options();
        tateyama::proto::endpoint::request::ClientInformation information{};

        std::string encryption_key{};
        if (auto encryption_key_opt = auth_client->get_encryption_key(); encryption_key_opt) {
            auto& key_pair = encryption_key_opt.value();
            if (key_pair.first == "RSA") {
                encryption_key = key_pair.second;
            }
        }

        credential_helper.add_credential(information, [&encryption_key](){
            if (!encryption_key.empty()) {
                return std::optional<std::string>{encryption_key};
            }
            return std::optional<std::string>{std::nullopt};
        });

        const tateyama::proto::endpoint::request::Credential& credential = information.credential();
        switch (credential.credential_opt_case()) {
        case tateyama::proto::endpoint::request::Credential::CredentialOptCase::kEncryptedCredential:
            if (auto token_opt = auth_client->verify_encrypted(credential.encrypted_credential()); token_opt) {
                if (!encryption_key.empty()) {
                    auto handler = std::make_unique<token_handler>(token_opt.value(), encryption_key);
                    if (credential_helper_class::check_username(handler->tsurugi_auth_name())) {
                        return; // OK
                    }
                    if (auto name_opt = handler->tsurugi_auth_name(); name_opt) {
                        std::cerr << "illegal user name: " << name_opt.value() << '\n' << std::flush;
                    } else {
                        std::cerr << "illegal user name\n" << std::flush;
                    }
                }
            }
            break;
        case tateyama::proto::endpoint::request::Credential::CredentialOptCase::kRememberMeCredential:
        {
            const auto& token = credential.remember_me_credential();
            if (auto token_opt = auth_client->verify_token(token); token_opt) {
                if (!encryption_key.empty()) {
                    auto handler = std::make_unique<token_handler>(token, encryption_key);
                    auto ns = std::chrono::system_clock::now().time_since_epoch();
                    if (std::chrono::duration_cast<std::chrono::seconds>(ns).count() < handler->expiration_time()) {
                        if (credential_helper_class::check_username(handler->tsurugi_auth_name())) {
                            return; // OK
                        }
                        if (auto name_opt = handler->tsurugi_auth_name(); name_opt) {
                            std::cerr << "illegal user name: " << name_opt.value() << '\n' << std::flush;
                        } else {
                            std::cerr << "illegal user name\n" << std::flush;
                        }
                    }
                }
            }
            break;
        }
        default:
            break;
        }
        throw tgctl::runtime_error(tateyama::monitor::reason::authentication_failure, "authentication failed");
    } catch (std::exception &ex) {
        throw tgctl::runtime_error(tateyama::monitor::reason::authentication_failure, ex.what());
    }
}

}  // tateyama::authentication
