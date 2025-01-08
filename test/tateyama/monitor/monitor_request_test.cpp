/*
 * Copyright 2022-2022 Project Tsurugi.
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

#include "tateyama/monitor/monitor.h"

namespace tateyama::testing {

class monitor_request_test : public ::testing::Test {
public:
    virtual void SetUp() {
        monitor_ = std::make_unique<tateyama::monitor::monitor>(ss_);
    }

    virtual void TearDown() {
    }

protected:
    std::stringstream ss_{};
    std::unique_ptr<tateyama::monitor::monitor> monitor_{};
};

TEST_F(monitor_request_test, list) {
    monitor_->request_list(123, 456, 789, 135, 246);

    std::string result = ss_.str();
    std::cout << result << std::endl;
    EXPECT_TRUE(validate_json(result));
    EXPECT_NE(std::string::npos, result.find(": 123,"));
    EXPECT_NE(std::string::npos, result.find(": 456,"));
    EXPECT_NE(std::string::npos, result.find(": 789,"));
    EXPECT_NE(std::string::npos, result.find(": 135,"));
    EXPECT_NE(std::string::npos, result.find(": 246 "));
}

TEST_F(monitor_request_test, payload) {
    monitor_->request_payload("abcdef");

    std::string result = ss_.str();
    std::cout << result << std::endl;
    EXPECT_TRUE(validate_json(result));
    EXPECT_NE(std::string::npos, result.find(R"(: "abcdef" )"));
}

TEST_F(monitor_request_test, sql_n_n) {
    std::optional<std::string> transaction_id{};
    std::optional<std::string> sql{};

    monitor_->request_extract_sql(transaction_id, sql);
    std::string result = ss_.str();
    std::cout << result << std::endl;
    EXPECT_TRUE(validate_json(result));
    EXPECT_EQ(std::string::npos, result.find(tateyama::monitor::TRANSACTION_ID));
    EXPECT_EQ(std::string::npos, result.find(tateyama::monitor::SQL));
}

TEST_F(monitor_request_test, sql_t_n) {
    std::optional<std::string> transaction_id{};
    std::optional<std::string> sql{};

    transaction_id = std::string("TID-xxxx");
    monitor_->request_extract_sql(transaction_id, sql);
    std::string result = ss_.str();
    std::cout << result << std::endl;
    EXPECT_TRUE(validate_json(result));
    EXPECT_NE(std::string::npos, result.find(tateyama::monitor::TRANSACTION_ID));
    EXPECT_NE(std::string::npos, result.find(R"( "TID-xxxx")"));
    EXPECT_EQ(std::string::npos, result.find(tateyama::monitor::SQL));
}

TEST_F(monitor_request_test, sql_n_s) {
    std::optional<std::string> transaction_id{};
    std::optional<std::string> sql{};

    sql = std::string("select 1");
    monitor_->request_extract_sql(transaction_id, sql);
    std::string result = ss_.str();
    std::cout << result << std::endl;
    EXPECT_TRUE(validate_json(result));
    EXPECT_EQ(std::string::npos, result.find(tateyama::monitor::TRANSACTION_ID));
    EXPECT_NE(std::string::npos, result.find(tateyama::monitor::SQL));
    EXPECT_NE(std::string::npos, result.find(R"( "select 1")"));
    EXPECT_NE(std::string::npos, result.find(R"( "query")"));
}

TEST_F(monitor_request_test, sql_t_s) {
    std::optional<std::string> transaction_id{};
    std::optional<std::string> sql{};

    transaction_id = std::string("TID-xxxx");
    sql = std::string("select 1");
    monitor_->request_extract_sql(transaction_id, sql);
    std::string result = ss_.str();
    std::cout << result << std::endl;
    EXPECT_TRUE(validate_json(result));
    EXPECT_NE(std::string::npos, result.find(tateyama::monitor::TRANSACTION_ID));
    EXPECT_NE(std::string::npos, result.find(R"( "TID-xxxx")"));
    EXPECT_NE(std::string::npos, result.find(tateyama::monitor::SQL));
    EXPECT_NE(std::string::npos, result.find(R"( "select 1")"));
    EXPECT_NE(std::string::npos, result.find(R"( "query")"));
}

}  // namespace tateyama::testing
