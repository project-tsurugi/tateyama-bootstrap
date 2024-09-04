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
#include <iostream>
#include <exception>
#include <string_view>
#include <cstring>
#include <sys/ioctl.h>
#include <termios.h>
#include <filesystem>
#include <memory>

#include <gflags/gflags.h>

#include <tateyama/logging.h>

#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/authentication/authentication.h"
#define BOOST_BIND_GLOBAL_PLACEHOLDERS  // FIXME (to retain the current behavior)
#include "tateyama/transport/transport.h"
#include "tateyama/monitor/monitor.h"
#include "backup.h"
#include "file_list.h"

// common
DECLARE_string(conf);  // NOLINT
DECLARE_string(label);  // NOLINT
DECLARE_string(monitor);  // NOLINT

// backup
DEFINE_bool(force, false, "no confirmation step");  // NOLINT
DEFINE_bool(keep_backup, true, "backup files will be kept");  // NOLINT
DEFINE_string(use_file_list, "", "json file describing the individual files to be specified for restore");  // NOLINT

namespace tateyama::datastore {

static bool prompt(std::string_view msg)
{
    struct termios oldt{};

    memset(&oldt, 0x00, sizeof(struct termios));
    if (tcgetattr(STDIN_FILENO, &oldt) == -1) {
        throw std::runtime_error("error tcgetattr");
    }

    auto newt = oldt;
    newt.c_iflag = ~( BRKINT | ISTRIP | IXON  );                          // NOLINT
    newt.c_lflag = ~( ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHONL );  // NOLINT
    newt.c_cc[VTIME] = 0;
    newt.c_cc[VMIN]  = 1;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) == -1) {
        throw std::runtime_error("error tcsetattr");
    }
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (oldf < 0) {
        throw std::runtime_error("error fcntl");
    }
    if (fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK) < 0) {  // NOLINT
        throw std::runtime_error("error fcntl");
    }

    std::cout << msg << std::flush;
    bool rtnv{};
    while(true) {
        int chr = getchar();
        if ((chr == 'y' ) || (chr == 'Y' )) {
            std::cout << "yes" << std::endl;
            rtnv = true;
            break;
        }
        if ((chr == 'n' ) || (chr == 'N' )) {
            std::cout << "no" << std::endl;
            rtnv = false;
            break;
        }
    }

    if (tcsetattr(STDIN_FILENO, TCSANOW, &oldt) == -1) {
        throw std::runtime_error("error tcsetattr");
    }
    if (fcntl(STDIN_FILENO, F_SETFL, oldf) < 0) {  // NOLINT
        throw std::runtime_error("error fcntl");
    }
    return rtnv;
}

