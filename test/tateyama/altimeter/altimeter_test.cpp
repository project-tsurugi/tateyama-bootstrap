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

#include <tateyama/proto/altimeter/request.pb.h>
#include <tateyama/proto/altimeter/response.pb.h>
#include <tateyama/framework/component_ids.h>
#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/test_utils/server_mock.h"

namespace tateyama::test_utils {

template<>
inline void server_mock::request_message<tateyama::proto::altimeter::request::Configure>(tateyama::proto::altimeter::request::Configure& rq) {
    tateyama::proto::altimeter::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::altimeter::request::Request::CommandCase::kConfigure);
    rq = r.configure();
}

template<>
inline void server_mock::request_message<tateyama::proto::altimeter::request::LogRotate>(tateyama::proto::altimeter::request::LogRotate& rq) {
    tateyama::proto::altimeter::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::altimeter::request::Request::CommandCase::kLogRotate);
    rq = r.log_rotate();
}

}  // tateyama::test_utils


namespace tateyama::altimeter {

class altimeter_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("altimeter_test", 20301);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("altimeter_test", bst_conf.digest(), sync_);
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
};

// enable
TEST_F(altimeter_test, enable_event) {
    {
        tateyama::proto::altimeter::response::Configure rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter enable event --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter enable event" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::Configure rq{};
    server_mock_->request_message(rq);

    EXPECT_TRUE(rq.has_event_log());
    EXPECT_FALSE(rq.has_audit_log());

    auto& ls = rq.event_log();
    EXPECT_EQ(ls.enabled_opt_case(), tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::kEnabled);
    EXPECT_EQ(ls.level_opt_case(), tateyama::proto::altimeter::request::LogSettings::LevelOptCase::LEVEL_OPT_NOT_SET);
    EXPECT_EQ(ls.statement_duration_opt_case(), tateyama::proto::altimeter::request::LogSettings::StatementDurationOptCase::STATEMENT_DURATION_OPT_NOT_SET);
    EXPECT_EQ(ls.enabled(), true);
}

TEST_F(altimeter_test, enable_audit) {
    {
        tateyama::proto::altimeter::response::Configure rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter enable audit --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter enable audit" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::Configure rq{};
    server_mock_->request_message(rq);

    EXPECT_FALSE(rq.has_event_log());
    EXPECT_TRUE(rq.has_audit_log());

    auto& ls = rq.audit_log();
    EXPECT_EQ(ls.enabled_opt_case(), tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::kEnabled);
    EXPECT_EQ(ls.level_opt_case(), tateyama::proto::altimeter::request::LogSettings::LevelOptCase::LEVEL_OPT_NOT_SET);
    EXPECT_EQ(ls.statement_duration_opt_case(), tateyama::proto::altimeter::request::LogSettings::StatementDurationOptCase::STATEMENT_DURATION_OPT_NOT_SET);
    EXPECT_EQ(ls.enabled(), true);
}

// disable
TEST_F(altimeter_test, disable_event) {
    {
        tateyama::proto::altimeter::response::Configure rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter disable event --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter disable event" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::Configure rq{};
    server_mock_->request_message(rq);

    EXPECT_TRUE(rq.has_event_log());
    EXPECT_FALSE(rq.has_audit_log());

    auto& ls = rq.event_log();
    EXPECT_EQ(ls.enabled_opt_case(), tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::kEnabled);
    EXPECT_EQ(ls.level_opt_case(), tateyama::proto::altimeter::request::LogSettings::LevelOptCase::LEVEL_OPT_NOT_SET);
    EXPECT_EQ(ls.statement_duration_opt_case(), tateyama::proto::altimeter::request::LogSettings::StatementDurationOptCase::STATEMENT_DURATION_OPT_NOT_SET);
    EXPECT_EQ(ls.enabled(), false);
}

TEST_F(altimeter_test, disable_audit) {
    {
        tateyama::proto::altimeter::response::Configure rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter disable audit --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter disable audit" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::Configure rq{};
    server_mock_->request_message(rq);

    EXPECT_FALSE(rq.has_event_log());
    EXPECT_TRUE(rq.has_audit_log());

    auto& ls = rq.audit_log();
    EXPECT_EQ(ls.enabled_opt_case(), tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::kEnabled);
    EXPECT_EQ(ls.level_opt_case(), tateyama::proto::altimeter::request::LogSettings::LevelOptCase::LEVEL_OPT_NOT_SET);
    EXPECT_EQ(ls.statement_duration_opt_case(), tateyama::proto::altimeter::request::LogSettings::StatementDurationOptCase::STATEMENT_DURATION_OPT_NOT_SET);
    EXPECT_EQ(ls.enabled(), false);
}



// set level
TEST_F(altimeter_test, level_event) {
    {
        tateyama::proto::altimeter::response::Configure rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter set event_level 12 --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter set event_level" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::Configure rq{};
    server_mock_->request_message(rq);

    EXPECT_TRUE(rq.has_event_log());
    EXPECT_FALSE(rq.has_audit_log());

    auto& ls = rq.event_log();
    EXPECT_EQ(ls.enabled_opt_case(), tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::ENABLED_OPT_NOT_SET);
    EXPECT_EQ(ls.level_opt_case(), tateyama::proto::altimeter::request::LogSettings::LevelOptCase::kLevel);
    EXPECT_EQ(ls.statement_duration_opt_case(), tateyama::proto::altimeter::request::LogSettings::StatementDurationOptCase::STATEMENT_DURATION_OPT_NOT_SET);
    EXPECT_EQ(ls.level(), 12);
}

TEST_F(altimeter_test, level_audit) {
    {
        tateyama::proto::altimeter::response::Configure rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter set audit_level 12 --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter set audit_level" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::Configure rq{};
    server_mock_->request_message(rq);

    EXPECT_FALSE(rq.has_event_log());
    EXPECT_TRUE(rq.has_audit_log());

    auto& ls = rq.audit_log();
    EXPECT_EQ(ls.enabled_opt_case(), tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::ENABLED_OPT_NOT_SET);
    EXPECT_EQ(ls.level_opt_case(), tateyama::proto::altimeter::request::LogSettings::LevelOptCase::kLevel);
    EXPECT_EQ(ls.statement_duration_opt_case(), tateyama::proto::altimeter::request::LogSettings::StatementDurationOptCase::STATEMENT_DURATION_OPT_NOT_SET);
    EXPECT_EQ(ls.level(), 12);
}

// set statement_duration
TEST_F(altimeter_test, statement_duration) {
    {
        tateyama::proto::altimeter::response::Configure rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter set statement_duration 12345 --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter set statement_duration" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::Configure rq{};
    server_mock_->request_message(rq);

    EXPECT_TRUE(rq.has_event_log());
    EXPECT_FALSE(rq.has_audit_log());

    auto& ls = rq.event_log();
    EXPECT_EQ(ls.enabled_opt_case(), tateyama::proto::altimeter::request::LogSettings::EnabledOptCase::ENABLED_OPT_NOT_SET);
    EXPECT_EQ(ls.level_opt_case(), tateyama::proto::altimeter::request::LogSettings::LevelOptCase::LEVEL_OPT_NOT_SET);
    EXPECT_EQ(ls.statement_duration_opt_case(), tateyama::proto::altimeter::request::LogSettings::StatementDurationOptCase::kStatementDuration);
    EXPECT_EQ(ls.statement_duration(), 12345);
}

// rotate
TEST_F(altimeter_test, rotate_event) {
    {
        tateyama::proto::altimeter::response::LogRotate rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter rotate event --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter rotate event" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::LogRotate rq{};
    server_mock_->request_message(rq);

    EXPECT_EQ(rq.category(), tateyama::proto::altimeter::common::LogCategory::EVENT);
}

TEST_F(altimeter_test, rotate_audit) {
    {
        tateyama::proto::altimeter::response::LogRotate rs{};
        (void) rs.mutable_success();
        server_mock_->push_response(rs.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl altimeter rotate audit --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl altimeter rotate audit" << std::endl;
    }
    auto result = read_pipe(fp);
    std::cout << result << std::flush;

    EXPECT_EQ(tateyama::framework::service_id_altimeter, server_mock_->component_id());
    tateyama::proto::altimeter::request::LogRotate rq{};
    server_mock_->request_message(rq);

    EXPECT_EQ(rq.category(), tateyama::proto::altimeter::common::LogCategory::AUDIT);
}

}  // namespace tateyama::altimeter
