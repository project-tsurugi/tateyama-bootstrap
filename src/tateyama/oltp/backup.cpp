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
#include <string_view>
#include <cstring>
#include <sys/ioctl.h>
#include <termios.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <tateyama/framework/component_ids.h>
#include <tateyama/logging.h>

#include "oltp.h"
#include "configuration.h"
#include "authentication.h"
#include "transport.h"
#include "monitor.h"

DECLARE_string(conf);  // NOLINT
DECLARE_bool(force);  // NOLINT
DECLARE_bool(keep_backup);  // NOLINT
DECLARE_string(label);  // NOLINT
DECLARE_string(monitor);  // NOLINT

namespace tateyama::bootstrap::backup {

using namespace tateyama::bootstrap::utils;

static bool prompt(std::string_view msg)
{
    struct termios io_conf{};

    memset(&io_conf, 0x00, sizeof(struct termios));
    tcgetattr(0, &io_conf);
    io_conf.c_lflag &= ~(ECHO);  // disabled echo  // NOLINT
    io_conf.c_lflag &= ~(ICANON);  // disabled canonical  // NOLINT
    io_conf.c_cc[VMIN]  = 1;
    io_conf.c_cc[VTIME] = 1;
    tcsetattr( 0 , TCSAFLUSH , &io_conf );

    std::cout << msg;
    bool rv{};
    while(true) {
        int c = getc(stdin);
        if ((c == 'y' ) || (c == 'Y' )) {
            std::cout << "yes" << std::endl;
            rv = true;
            break;
        }
        if ((c == 'n' ) || (c == 'N' )) {
            std::cout << "no" << std::endl;
            rv = false;
            break;
        }
    }

    memset(&io_conf, 0x00, sizeof(struct termios));
    tcgetattr(0, &io_conf);
    io_conf.c_lflag |= ECHO;  // enable echo  // NOLINT
    io_conf.c_lflag |= ICANON;  // enable canonical  // NOLINT
    io_conf.c_cc[VMIN]  = 1;
    io_conf.c_cc[VTIME] = 1;
    tcsetattr( 0 , TCSAFLUSH , &io_conf );

    return rv;
}

static std::string database_name() {
    if (auto conf = bootstrap_configuration::create_configuration(FLAGS_conf); conf != nullptr) {
        auto endpoint_config = conf->get_section("ipc_endpoint");
        if (endpoint_config == nullptr) {
            LOG(ERROR) << "cannot find ipc_endpoint section in the configuration";
            exit(tateyama::bootstrap::return_code::err);
        }
        auto database_name_opt = endpoint_config->get<std::string>("database_name");
        if (!database_name_opt) {
            LOG(ERROR) << "cannot find database_name at the section in the configuration";
            exit(tateyama::bootstrap::return_code::err);
        }
        return database_name_opt.value();
    }
    LOG(ERROR) << "error in create_configuration";
    exit(2);
}

static std::string digest() {
    auto bst_conf = utils::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (bst_conf.valid()) {
        return bst_conf.digest();
    }
    return std::string();
}
    
return_code oltp_backup_create(const std::string& path_to_backup) {
    std::unique_ptr<utils::monitor> monitor_output{};

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = tateyama::bootstrap::return_code::ok;
    auth_options();
    auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(database_name(), digest(), tateyama::framework::service_id_datastore);
    ::tateyama::proto::datastore::request::Request requestBegin{};
    auto backup_begin = requestBegin.mutable_backup_begin();
    if (!FLAGS_label.empty()) {
        backup_begin->set_label(FLAGS_label);
    }
    auto responseBegin = transport->send<::tateyama::proto::datastore::response::BackupBegin>(requestBegin);
    requestBegin.clear_backup_begin();

    if (responseBegin) {
        auto rb = responseBegin.value();
        switch(rb.result_case()) {
        case ::tateyama::proto::datastore::response::BackupBegin::ResultCase::kSuccess:
            break;
        case ::tateyama::proto::datastore::response::BackupBegin::ResultCase::kUnknownError:
            LOG(ERROR) << "BackupBegin error: " << rb.unknown_error().message();
            rc = tateyama::bootstrap::return_code::err;
            break;
        default:
            LOG(ERROR) << "BackupBegin result_case() error: ";
            rc = tateyama::bootstrap::return_code::err;
        }

        if (rc == tateyama::bootstrap::return_code::ok) {
            std::int64_t backup_id = rb.success().id();

            auto location = boost::filesystem::path(path_to_backup);

            std::size_t total_bytes = 0;
            if(!FLAGS_monitor.empty()) {
                for (auto&& e : rb.success().files()) {
                    auto src = boost::filesystem::path(e);
                    total_bytes += boost::filesystem::file_size(src);
                }
            }

            std::size_t completed_bytes = 0;
            for (auto&& e : rb.success().files()) {
                auto src = boost::filesystem::path(e);
                boost::filesystem::copy_file(src, location / src.filename());
                if(!FLAGS_monitor.empty()) {
                    completed_bytes += boost::filesystem::file_size(src);
                    monitor_output->progress(static_cast<float>(completed_bytes) / static_cast<float>(total_bytes));
                }
            }

            ::tateyama::proto::datastore::request::Request requestEnd{};
            auto backup_end = requestEnd.mutable_backup_end();
            backup_end->set_id(backup_id);
            auto responseEnd = transport->send<::tateyama::proto::datastore::response::BackupEnd>(requestEnd);
            requestEnd.clear_backup_end();
            transport->close();

            if (responseEnd) {
                auto re = responseEnd.value();
                switch(re.result_case()) {
                case ::tateyama::proto::datastore::response::BackupEnd::ResultCase::kSuccess:
                    break;
                case ::tateyama::proto::datastore::response::BackupEnd::ResultCase::kUnknownError:
                    LOG(ERROR) << "BackupEnd error: " << re.unknown_error().message();
                    rc = tateyama::bootstrap::return_code::err;
                    break;
                default:
                    LOG(ERROR) << "BackupEnd result_case() error: ";
                    rc = tateyama::bootstrap::return_code::err;
                }
                if (rc == tateyama::bootstrap::return_code::ok) {
                    if (monitor_output) {
                        monitor_output->finish(true);
                    }
                    return rc;
                }
            } else {
                LOG(ERROR) << "BackupEnd response error: ";
                rc = tateyama::bootstrap::return_code::err;
            }
        }
    } else {
        LOG(ERROR) << "BackupBegin response error: ";
        rc = tateyama::bootstrap::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

return_code oltp_backup_estimate() {
    std::unique_ptr<utils::monitor> monitor_output{};

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = tateyama::bootstrap::return_code::ok;
    auth_options();

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(database_name(), digest(), tateyama::framework::service_id_datastore);
        ::tateyama::proto::datastore::request::Request request{};
        request.mutable_backup_estimate();
        auto response = transport->send<::tateyama::proto::datastore::response::BackupEstimate>(request);
        request.clear_backup_estimate();
        transport->close();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::BackupEstimate::kSuccess: {
                auto success = response.value().success();
                std::cout << "number_of_files = " << success.number_of_files()
                          << ", number_of_bytes = " << success.number_of_bytes() << std::endl;
                break;
            }
            case ::tateyama::proto::datastore::response::BackupEstimate::kUnknownError:
            case ::tateyama::proto::datastore::response::BackupEstimate::RESULT_NOT_SET:
                LOG(ERROR) << " ends up with " << response.value().result_case();
                rc = tateyama::bootstrap::return_code::err;
            }
            if (rc == tateyama::bootstrap::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rc;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << database_name();
    }
    rc = tateyama::bootstrap::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

return_code oltp_restore_backup(const std::string& path_to_backup) {
    std::unique_ptr<utils::monitor> monitor_output{};

    if(!FLAGS_force) {
        if (!prompt("continue? (press y or n) : ")) {
            std::cout << "restore backup has been canceled." << std::endl;
            LOG(INFO) << "restore backup has been canceled.";
            return tateyama::bootstrap::return_code::err;
        }
    }

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = tateyama::bootstrap::return_code::ok;
    auth_options();

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(database_name(), digest(), tateyama::framework::service_id_datastore);
        ::tateyama::proto::datastore::request::Request request{};
        auto restore_backup = request.mutable_restore_backup();
        restore_backup->set_path(path_to_backup);
        restore_backup->set_keep_backup(FLAGS_keep_backup);
        if (!FLAGS_label.empty()) {
            restore_backup->set_label(FLAGS_label);
        }
        auto response = transport->send<::tateyama::proto::datastore::response::RestoreBackup>(request);
        request.clear_restore_backup();
        transport->close();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::RestoreBackup::kSuccess:
                break;
            case ::tateyama::proto::datastore::response::RestoreBackup::kNotFound:
            case ::tateyama::proto::datastore::response::RestoreBackup::kPermissionError:
            case ::tateyama::proto::datastore::response::RestoreBackup::kBrokenData:
            case ::tateyama::proto::datastore::response::RestoreBackup::kUnknownError:
            case ::tateyama::proto::datastore::response::RestoreBackup::RESULT_NOT_SET:
                LOG(ERROR) << " ends up with " << response.value().result_case();
                rc = tateyama::bootstrap::return_code::err;
            }
            if (rc == tateyama::bootstrap::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rc;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << database_name();
    }
    rc = tateyama::bootstrap::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

return_code oltp_restore_tag(const std::string& tag_name) {
    std::unique_ptr<utils::monitor> monitor_output{};

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = tateyama::bootstrap::return_code::ok;
    auth_options();

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(database_name(), digest(), tateyama::framework::service_id_datastore);

        ::tateyama::proto::datastore::request::Request request{};
        auto restore_tag = request.mutable_restore_tag();
        restore_tag->set_name(tag_name);
        auto response = transport->send<::tateyama::proto::datastore::response::RestoreTag>(request);
        request.clear_restore_tag();
        transport->close();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::RestoreTag::kSuccess:
                break;
            case ::tateyama::proto::datastore::response::RestoreTag::kNotFound:
            case ::tateyama::proto::datastore::response::RestoreTag::kUnknownError:
            case ::tateyama::proto::datastore::response::RestoreTag::RESULT_NOT_SET:
                LOG(ERROR) << " ends up with " << response.value().result_case();
                rc = tateyama::bootstrap::return_code::err;
            }
            if (rc == tateyama::bootstrap::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rc;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << database_name();
    }
    rc = tateyama::bootstrap::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

} //  tateyama::bootstrap::backup
