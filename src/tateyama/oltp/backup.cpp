/*
 * Copyright 2022-2022 tsurugi project.
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
#include <exception>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/framework/component_ids.h>
#include <tateyama/logging.h>

#include "oltp.h"
#include "configuration.h"
#include "transport.h"

DEFINE_bool(overwrite, false, "backup files will be over written");  // NOLINT
DEFINE_bool(keep_backup, false, "backup files will be kept");  // NOLINT
DECLARE_string(conf);  // NOLINT

namespace tateyama::bootstrap::backup {

using namespace tateyama::bootstrap::utils;

static std::string name() {
    if (auto conf = bootstrap_configuration(FLAGS_conf).create_configuration(); conf != nullptr) {
        auto endpoint_config = conf->get_section("ipc_endpoint");
        if (endpoint_config == nullptr) {
            LOG(ERROR) << "cannot find ipc_endpoint section in the configuration";
            exit(2);
        }
        auto database_name_opt = endpoint_config->get<std::string>("database_name");
        if (!database_name_opt) {
            LOG(ERROR) << "cannot find database_name at the section in the configuration";
            exit(2);
        }
        return database_name_opt.value();
    } else {
        LOG(ERROR) << "error in create_configuration";
        exit(2);
    }
}

int oltp_backup_create([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(name(), tateyama::framework::service_id_datastore);
    ::tateyama::proto::datastore::request::Request requestBegin{};
    requestBegin.mutable_backup_begin();
    auto responseBegin = transport->send<::tateyama::proto::datastore::response::BackupBegin>(requestBegin);
    requestBegin.clear_backup_begin();

    if (!responseBegin) {
        LOG(ERROR) << "BackupBegin response error: ";
        return 1;
    }

    auto rb = responseBegin.value();
    switch(rb.result_case()) {
    case ::tateyama::proto::datastore::response::BackupBegin::ResultCase::kSuccess:
        break;
    case ::tateyama::proto::datastore::response::BackupBegin::ResultCase::kUnknownError:
        LOG(ERROR) << "BackupBegin error: " << rb.unknown_error().message();
        return 2;
    default:
        LOG(ERROR) << "BackupBegin result_case() error: ";
        return 3;
    }

    std::int64_t backup_id = rb.success().id();
    // FIXME do copy when actual operation begin.
    std::cout << "do backup create, overwrite = " << FLAGS_overwrite << " , files are " << std::endl;
    for (auto&& e : rb.success().files()) {
        std::cout << e << std::endl;
    }

    ::tateyama::proto::datastore::request::Request requestEnd{};
    auto backup_end = requestEnd.mutable_backup_end();
    backup_end->set_id(backup_id);
    auto responseEnd = transport->send<::tateyama::proto::datastore::response::BackupEnd>(requestEnd);
    requestEnd.clear_backup_end();

    if (!responseEnd) {
        LOG(ERROR) << "BackupEnd response error: ";
        return 1;
    }
    auto re = responseEnd.value();
    switch(re.result_case()) {
    case ::tateyama::proto::datastore::response::BackupEnd::ResultCase::kSuccess:
        break;
    case ::tateyama::proto::datastore::response::BackupEnd::ResultCase::kUnknownError:
        LOG(ERROR) << "BackupEnd error: " << re.unknown_error().message();
        return 2;
    default:
        LOG(ERROR) << "BackupEnd result_case() error: ";
        return 3;
    }
    return 0;
}

int oltp_backup_estimate([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(name(), tateyama::framework::service_id_datastore);
        ::tateyama::proto::datastore::request::Request request{};
        request.mutable_backup_estimate();
        auto response = transport->send<::tateyama::proto::datastore::response::BackupEstimate>(request);
        request.clear_backup_estimate();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::BackupEstimate::kSuccess: {
                auto success = response.value().success();
                std::cout << "number_of_files = " << success.number_of_files()
                          << ", number_of_bytes = " << success.number_of_bytes() << std::endl;
                return 0;
            }
            case ::tateyama::proto::datastore::response::BackupEstimate::kUnknownError:
            case ::tateyama::proto::datastore::response::BackupEstimate::RESULT_NOT_SET:
                LOG(ERROR) << __func__ << " ends up with " << response.value().result_case();
                return 1;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << name();
    }
    return -1;
}

int oltp_restore_backup([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    const char* path_to_backup = *(argv + 1);
    argc--;
    argv++;

    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(name(), tateyama::framework::service_id_datastore);

        ::tateyama::proto::datastore::request::Request request{};
        auto restore_backup = request.mutable_restore_backup();
        restore_backup->set_path(path_to_backup);
        restore_backup->set_keep_backup(FLAGS_keep_backup);
        auto response = transport->send<::tateyama::proto::datastore::response::RestoreBackup>(request);
        request.clear_restore_backup();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::RestoreBackup::kSuccess:
                return 0;
            case ::tateyama::proto::datastore::response::RestoreBackup::kNotFound:
            case ::tateyama::proto::datastore::response::RestoreBackup::kPermissionError:
            case ::tateyama::proto::datastore::response::RestoreBackup::kBrokenData:
            case ::tateyama::proto::datastore::response::RestoreBackup::kUnknownError:
            case ::tateyama::proto::datastore::response::RestoreBackup::RESULT_NOT_SET:
                LOG(ERROR) << __func__ << " ends up with " << response.value().result_case();
                return 1;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << name();
    }
    return -1;
}

int oltp_restore_tag([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    const char* tag_name = *(argv + 1);
    argc--;
    argv++;

    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(name(), tateyama::framework::service_id_datastore);

        ::tateyama::proto::datastore::request::Request request{};
        auto restore_tag = request.mutable_restore_tag();
        restore_tag->set_name(tag_name);
        auto response = transport->send<::tateyama::proto::datastore::response::RestoreTag>(request);
        request.clear_restore_tag();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::RestoreTag::kSuccess:
                return 0;
            case ::tateyama::proto::datastore::response::RestoreTag::kNotFound:
            case ::tateyama::proto::datastore::response::RestoreTag::kUnknownError:
            case ::tateyama::proto::datastore::response::RestoreTag::RESULT_NOT_SET:
                LOG(ERROR) << __func__ << " ends up with " << response.value().result_case();
                return 1;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << name();
    }
    return -1;
}

} //  tateyama::bootstrap::backup
