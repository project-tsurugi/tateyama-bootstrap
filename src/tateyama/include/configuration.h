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
#include <openssl/md5.h>

#include <tateyama/api/configuration.h>

namespace tateyama::bootstrap::utils {

static const boost::filesystem::path CONF_FILE_NAME = boost::filesystem::path("tsurugi.ini");
static const boost::filesystem::path PID_DIR = boost::filesystem::path("/tmp");
static const std::string_view  PID_FILE_NAME = "tsurugi";  // NOLINT
static const char *ENV_ENTRY = "TGDIR";  // NOLINT

class bootstrap_configuration {
public:
    bootstrap_configuration(std::string_view f) {
        // tsurugi.ini
        if (!f.empty()) {
            conf_file_ = boost::filesystem::path(std::string(f).c_str());
        } else {
            if (auto env = getenv(ENV_ENTRY); env != nullptr) {
                conf_file_ = boost::filesystem::path(env) / CONF_FILE_NAME;
            } else {
                conf_file_ = std::string("");
                property_file_absent_ = true;
            }
        }
        std::string pid_file_name(PID_FILE_NAME);
        pid_file_name += suffix(boost::filesystem::canonical(conf_file_).string());
        pid_file_name += ".pid";
        lock_file_ = PID_DIR / boost::filesystem::path(pid_file_name);
    }
    std::shared_ptr<tateyama::api::configuration::whole> create_configuration() {
        if (!property_file_absent_) {
            return tateyama::api::configuration::create_configuration(conf_file_.string());
        }
        return nullptr;
    }
    boost::filesystem::path lock_file() {
        return lock_file_;
    }

private:
    boost::filesystem::path conf_file_;
    boost::filesystem::path lock_file_;
    std::shared_ptr<tateyama::api::configuration::whole> configuration_;
    bool property_file_absent_{};

    std::string suffix(const std::string& path_string) {
        MD5_CTX mdContext;
        unsigned int len = path_string.length();
        std::uint8_t digest[MD5_DIGEST_LENGTH];

        char s[len + 1];
        path_string.copy(s, len + 1);
        s[len] = '\0';
        MD5_Init(&mdContext);
        MD5_Update (&mdContext, reinterpret_cast<unsigned char*>(s), len);
        MD5_Final(digest, &mdContext);

        std::string digest_string{};
        digest_string.resize((MD5_DIGEST_LENGTH * 2) + 1);
        auto it = digest_string.begin();
        *(it++) = '-';
        std::cout << std::hex;
        for(std::size_t i = 0; i < MD5_DIGEST_LENGTH; i++) {
            std::uint32_t n = (digest[i] >> 8) & 0xf;
            *(it++) = (n < 0xa) ? ('0' + n) : ('a' + (n - 0xa));
            n = digest[i] & 0xf;
            *(it++) = (n < 0xa) ? ('0' + n) : ('a' + (n - 0xa));
        }
        return digest_string;
    }
};

} // namespace tateyama::bootstrap::utils
