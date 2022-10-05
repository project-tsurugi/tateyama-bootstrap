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
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <stdexcept> // std::runtime_error

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/process/search_path.hpp>

#include <tateyama/logging.h>

#include "oltp.h"
#include "configuration.h"
#include "proc_mutex.h"
#include "monitor.h"
#include "status_info.h"

DECLARE_string(conf);
DEFINE_bool(quiesce, false, "invoke in quiesce mode");  // NOLINT for quiesce
DECLARE_string(label);
DEFINE_bool(maintenance_server, false, "invoke in maintenance_server mode");  // NOLINT for oltp_start() invoked from start_maintenance_server()
DECLARE_string(monitor);  // NOLINT

namespace tateyama::bootstrap {

constexpr std::string_view server_name_string = "tateyama-server";
constexpr std::string_view server_name_string_for_status = "Tsurugi OLTP database";
const std::size_t check_count = 100;
const int sleep_time_unit = 20;

using namespace tateyama::bootstrap::utils;

static bool status_check(proc_mutex::lock_state state, const boost::filesystem::path& lock_file) {
    std::unique_ptr<proc_mutex> file_mutex{};
    for (size_t i = 0; i < check_count; i++) {
        if (!file_mutex) {
            try {
                file_mutex = std::make_unique<proc_mutex>(lock_file, false);
            } catch (std::runtime_error &e) {
                usleep(sleep_time_unit * 1000);
                continue;
            }
        }
        if (file_mutex->check() == state) {
            return true;
        }
        usleep(sleep_time_unit * 5 * 1000);
    }
    return false;
}

int oltp_start([[maybe_unused]] int argc, char* argv[], char *argv0, bool need_check) {
    std::string server_name(server_name_string);
    pid_t child_pid = 0;
    if (child_pid = fork(); child_pid == 0) {
        boost::filesystem::path path_for_this{};
        if (auto a0f = boost::filesystem::path(argv0); a0f.parent_path().string().empty()) {
            path_for_this = boost::filesystem::canonical(boost::process::search_path(a0f));
        } else{
            path_for_this = boost::filesystem::canonical(a0f);
        }

        if (boost::filesystem::exists(path_for_this)) {
            argv[0] = const_cast<char *>(server_name.c_str());
            auto base = boost::filesystem::canonical(path_for_this).parent_path().parent_path();
//            close(0);  // FIXME activate this line when it completes
//            close(1);  // FIXME activate this line when it completes
//            close(2);  // FIXME activate this line when it completes
            execv((base / boost::filesystem::path("libexec") / boost::filesystem::path(server_name)).generic_string().c_str(), argv);
            perror("execvp");
        }
        LOG(ERROR) << "cannot find the location of " << argv0;
        exit(1);
    } else {  // to output pid to DVLOG(log_trace)
        DVLOG(log_trace) << "start " << server_name << ", pid = " << child_pid;
    }

    int rc = tateyama::bootstrap::return_code::ok;
    if (need_check) {
        std::unique_ptr<utils::monitor> monitor_output{};

        // command arguments
        gflags::SetUsageMessage("tateyama database server CLI");
        gflags::ParseCommandLineFlags(&argc, &argv, true);

        if(!FLAGS_monitor.empty()) {
            monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
            monitor_output->start();
        }

        auto bst_conf = utils::bootstrap_configuration(FLAGS_conf);
        if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
            std::size_t n = 0;

            std::unique_ptr<proc_mutex> file_mutex{};
            enum {
                init,
                ok,
                another,
            } check_result = init;
            for (size_t i = n; i < check_count; i++) {
                if (!file_mutex) {
                    try {
                        file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                    } catch (std::runtime_error &e) {
                        usleep(sleep_time_unit * 1000);
                        continue;
                    }
                }
                if (file_mutex->check() == proc_mutex::lock_state::locked) {
                    auto pid = file_mutex->pid(); 
                    if (pid == 0) {
                        continue;
                    }
                    check_result = (child_pid == pid) ? ok : another;
                    n = i;
                    break;
                }
                usleep(sleep_time_unit * 5 * 1000);
            }

            if (check_result == ok) {
                std::unique_ptr<status_info_bridge> status_info = std::make_unique<status_info_bridge>();
                // wait for creation of shared memory for status info
                for (size_t i = n; i < check_count; i++) {
                    if (status_info->attach(bst_conf.digest())) {
                        n = i;
                        break;
                    }
                    usleep(sleep_time_unit * 1000);
                }

                // wait for pid set
                bool checked = false;
                for (size_t i = n; i < check_count; i++) {
                    auto pid = status_info->pid();
                    if (pid == 0) {
                        usleep(sleep_time_unit * 1000);
                        continue;
                    }
                    if (child_pid == pid) {
                        if (monitor_output) {
                            monitor_output->finish(true);
                        }
                        return tateyama::bootstrap::return_code::ok;
                    }
                    LOG(ERROR) << "another " << server_name_string_for_status << " is running";
                    rc = tateyama::bootstrap::return_code::err;
                    checked = true;
                }
                if (!checked) {
                    LOG(ERROR) << "cannot confirm the server process within the specified time";
                    rc = tateyama::bootstrap::return_code::err;
                }
            } else {
                if (check_result == init) {
                    LOG(ERROR) << "cannot invoke a server process";
                } else {
                    LOG(ERROR) << "another " << server_name_string_for_status << " is running";
                }
                rc = tateyama::bootstrap::return_code::err;
            }
        } else {
            LOG(ERROR) << "cannot find the configuration file";
            rc = tateyama::bootstrap::return_code::err;
        }
        if (monitor_output) {
            monitor_output->finish(false);
        }
        return rc;
    }
    return tateyama::bootstrap::return_code::ok;  // when need_check is false, it should return ok
}

