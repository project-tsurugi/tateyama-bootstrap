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

#include "test_root.h"

#include <iostream>
#include <chrono>
#include <string_view>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <boost/process/system.hpp>
#include <boost/process/io.hpp>
#include <boost/process/pipe.hpp>
# include <boost/iostreams/copy.hpp>
#include <boost/thread/barrier.hpp>

#include <tateyama/proto/session/response.pb.h>
#include <tateyama/framework/component_ids.h>
#include "tateyama/configuration/bootstrap_configuration.h"

#include "tateyama/test_utils/server_mock.h"
#include "crypto/token.h"

namespace tateyama::authentication {

class authentication_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("authentication_test", 20401);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("authentication_test", bst_conf.digest(), sync_, true);
        sync_.wait();

        auto now_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        {
            tateyama::proto::session::response::SessionList session_list{};
            auto* success = session_list.mutable_success();
            auto *entry = success->add_entries();
            entry->set_session_id(":123456");
            entry->set_label("test_label");
            entry->set_application("authentication_test_usig_session_list");
            entry->set_user("test_user");
            entry->set_start_at(now_stamp);
            entry->set_connection_type(std::string("ipc"));
            entry->set_connection_info(std::to_string(getpid()));
            server_mock_->push_response(session_list.SerializeAsString());
        }
    }

    virtual void TearDown() {
        helper_->tear_down();
    }

    void do_test(const std::string& command, const std::string& pw, std::stringstream& ss) {
        std::cout << command << std::endl;

        boost::process::opstream enc;
        boost::process::ipstream dec;
        boost::process::child c(command, boost::process::std_in < enc, boost::process::std_out > dec);

        if (!pw.empty()) {
            enc.write(pw.data(), pw.length());
            enc.flush();
        }
        enc.pipe().close();

        boost::iostreams::copy(dec, ss);
        c.wait();
    }
    void do_test(const std::string& command, std::stringstream& ss) {
        do_test(command, "", ss);
    }

protected:
    std::unique_ptr<directory_helper> helper_{};
    std::unique_ptr<tateyama::test_utils::server_mock> server_mock_{};
    boost::barrier sync_{2};
};

TEST_F(authentication_test, no_auth) {
    std::string command;
    command = "tgctl session list --verbose --noauth --conf ";
    command += helper_->conf_file_path();

    std::stringstream ss;
    do_test(command, ss);

    EXPECT_FALSE(ss.str().find("authentication_test_usig_session_list") != std::string::npos);
}

TEST_F(authentication_test, user_password_success) {
    std::string command;
    command = "tgctl session list --user tsurugi --verbose --conf ";
    command += helper_->conf_file_path();

    std::stringstream ss;
    do_test(command, "password\n", ss);

    EXPECT_TRUE(ss.str().find("authentication_test_usig_session_list") != std::string::npos);
}

TEST_F(authentication_test, user_password_fail) {
    std::string command;
    command = "tgctl session list --user tsurugi --verbose --conf ";
    command += helper_->conf_file_path();

    std::stringstream ss;
    do_test(command, "wordpass\n", ss);  // wrong password

    EXPECT_FALSE(ss.str().find("authentication_test_usig_session_list") != std::string::npos);
}

TEST_F(authentication_test, token_success) {
    std::string command;
    command = "tgctl session list --auth-token ";
    command += crypto::token;
    command += " --verbose --conf ";
    command += helper_->conf_file_path();

    std::stringstream ss;
    do_test(command, ss);

    EXPECT_TRUE(ss.str().find("authentication_test_usig_session_list") != std::string::npos);
}

TEST_F(authentication_test, token_fail) {
    std::string command;
    command = "tgctl session list --auth-token ";
    command += "this_is_an_invalid_token";  // wrong token
    command += " --verbose --conf ";
    command += helper_->conf_file_path();

    std::stringstream ss;
    do_test(command, ss);

    EXPECT_FALSE(ss.str().find("authentication_test_usig_session_list") != std::string::npos);
}

TEST_F(authentication_test, env_token_success) {
    std::string command;
    command = "tgctl session list --verbose --conf ";
    command += helper_->conf_file_path();

    setenv("TSURUGI_AUTH_TOKEN", crypto::token, 1);
    std::stringstream ss;
    do_test(command, ss);

    EXPECT_TRUE(ss.str().find("authentication_test_usig_session_list") != std::string::npos);
}

TEST_F(authentication_test, env_token_fail) {
    std::string command;
    command = "tgctl session list --verbose --conf ";
    command += helper_->conf_file_path();

    setenv("TSURUGI_AUTH_TOKEN", "this_is_an_invalid_token", 1);
    std::stringstream ss;
    do_test(command, ss);

    EXPECT_FALSE(ss.str().find("authentication_test_usig_session_list") != std::string::npos);
}

}  // namespace tateyama::authentication
