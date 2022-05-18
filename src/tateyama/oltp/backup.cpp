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
#include <exception>

#include <gflags/gflags.h>
#include <glog/logging.h>

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
    const char* path_to_backup = *(argv + 1);
    argc--;
    argv++;

    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // do backup create, /path/to/backup is argv[1]
    std::cout << path_to_backup << ":" << FLAGS_overwrite << std::endl;
    return 0;
}

int oltp_backup_estimate([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(name());
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
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(name());

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
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(name());

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
