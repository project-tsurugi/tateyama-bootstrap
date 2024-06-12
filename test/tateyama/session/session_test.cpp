/*
 * Copyright 2022-2024 Project Tsurugi.
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
#include <sstream>
#include <array>
#include <sys/types.h>
#include <unistd.h>

#include <boost/thread/barrier.hpp>

#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>
#include <tateyama/framework/component_ids.h>
#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/test_utils/server_mock.h"

namespace tateyama::test_utils {

template<>
inline void server_mock::request_message<tateyama::proto::session::request::SessionGet>(tateyama::proto::session::request::SessionGet& rq) {
    tateyama::proto::session::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::session::request::Request::CommandCase::kSessionGet);
    rq = r.session_get();
}

template<>
inline void server_mock::request_message<tateyama::proto::session::request::SessionList>(tateyama::proto::session::request::SessionList& rq) {
    tateyama::proto::session::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::session::request::Request::CommandCase::kSessionList);
    rq = r.session_list();
}

template<>
inline void server_mock::request_message<tateyama::proto::session::request::SessionShutdown>(tateyama::proto::session::request::SessionShutdown& rq) {
    tateyama::proto::session::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::session::request::Request::CommandCase::kSessionShutdown);
    rq = r.session_shutdown();
}

template<>
inline void server_mock::request_message<tateyama::proto::session::request::SessionSetVariable>(tateyama::proto::session::request::SessionSetVariable& rq) {
    tateyama::proto::session::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::session::request::Request::CommandCase::kSessionSetVariable);
    rq = r.session_set_variable();
}


}  // tateyama::test_utils


namespace tateyama::session {

class session_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("session_test", 20201);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("session_test", bst_conf.digest(), sync_);
        sync_.wait();
    }

    virtual void TearDown() {
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};
    std::unique_ptr<tateyama::test_utils::server_mock> server_mock_{};
    boost::barrier sync_{2};

    std::string read_pipe(FILE* fp) {
        std::stringstream ss{};
        int c{};
        while ((c = std::fgetc(fp)) != EOF) {
            ss << static_cast<char>(c);
        }
        return ss.str();
    }

    std::string to_timepoint_string(std::uint64_t msu) {
        std::chrono::time_point<std::chrono::system_clock> e0{};
        std::chrono::time_point<std::chrono::system_clock> t = e0 + std::chrono::milliseconds(static_cast<std::int64_t>(msu));

        std::stringstream stream;
        time_t epoch_seconds = std::chrono::system_clock::to_time_t(t);
        struct tm buf;
        gmtime_r(&epoch_seconds, &buf);
        stream << std::put_time(&buf, "%FT%TZ");
        return stream.str();
    }
};

TEST_F(session_test, session_list) {
    std::string command;
    FILE *fp;

    auto now_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    {
        tateyama::proto::session::response::SessionList session_list{};
        auto* success = session_list.mutable_success();
        auto *entry = success->add_entries();
        entry->set_session_id(":123456");
        entry->set_label("test_label");
        entry->set_application("session_test");
        entry->set_user("test_user");
        entry->set_start_at(now_stamp);
        entry->set_connection_type(std::string("ipc"));
        entry->set_connection_info(std::to_string(getpid()));
        server_mock_->push_response(session_list.SerializeAsString());
    }

    command = "tgctl session list --verbose --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl session list" << std::endl;
    }
    auto result = read_pipe(fp);
//    std::cout << result << std::flush;
    EXPECT_NE(std::string::npos, result.find(":123456"));
    EXPECT_NE(std::string::npos, result.find("test_label"));
    EXPECT_NE(std::string::npos, result.find("session_test"));
    EXPECT_NE(std::string::npos, result.find("test_user"));
    EXPECT_NE(std::string::npos, result.find(to_timepoint_string(now_stamp)));
    EXPECT_NE(std::string::npos, result.find("ipc"));
    EXPECT_NE(std::string::npos, result.find(std::to_string(getpid())));

    EXPECT_EQ(tateyama::framework::service_id_session, server_mock_->component_id());
    tateyama::proto::session::request::SessionList rq{};
    server_mock_->request_message(rq);
}

TEST_F(session_test, session_show) {
    std::string command;
    FILE *fp;

    auto now_stamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    {
        tateyama::proto::session::response::SessionGet session_get{};
        auto* success = session_get.mutable_success();
        auto *entry = success->mutable_entry();
        entry->set_session_id("123456");
        entry->set_label("test_label");
        entry->set_application("session_test");
        entry->set_user("test_user");
        entry->set_start_at(now_stamp);
        entry->set_connection_type(std::string("ipc"));
        entry->set_connection_info(std::to_string(getpid()));
        server_mock_->push_response(session_get.SerializeAsString());
    }

    command = "tgctl session show :123456 --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl session show" << std::endl;
    }
    auto result = read_pipe(fp);
//    std::cout << result << std::flush;
    EXPECT_NE(std::string::npos, result.find("123456"));
    EXPECT_NE(std::string::npos, result.find("test_label"));
    EXPECT_NE(std::string::npos, result.find("session_test"));
    EXPECT_NE(std::string::npos, result.find("test_user"));
    EXPECT_NE(std::string::npos, result.find(to_timepoint_string(now_stamp)));
    EXPECT_NE(std::string::npos, result.find("ipc"));
    EXPECT_NE(std::string::npos, result.find(std::to_string(getpid())));

    EXPECT_EQ(tateyama::framework::service_id_session, server_mock_->component_id());
    tateyama::proto::session::request::SessionGet rq{};
    server_mock_->request_message(rq);
    EXPECT_EQ(":123456", rq.session_specifier());
}

TEST_F(session_test, session_shutdown_graceful) {
    std::string command;
    FILE *fp;

    {
        tateyama::proto::session::response::SessionShutdown session_sd{};
        auto* success = session_sd.mutable_success();
        server_mock_->push_response(session_sd.SerializeAsString());
    }

    command = "tgctl session shutdown :123456 --conf ";
    command += helper_->conf_file_path();
    command += " --graceful";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl session shutdown" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_session, server_mock_->component_id());
    tateyama::proto::session::request::SessionShutdown rq{};
    server_mock_->request_message(rq);
    EXPECT_EQ(":123456", rq.session_specifier());
    EXPECT_EQ(tateyama::proto::session::request::SessionShutdownType::GRACEFUL, rq.request_type());
}

TEST_F(session_test, session_shutdown_forceful) {
    std::string command;
    FILE *fp;

    {
        tateyama::proto::session::response::SessionShutdown session_sd{};
        auto* success = session_sd.mutable_success();
        server_mock_->push_response(session_sd.SerializeAsString());
    }

    command = "tgctl session shutdown :123456 --conf ";
    command += helper_->conf_file_path();
    command += " --forceful";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl session shutdown" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_session, server_mock_->component_id());
    tateyama::proto::session::request::SessionShutdown rq{};
    server_mock_->request_message(rq);
    EXPECT_EQ(":123456", rq.session_specifier());
    EXPECT_EQ(tateyama::proto::session::request::SessionShutdownType::FORCEFUL, rq.request_type());
}

TEST_F(session_test, session_shutdown) {
    std::string command;
    FILE *fp;

    {
        tateyama::proto::session::response::SessionShutdown session_sd{};
        auto* success = session_sd.mutable_success();
        server_mock_->push_response(session_sd.SerializeAsString());
    }

    command = "tgctl session shutdown :123456 --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl session shutdown" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_session, server_mock_->component_id());
    tateyama::proto::session::request::SessionShutdown rq{};
    server_mock_->request_message(rq);
    EXPECT_EQ(":123456", rq.session_specifier());
    EXPECT_EQ(tateyama::proto::session::request::SessionShutdownType::SESSION_SHUTDOWN_TYPE_NOT_SET, rq.request_type());
}

TEST_F(session_test, session_set) {
    std::string command;
    FILE *fp;

    {
        tateyama::proto::session::response::SessionSetVariable session_set{};
        auto* success = session_set.mutable_success();
        server_mock_->push_response(session_set.SerializeAsString());
    }

    command = "tgctl session set :123456 test_variable test_value --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl session set" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_session, server_mock_->component_id());
    tateyama::proto::session::request::SessionSetVariable rq{};
    server_mock_->request_message(rq);
    EXPECT_EQ(":123456", rq.session_specifier());
    EXPECT_EQ("test_variable", rq.name());
    EXPECT_EQ("test_value", rq.value());
}

}  // namespace tateyama::session