int oltp_shutdown_kill(int argc, char* argv[], bool force, bool status_output) {
    std::unique_ptr<utils::monitor> monitor_output{};

    // command arguments
    gflags::SetUsageMessage("tateyama database server CLI");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if(!FLAGS_monitor.empty() && status_output) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    int rc = tateyama::bootstrap::return_code::ok;
    auto bst_conf = utils::bootstrap_configuration(FLAGS_conf);
    if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
        try {
            auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
            if (force) {
                auto pid = file_mutex->pid(false);
                unlink(file_mutex->name().c_str());
                status_info_bridge::force_delete(bst_conf.digest());
                if (pid != 0) {
                    DVLOG(log_trace) << "kill (SIGKILL) to process " << pid << " and remove " << file_mutex->name();
                    kill(pid, SIGKILL);
                    usleep(sleep_time_unit * 5 * 1000);
                    if (monitor_output) {
                        monitor_output->finish(true);
                    }
                    return rc;
                }
                LOG(ERROR) << "contents of the file (" << file_mutex->name() << ") cannot be used";
                rc = tateyama::bootstrap::return_code::err;
            } else {
                std::unique_ptr<status_info_bridge> status_info = std::make_unique<status_info_bridge>(bst_conf.digest());
                if (!status_info->shutdown()) {
                    auto pid = file_mutex->pid(true);
                    if (pid != 0) {
                        DVLOG(log_trace) << "kill (SIGINT) to process " << pid;
                        kill(pid, SIGINT);
                        if (status_check(proc_mutex::lock_state::no_file, bst_conf.lock_file())) {
                            if (monitor_output) {
                                monitor_output->finish(true);
                            }
                            return rc;
                        }
                        LOG(ERROR) << "failed to shutdown, the server may still be alive";
                        rc = tateyama::bootstrap::return_code::err;
                    } else {
                        LOG(ERROR) << "contents of the file (" << file_mutex->name() << ") cannot be used";
                        rc = tateyama::bootstrap::return_code::err;
                    }
                } else {
                    LOG(ERROR) << "another shutdown is being conducted";
                    rc = tateyama::bootstrap::return_code::err;
                }
            }
        } catch (std::runtime_error &e) {
            LOG(ERROR) << e.what();
            rc = tateyama::bootstrap::return_code::err;
        }
    } else {
        LOG(ERROR) << "error in create_configuration";
        rc = tateyama::bootstrap::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

int oltp_status(int argc, char* argv[]) {
    std::unique_ptr<utils::monitor> monitor_output{};

    // command arguments
    gflags::SetUsageMessage("tateyama database server CLI");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if(!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    int rc = tateyama::bootstrap::return_code::ok;
    auto bst_conf = utils::bootstrap_configuration(FLAGS_conf);
    if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
        auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false, false);
        using state = proc_mutex::lock_state;
        switch (file_mutex->check()) {
        case state::no_file:
            if (monitor_output) {
                monitor_output->status(tateyama::bootstrap::utils::status::stop);
                break;
            }
            std::cout << server_name_string_for_status << " is INACTIVE" << std::endl;
            break;
        case state::locked:
        {
            std::unique_ptr<status_info_bridge> status_info{};
            try {
                status_info = std::make_unique<status_info_bridge>(bst_conf.digest());

                switch(status_info->whole()) {
                case tateyama::status_info::state::initial:
                case tateyama::status_info::state::ready:
                    if (monitor_output) {
                        monitor_output->status(tateyama::bootstrap::utils::status::ready);
                        break;
                    }
                    std::cout << server_name_string_for_status << " is BOOTING_UP" << std::endl;
                    break;
                case tateyama::status_info::state::activated:
                    if (monitor_output) {
                        monitor_output->status(tateyama::bootstrap::utils::status::activated);
                        break;
                    }
                    std::cout << server_name_string_for_status << " is RUNNING" << std::endl;
                    break;
                case tateyama::status_info::state::deactivating:
                    if (monitor_output) {
                        monitor_output->status(tateyama::bootstrap::utils::status::deactivating);
                        break;
                    }
                    std::cout << server_name_string_for_status << " is  SHUTTING_DOWN" << std::endl;
                    break;
                case tateyama::status_info::state::deactivated:
                    if (monitor_output) {
                        monitor_output->status(tateyama::bootstrap::utils::status::deactivated);
                        break;
                    }
                    std::cout << server_name_string_for_status << " is INACTIVE" << std::endl;
                    break;
                };
            } catch (std::exception& e) {
                LOG(ERROR) << e.what();
                rc = tateyama::bootstrap::return_code::err;
            }
            break;
        }
        default:
            if (monitor_output) {
                monitor_output->status(tateyama::bootstrap::utils::status::unknown);
                break;
            }
            std::cout << server_name_string_for_status << " is UNKNOWN" << std::endl;
            break;
        }
        if (rc == tateyama::bootstrap::return_code::ok) {
            if (monitor_output) {
                monitor_output->finish(true);
            }
            return rc;
        }
    } else {
        LOG(ERROR) << "error in create_configuration";
        rc = tateyama::bootstrap::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

int start_maintenance_server(int argc, char* argv[], char *argv0) {
    char *argvss[argc + 2];  // for "--maintenance_server" and nullptr

    std::size_t index = 0;
    argvss[index++] = const_cast<char*>("--maintenance_server");
    for(int i = 0; i < argc; i++) {
        argvss[index++] = argv[i];
    }
    argvss[index] = nullptr;

    return oltp_start(index, argvss, argv0, true);
}

}  // tateyama::bootstrap
