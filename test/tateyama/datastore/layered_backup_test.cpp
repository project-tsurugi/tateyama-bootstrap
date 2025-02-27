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
// #include <chrono>
// #include <sstream>
// #include <array>
// #include <sys/types.h>
// #include <unistd.h>

#include <boost/thread/barrier.hpp>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>
#include <tateyama/framework/component_ids.h>
#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/test_utils/server_mock.h"

namespace tateyama::test_utils {

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::BackupBegin>(tateyama::proto::datastore::request::BackupBegin& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kBackupBegin);
    rq = r.backup_begin();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::BackupEnd>(tateyama::proto::datastore::request::BackupEnd& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kBackupEnd);
    rq = r.backup_end();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::BackupEstimate>(tateyama::proto::datastore::request::BackupEstimate& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kBackupEstimate);
    rq = r.backup_estimate();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::BackupDetailBegin>(tateyama::proto::datastore::request::BackupDetailBegin& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kBackupDetailBegin);
    rq = r.backup_detail_begin();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::TagList>(tateyama::proto::datastore::request::TagList& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kTagList);
    rq = r.tag_list();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::TagAdd>(tateyama::proto::datastore::request::TagAdd& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kTagAdd);
    rq = r.tag_add();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::TagGet>(tateyama::proto::datastore::request::TagGet& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kTagGet);
    rq = r.tag_get();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::TagRemove>(tateyama::proto::datastore::request::TagRemove& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kTagRemove);
    rq = r.tag_remove();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::RestoreBegin>(tateyama::proto::datastore::request::RestoreBegin& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kRestoreBegin);
    rq = r.restore_begin();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::RestoreStatus>(tateyama::proto::datastore::request::RestoreStatus& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kRestoreStatus);
    rq = r.restore_status();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::RestoreCancel>(tateyama::proto::datastore::request::RestoreCancel& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kRestoreCancel);
    rq = r.restore_cancel();
}

template<>
inline void server_mock::request_message<tateyama::proto::datastore::request::RestoreDispose>(tateyama::proto::datastore::request::RestoreDispose& rq) {
    tateyama::proto::datastore::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::datastore::request::Request::CommandCase::kRestoreDispose);
    rq = r.restore_dispose();
}

}  // tateyama::test_utils


namespace tateyama::datastore {

class layered_backup_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("layered_backup_test", 20501);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("layered_backup_test", bst_conf.digest(), sync_);
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

TEST_F(layered_backup_test, backup) {
    std::string command;
    FILE *fp;

    {
        tateyama::proto::datastore::response::BackupBegin response{};
        auto* success = response.mutable_success();
        auto* sources = success->mutable_simple_source();

        server_mock_->push_response(response.SerializeAsString());
    }
    {
        tateyama::proto::datastore::response::BackupEnd response{};
        auto* success = response.mutable_success();

        server_mock_->push_response(response.SerializeAsString());
    }

    command = "tgctl  backup create ";
    command += " --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl backup create" << std::endl;
    }
    auto result = read_pipe(fp);
}

}  // namespace tateyama::datastore
