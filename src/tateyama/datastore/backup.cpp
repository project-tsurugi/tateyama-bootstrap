/*
 * Copyright 2022-2023 tsurugi project.
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

#include "configuration/configuration.h"
#include "authentication/authentication.h"
#include "transport/transport.h"
#include "monitor/monitor.h"
#include "backup.h"
#include "file_list.h"

DECLARE_string(conf);  // NOLINT
DECLARE_bool(force);  // NOLINT
DECLARE_bool(keep_backup);  // NOLINT
DECLARE_string(label);  // NOLINT
DECLARE_string(monitor);  // NOLINT
DECLARE_string(use_file_list);  // NOLINT

namespace tateyama::datastore {

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
    if (auto conf = configuration::bootstrap_configuration::create_configuration(FLAGS_conf); conf != nullptr) {
        auto endpoint_config = conf->get_section("ipc_endpoint");
        if (endpoint_config == nullptr) {
            LOG(ERROR) << "cannot find ipc_endpoint section in the configuration";
            exit(oltp::return_code::err);
        }
        auto database_name_opt = endpoint_config->get<std::string>("database_name");
        if (!database_name_opt) {
            LOG(ERROR) << "cannot find database_name at the section in the configuration";
            exit(oltp::return_code::err);
        }
        return database_name_opt.value();
    }
    LOG(ERROR) << "error in create_configuration";
    exit(2);
}

static std::string digest() {
    auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (bst_conf.valid()) {
        return bst_conf.digest();
    }
    return std::string();
}
    
oltp::return_code oltp_backup_create(const std::string& path_to_backup) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = oltp::return_code::ok;
    authentication::auth_options();
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
            rc = oltp::return_code::err;
            break;
        default:
            LOG(ERROR) << "BackupBegin result_case() error: ";
            rc = oltp::return_code::err;
        }

        if (rc == oltp::return_code::ok) {
            std::int64_t backup_id = rb.success().id();

            auto location = boost::filesystem::path(path_to_backup);

            std::size_t total_bytes = 0;
            if(!FLAGS_monitor.empty()) {
                for (auto&& e : rb.success().simple_source().files()) {
                    auto src = boost::filesystem::path(e);
                    total_bytes += boost::filesystem::file_size(src);
                }
            }

            std::size_t completed_bytes = 0;
            for (auto&& e : rb.success().simple_source().files()) {
                auto src = boost::filesystem::path(e);
                boost::filesystem::copy_file(src, location / src.filename());
                if(!FLAGS_monitor.empty()) {
                    completed_bytes += boost::filesystem::file_size(src);
                    if (total_bytes > 0) {
                        monitor_output->progress(static_cast<float>(completed_bytes) / static_cast<float>(total_bytes));
                    } else {
                        monitor_output->progress(1.0);
                    }
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
                    rc = oltp::return_code::err;
                    break;
                default:
                    LOG(ERROR) << "BackupEnd result_case() error: ";
                    rc = oltp::return_code::err;
                }
                if (rc == oltp::return_code::ok) {
                    if (monitor_output) {
                        monitor_output->finish(true);
                    }
                    return rc;
                }
            } else {
                LOG(ERROR) << "BackupEnd response error: ";
                rc = oltp::return_code::err;
            }
        }
    } else {
        LOG(ERROR) << "BackupBegin response error: ";
        rc = oltp::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

oltp::return_code oltp_backup_estimate() {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = oltp::return_code::ok;
    authentication::auth_options();

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
                rc = oltp::return_code::err;
            }
            if (rc == oltp::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rc;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << database_name();
    }
    rc = oltp::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

oltp::return_code oltp_restore_backup(const std::string& path_to_backup) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if(!FLAGS_force) {
        if (!prompt("continue? (press y or n) : ")) {
            std::cout << "restore backup has been canceled." << std::endl;
            LOG(INFO) << "restore backup has been canceled.";
            return oltp::return_code::err;
        }
    }

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = oltp::return_code::ok;
    authentication::auth_options();

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(database_name(), digest(), tateyama::framework::service_id_datastore);
        ::tateyama::proto::datastore::request::Request request{};
        auto restore_begin = request.mutable_restore_begin();
        restore_begin->set_backup_directory(path_to_backup);
        restore_begin->set_keep_backup(FLAGS_keep_backup);
        if (!FLAGS_label.empty()) {
            restore_begin->set_label(FLAGS_label);
        }
        auto response = transport->send<::tateyama::proto::datastore::response::RestoreBegin>(request);
        request.clear_restore_begin();
        transport->close();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::RestoreBegin::kSuccess:
                break;
            case ::tateyama::proto::datastore::response::RestoreBegin::kNotFound:
            case ::tateyama::proto::datastore::response::RestoreBegin::kPermissionError:
            case ::tateyama::proto::datastore::response::RestoreBegin::kBrokenData:
            case ::tateyama::proto::datastore::response::RestoreBegin::kUnknownError:
            case ::tateyama::proto::datastore::response::RestoreBegin::RESULT_NOT_SET:
                LOG(ERROR) << " ends up with " << response.value().result_case();
                rc = oltp::return_code::err;
            }
            if (rc == oltp::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rc;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << database_name();
    }
    rc = oltp::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

oltp::return_code oltp_restore_backup_use_file_list(const std::string& path_to_backup) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if(!FLAGS_force) {
        if (!prompt("continue? (press y or n) : ")) {
            std::cout << "restore backup has been canceled." << std::endl;
            LOG(INFO) << "restore backup has been canceled.";
            return oltp::return_code::err;
        }
    }

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = oltp::return_code::ok;
    authentication::auth_options();

    try {
        auto parser = std::make_unique<file_list>();
        if (!parser->read_json(FLAGS_use_file_list)) {
            LOG(ERROR) << "error occurred in using the file_list (" << FLAGS_use_file_list << ")";
            if (monitor_output) {
                monitor_output->finish(false);
            }
            return oltp::return_code::err;
        }
        if (!FLAGS_keep_backup) {
            LOG(WARNING) << "option --nokeep_backup is ignored when --use-file-list is specified";
        }

        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(database_name(), digest(), tateyama::framework::service_id_datastore);
        ::tateyama::proto::datastore::request::Request request{};
        auto restore_begin = request.mutable_restore_begin();
        auto entries = restore_begin->mutable_entries();
        if (!path_to_backup.empty()) {
            entries->set_directory(path_to_backup);
        }
        parser->for_each([entries](const std::string& source, const std::string& destination, bool detached) {
                             auto file_set_entry = entries->add_file_set_entry();
                             file_set_entry->set_source_path(source);
                             file_set_entry->set_destination_path(destination);
                             file_set_entry->set_detached(detached);
                         });
        if (!FLAGS_label.empty()) {
            restore_begin->set_label(FLAGS_label);
        }
        auto response = transport->send<::tateyama::proto::datastore::response::RestoreBegin>(request);
        restore_begin->clear_entries();
        request.clear_restore_begin();
        transport->close();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::RestoreBegin::kSuccess:
                break;
            case ::tateyama::proto::datastore::response::RestoreBegin::kNotFound:
            case ::tateyama::proto::datastore::response::RestoreBegin::kPermissionError:
            case ::tateyama::proto::datastore::response::RestoreBegin::kBrokenData:
            case ::tateyama::proto::datastore::response::RestoreBegin::kUnknownError:
            case ::tateyama::proto::datastore::response::RestoreBegin::RESULT_NOT_SET:
                LOG(ERROR) << " ends up with " << response.value().result_case();
                rc = oltp::return_code::err;
            }
            if (rc == oltp::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rc;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << database_name();
    }
    rc = oltp::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

oltp::return_code oltp_restore_tag(const std::string& tag_name) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = oltp::return_code::ok;
    authentication::auth_options();

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(database_name(), digest(), tateyama::framework::service_id_datastore);

        ::tateyama::proto::datastore::request::Request request{};
        auto restore_begin = request.mutable_restore_begin();
        restore_begin->set_tag_name(tag_name);
        auto response = transport->send<::tateyama::proto::datastore::response::RestoreBegin>(request);
        request.clear_restore_begin();
        transport->close();

        if (response) {
            switch(response.value().result_case()) {
            case ::tateyama::proto::datastore::response::RestoreBegin::kSuccess:
                break;
            case ::tateyama::proto::datastore::response::RestoreBegin::kNotFound:
            case ::tateyama::proto::datastore::response::RestoreBegin::kPermissionError:
            case ::tateyama::proto::datastore::response::RestoreBegin::kBrokenData:
            case ::tateyama::proto::datastore::response::RestoreBegin::kUnknownError:
            case ::tateyama::proto::datastore::response::RestoreBegin::RESULT_NOT_SET:
                LOG(ERROR) << " ends up with " << response.value().result_case();
                rc = oltp::return_code::err;
            }
            if (rc == oltp::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rc;
            }
        }
    } catch (std::runtime_error &e) {
        LOG(ERROR) << "could not connect to database with name " << database_name();
    }
    rc = oltp::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

} //  tateyama::bootstrap::backup