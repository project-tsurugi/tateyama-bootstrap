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
#include <array>
#include <sys/types.h>
#include <unistd.h>

#include <boost/thread/barrier.hpp>

#include <tateyama/proto/request/request.pb.h>
#include <tateyama/proto/request/response.pb.h>
#include <jogasaki/proto/sql/request.pb.h>
#include <jogasaki/proto/sql/response.pb.h>
#include <tateyama/framework/component_ids.h>
#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/test_utils/server_mock.h"

namespace tateyama::test_utils {

template<>
inline void server_mock::request_message<tateyama::proto::request::request::ListRequest>(tateyama::proto::request::request::ListRequest& rq) {
    tateyama::proto::request::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::request::request::Request::CommandCase::kListRequest);
    rq = r.list_request();
}

template<>
inline void server_mock::request_message<tateyama::proto::request::request::GetPayload>(tateyama::proto::request::request::GetPayload& rq) {
    tateyama::proto::request::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::request::request::Request::CommandCase::kGetPayload);
    rq = r.get_payload();
}

template<>
inline void server_mock::request_message<jogasaki::proto::sql::request::ExtractStatementInfo>(jogasaki::proto::sql::request::ExtractStatementInfo& rq) {
    jogasaki::proto::sql::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.request_case(), jogasaki::proto::sql::request::Request::RequestCase::kExtractStatementInfo);
    rq = r.extract_statement_info();
}

}  // tateyama::test_utils


namespace tateyama::request {
using namespace std::literals::string_literals;

class request_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("request_test", 20401);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("request_test", bst_conf.digest(), sync_);
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

TEST_F(request_test, request_list) {
    std::array<std::array<std::size_t, 4>, 3> expected = {
        {
            {123, 1, 4, 13},
            {234, 2, 5, 24},
            {345, 3, 6, 36},
        },
    };

    {
        tateyama::proto::request::response::ListRequest response{};
        auto* success = response.mutable_success();

        auto tp = std::chrono::system_clock::now();
        auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
        auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) - std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);

        for (std::size_t i = 0; i < 3; i++) {
            auto&& e = expected.at(i);

            tateyama::proto::request::response::RequestInfo info{};
            info.set_session_id(e.at(0));
            info.set_request_id(e.at(1));
            info.set_service_id(e.at(2));
            info.set_payload_size(e.at(3));
            info.set_started_time(secs.time_since_epoch().count() * 1000 + (ns.count() + 500000) / 1000000);

            *(success->add_requests()) = info;
        }
        server_mock_->push_response(response.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl request list --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl request list" << std::endl;
    }
    auto result = read_pipe(fp);
//    std::cout << result << std::flush;
    EXPECT_NE(std::string::npos, result.find("       123"));
    EXPECT_NE(std::string::npos, result.find("           1"));
    EXPECT_NE(std::string::npos, result.find("          4"));
    EXPECT_NE(std::string::npos, result.find("            13"));

    EXPECT_NE(std::string::npos, result.find("       234"));
    EXPECT_NE(std::string::npos, result.find("           2"));
    EXPECT_NE(std::string::npos, result.find("          5"));
    EXPECT_NE(std::string::npos, result.find("            24"));

    EXPECT_NE(std::string::npos, result.find("       345"));
    EXPECT_NE(std::string::npos, result.find("           3"));
    EXPECT_NE(std::string::npos, result.find("          6"));
    EXPECT_NE(std::string::npos, result.find("            36"));

    EXPECT_EQ(tateyama::framework::service_id_request, server_mock_->component_id());
    tateyama::proto::request::request::ListRequest rq{};
    server_mock_->request_message(rq);
}

TEST_F(request_test, request_payload) {
    {
        tateyama::proto::request::response::GetPayload response{};
        auto* success = response.mutable_success();
        success->set_data("abcdefg");

        server_mock_->push_response(response.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl request payload 123456 987654 --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl request payload" << std::endl;
    }
    auto result = read_pipe(fp);
//    std::cout << result << std::flush;
    EXPECT_NE(std::string::npos, result.find("YWJjZGVmZw"));

    EXPECT_EQ(tateyama::framework::service_id_request, server_mock_->component_id());
    tateyama::proto::request::request::GetPayload rq{};
    server_mock_->request_message(rq);
    EXPECT_EQ(rq.session_id(), 123456);
    EXPECT_EQ(rq.request_id(), 987654);
}

TEST_F(request_test, request_extract_sql) {
    {
        jogasaki::proto::sql::response::Response response{};
        auto* extract_statement_info = response.mutable_extract_statement_info();
        auto* success = extract_statement_info->mutable_success();
        success->mutable_transaction_id()->set_id("transaction_id_for_test");
        success->set_sql("select 1");

        server_mock_->push_response(response.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl request extract-sql 123456 YWJjZGVmZw --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl request extract-sql" << std::endl;
    }
    auto result = read_pipe(fp);
//    std::cout << result << std::flush;
    EXPECT_NE(std::string::npos, result.find("select 1"));

    EXPECT_EQ(tateyama::framework::service_id_sql, server_mock_->component_id());
    jogasaki::proto::sql::request::ExtractStatementInfo rq{};
    server_mock_->request_message(rq);
    EXPECT_EQ(rq.session_id(), 123456);
    EXPECT_EQ(rq.payload(), "abcdefg");
}

}  // namespace tateyama::request
