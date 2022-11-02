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

#include <boost/filesystem/path.hpp>
#include <openssl/md5.h>

#include <tateyama/api/configuration.h>

namespace tateyama::bootstrap::utils {

static const boost::filesystem::path CONF_FILE_NAME = boost::filesystem::path("tsurugi.ini");  // NOLINT
static const boost::filesystem::path PID_DIR = boost::filesystem::path("/tmp");  // NOLINT
static const std::string_view  PID_FILE_NAME = "tsurugi";
static const char *ENV_ENTRY = "TGDIR";  // NOLINT

class bootstrap_configuration {
public:
    static bootstrap_configuration create_bootstrap_configuration(std::string_view file) {
        try {
            return bootstrap_configuration(file);
        } catch (std::runtime_error &e) {
            LOG(ERROR) << e.what();
            return bootstrap_configuration();
        }
    }
    static std::shared_ptr<tateyama::api::configuration::whole> create_configuration(std::string_view file) {
        auto bst_conf = create_bootstrap_configuration(file);
        if (bst_conf.valid()) {
            return bst_conf.create_configuration();
        }
        return nullptr;
    }
    std::shared_ptr<tateyama::api::configuration::whole> create_configuration() {
        if (!property_file_absent_) {
            return tateyama::api::configuration::create_configuration(conf_file_.string());
        }
        return nullptr;
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
    std::shared_ptr<tateyama::api::configuration::whole> configuration_;
    bool property_file_absent_{};
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
                conf_file_ = std::string("");
                property_file_absent_ = true;
            }
        }
        // do sanity check for conf_file_
        if (!property_file_absent_) {
            boost::system::error_code error;
            const bool result = boost::filesystem::exists(conf_file_, error);
            if (!result || error) {
                throw std::runtime_error(std::string("cannot find configuration file: ") + conf_file_.string());
            }
            if (boost::filesystem::is_directory(conf_file_)) {
                throw std::runtime_error(conf_file_.string() + " is a directory");
            }
        }
        std::string pid_file_name(PID_FILE_NAME);
        pid_file_name += "-";
        pid_file_name += digest(boost::filesystem::canonical(conf_file_).string());
        pid_file_name += ".pid";
        lock_file_ = PID_DIR / boost::filesystem::path(pid_file_name);
        valid_ = true;
    }
    std::string digest(const std::string& path_string) {
        MD5_CTX mdContext;
        unsigned int len = path_string.length();
        std::array<unsigned char, MD5_DIGEST_LENGTH> digest{};

        std::string s{};
        s.resize(len + 1);
        path_string.copy(s.data(), len + 1);
        s.at(len) = '\0';
        MD5_Init(&mdContext);
        MD5_Update (&mdContext, s.data(), len);
        MD5_Final(digest.data(), &mdContext);

        std::string digest_string{};
        digest_string.resize(MD5_DIGEST_LENGTH * 2);
        auto it = digest_string.begin();
        for(unsigned char c : digest) {
            std::uint32_t n = c & 0xf0U;
            n >>= 8U;
            *(it++) = (n < 0xa) ? ('0' + n) : ('a' + (n - 0xa));
            n = c & 0xfU;
            *(it++) = (n < 0xa) ? ('0' + n) : ('a' + (n - 0xa));
        }
        return digest_string;
    }
};

} // namespace tateyama::bootstrap::utils