tgctl::return_code tgctl_backup_create(const std::string& path_to_backup) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_datastore);
        ::tateyama::proto::datastore::request::Request requestBegin{};
        auto backup_begin = requestBegin.mutable_backup_begin();
        if (!FLAGS_label.empty()) {
            backup_begin->set_label(FLAGS_label);
        }
        auto responseBegin = transport->send<::tateyama::proto::datastore::response::BackupBegin>(requestBegin);
        requestBegin.clear_backup_begin();

        if (responseBegin) {
            auto rbgn = responseBegin.value();
            switch(rbgn.result_case()) {
            case ::tateyama::proto::datastore::response::BackupBegin::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::datastore::response::BackupBegin::ResultCase::kUnknownError:
                std::cerr << "BackupBegin error: " << rbgn.unknown_error().message() << std::endl;
                rtnv = tgctl::return_code::err;
                break;
            default:
                std::cerr << "BackupBegin result_case() error: " << std::endl;
                rtnv = tgctl::return_code::err;
            }

            if (rtnv == tgctl::return_code::ok) {
                auto backup_id = static_cast<std::int64_t>(rbgn.success().id());

                auto location = std::filesystem::path(path_to_backup);

                std::size_t total_bytes = 0;
                if(!FLAGS_monitor.empty()) {
                    for (auto&& file : rbgn.success().simple_source().files()) {
                        auto src = std::filesystem::path(file);
                        total_bytes += std::filesystem::file_size(src);
                    }
                }

                std::size_t completed_bytes = 0;
                for (auto&& file : rbgn.success().simple_source().files()) {
                    auto src = std::filesystem::path(file);
                    std::filesystem::copy_file(src, location / src.filename());
                    if(!FLAGS_monitor.empty()) {
                        completed_bytes += std::filesystem::file_size(src);
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
                    auto rend = responseEnd.value();
                    switch(rend.result_case()) {
                    case ::tateyama::proto::datastore::response::BackupEnd::ResultCase::kSuccess:
                        break;
                    case ::tateyama::proto::datastore::response::BackupEnd::ResultCase::kUnknownError:
                        std::cerr << "BackupEnd error: " << rend.unknown_error().message() << std::endl;
                        rtnv = tgctl::return_code::err;
                        break;
                    default:
                        std::cerr << "BackupEnd result_case() error: " << std::endl;
                        rtnv = tgctl::return_code::err;
                    }
                    if (rtnv == tgctl::return_code::ok) {
                        if (monitor_output) {
                            monitor_output->finish(true);
                        }
                        return rtnv;
                    }
                } else {
                    std::cerr << "BackupEnd response error: " << std::endl;
                    rtnv = tgctl::return_code::err;
                }
            }
        } else {
            std::cerr << "BackupBegin response error: " << std::endl;
            rtnv = tgctl::return_code::err;
        }

    } catch (std::runtime_error &ex) {
        std::cerr << ex.what() << std::endl;
        rtnv = tgctl::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code tgctl_backup_estimate() {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_datastore);
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
                std::cerr << " ends up with " << response.value().result_case() << std::endl;
                rtnv = tgctl::return_code::err;
            }
            if (rtnv == tgctl::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rtnv;
            }
        }
    } catch (std::runtime_error &e) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code tgctl_restore_backup(const std::string& path_to_backup) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_force) {
        try {
            if (!prompt("continue? (press y or n) : ")) {
                std::cout << "restore backup has been canceled." << std::endl;
                return tgctl::return_code::err;
            }
        } catch (std::runtime_error &ex) {
            std::cerr << "prompt fail, cause: " << ex.what() << std::endl;
            return tgctl::return_code::err;
        }
    }

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_datastore);
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
                std::cerr << " ends up with " << response.value().result_case() << std::endl;
                rtnv = tgctl::return_code::err;
            }
            if (rtnv == tgctl::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rtnv;
            }
        }
    } catch (std::runtime_error &e) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code tgctl_restore_backup_use_file_list(const std::string& path_to_backup) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_force) {
        try {
            if (!prompt("continue? (press y or n) : ")) {
                std::cout << "restore backup has been canceled." << std::endl;
                return tgctl::return_code::err;
            }
        } catch (std::runtime_error &ex) {
            std::cerr << "prompt fail, cause: " << ex.what() << std::endl;
            return tgctl::return_code::err;
        }
    }

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();

    try {
        auto parser = std::make_unique<file_list>();
        if (!parser->read_json(FLAGS_use_file_list)) {
            std::cerr << "error occurred in using the file_list (" << FLAGS_use_file_list << ")" << std::endl;
            if (monitor_output) {
                monitor_output->finish(false);
            }
            return tgctl::return_code::err;
        }
        if (!FLAGS_keep_backup) {
            std::cerr << "option --nokeep_backup is ignored when --use-file-list is specified" << std::endl;
        }

        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_datastore);
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
                std::cerr << " ends up with " << response.value().result_case() << std::endl;
                rtnv = tgctl::return_code::err;
            }
            if (rtnv == tgctl::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rtnv;
            }
        }
    } catch (std::runtime_error &e) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code tgctl_restore_tag(const std::string& tag_name) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();

    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_datastore);

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
                std::cerr << " ends up with " << response.value().result_case() << std::endl;
                rtnv = tgctl::return_code::err;
            }
            if (rtnv == tgctl::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rtnv;
            }
        }
    } catch (std::runtime_error &e) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

} //  tateyama::bootstrap::backup
