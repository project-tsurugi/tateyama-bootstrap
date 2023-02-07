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
#include <boost/process.hpp>

#include <tateyama/logging.h>

#include "oltp.h"
#include "monitor.h"
#include "status_info.h"
#include "client_wire.h"

DECLARE_string(conf);  // NOLINT
DECLARE_string(monitor);  // NOLINT
DECLARE_string(label);  // NOLINT
DECLARE_bool(quiesce);  // NOLINT
DECLARE_bool(maintenance_server);  // NOLINT
DECLARE_string(start_mode);  // NOLINT
DECLARE_int32(timeout); // NOLINT

DECLARE_string(location);  // NOLINT
DECLARE_bool(load);  // NOLINT
DECLARE_bool(tpch);  // NOLINT

namespace tateyama::bootstrap {

constexpr static int data_size = sizeof(pid_t);
constexpr std::string_view server_name_string = "tateyama-server";
constexpr std::string_view server_name_string_for_status = "Tsurugi OLTP database";
constexpr std::string_view undertaker_name_string = "undertaker";
const int sleep_time_unit_regular = 20;
const int sleep_time_unit_shutdown = 1000;
const std::size_t check_count_startup = 500;   // 10S
const std::size_t check_count_shutdown = 300;  // 300S (use sleep_time_unit_shutdown)
const std::size_t check_count_status = 10;     // 200mS
const std::size_t check_count_kill = 500;      // 10S
const int sleep_time_unit_mutex = 50;

using namespace tateyama::bootstrap::utils;

void build_args(std::vector<std::string>& args, tateyama::framework::boot_mode mode) {
    switch (mode) {
    case tateyama::framework::boot_mode::database_server:
        break;
    case tateyama::framework::boot_mode::maintenance_server:
        args.emplace_back("--maintenance_server");
        break;
    case tateyama::framework::boot_mode::quiescent_server:
        args.emplace_back("--quiesce");
        break;
    default:
        LOG(ERROR) << "illegal framework boot-up mode: " << tateyama::framework::to_string_view(mode);
        break;
    }
    if (!FLAGS_conf.empty()) {
        args.emplace_back("--conf");
        args.emplace_back(FLAGS_conf);
    }
    if (!FLAGS_label.empty()) {
        args.emplace_back("--label");
        args.emplace_back(FLAGS_label);
    }
    if (FLAGS_location != "./db") {
        args.emplace_back("--location");
        args.emplace_back(FLAGS_location);
    }
    if (FLAGS_load) {
        args.emplace_back("--load");
    }
    if (FLAGS_tpch) {
        args.emplace_back("--tpch");
    }
    if (FLAGS_v != 0) {
        args.emplace_back("--v");
        args.emplace_back(std::to_string(FLAGS_v));
    }
    if (FLAGS_logtostderr) {
        args.emplace_back("--logtostderr");
    }
}

return_code oltp_start(const std::string& argv0, bool need_check, tateyama::framework::boot_mode mode) {
    std::unique_ptr<utils::monitor> monitor_output{};

    if (!FLAGS_monitor.empty() && need_check) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }
    auto rc = tateyama::bootstrap::return_code::ok;
    auto bst_conf = utils::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);

    if (bst_conf.valid()) {
        if (!FLAGS_start_mode.empty()) {
            if (FLAGS_start_mode == "force") {
                auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                if (rc = oltp_kill(file_mutex.get(), bst_conf); rc != tateyama::bootstrap::return_code::ok) {
                    LOG(ERROR) << "cannot oltp kill before start";
                    if (monitor_output) {
                        monitor_output->finish(false);
                    }
                    return tateyama::bootstrap::return_code::err;
                }
            } else {
                LOG(ERROR) << "only \"force\" can be specified for the start-mode";
                if (monitor_output) {
                    monitor_output->finish(false);
                }
                return tateyama::bootstrap::return_code::err;
            }
        }

        std::string server_name(server_name_string);
        boost::filesystem::path path_for_this{};
        if (auto a0f = boost::filesystem::path(argv0); a0f.parent_path().string().empty()) {
            path_for_this = boost::filesystem::canonical(boost::process::search_path(a0f));
        } else{
            path_for_this = boost::filesystem::canonical(a0f);
        }
        if (!boost::filesystem::exists(path_for_this)) {
            LOG(ERROR) << "cannot find " << server_name_string;
            return tateyama::bootstrap::return_code::err;
        }

        int shm_id = shmget(IPC_PRIVATE, data_size, IPC_CREAT|0666);  // NOLINT
        // Shared memory create a new with IPC_CREATE
        if (shm_id == -1) {
            LOG(ERROR) << "error in shmget()";
            if (monitor_output) {
                monitor_output->finish(false);
            }
            return tateyama::bootstrap::return_code::err;
        }
        // Shared memory attach and convert char address
        auto *shm_data = shmat(shm_id, nullptr, 0);
        if (reinterpret_cast<std::uintptr_t>(shm_data) == static_cast<std::uintptr_t>(-1)) {  // NOLINT
            LOG(ERROR) << "error in shmat()";
            if (monitor_output) {
                monitor_output->finish(false);
            }
            return tateyama::bootstrap::return_code::err;
        }
        *static_cast<pid_t*>(shm_data) = 0;

        if (fork() == 0) {
            auto base = boost::filesystem::canonical(path_for_this).parent_path().parent_path();
            auto exec = base / boost::filesystem::path("libexec") / boost::filesystem::path(server_name);
            std::vector<std::string> args{};
            build_args(args, mode);
            boost::process::child c(exec, boost::process::args (args));
            pid_t child_pid = c.id();
            *static_cast<pid_t*>(shm_data) = child_pid;
            c.detach();

            std::string undertaker_name(undertaker_name_string);
            auto undertaker = base / boost::filesystem::path("libexec") / boost::filesystem::path(undertaker_name);
            execl(undertaker.string().c_str(), undertaker.string().c_str(), std::to_string(child_pid).c_str(), nullptr);  // NOLINT
            exit(-1);  // should not reach here
        }

        pid_t child_pid{};
        do {
            usleep(sleep_time_unit_regular * 1000);
            child_pid = *static_cast<pid_t*>(shm_data);
        } while (child_pid == 0);

        // Detach shred memory
        if (shmdt(shm_data) == -1) {
            LOG(ERROR) << "error in shmdt()";
        }
        // Remove shred memory
        if (shmctl(shm_id, IPC_RMID, nullptr)==-1) {
            LOG(ERROR) << "error in shctl()";
        }

        rc = tateyama::bootstrap::return_code::ok;
        if (need_check) {
            std::size_t check_count = check_count_startup;
            if (FLAGS_timeout > 0) {
                check_count = (1000L * FLAGS_timeout) / sleep_time_unit_regular;  // in mS
            } else if(FLAGS_timeout == 0) {
                check_count = INT64_MAX;  // practically infinite time
            }
            if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
                std::size_t n = 0;

                std::unique_ptr<proc_mutex> file_mutex{};
                // a flag indicating whether the child_pid matches the pid recorded in file_mutex
                enum {
                    init,
                    ok,
                    another,
                } check_result = init;
                pid_t pid_in_file_mutex{};
                for (size_t i = n; i < check_count; i++) {
                    if (!file_mutex) {
                        try {
                            file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                        } catch (std::runtime_error &e) {
                            usleep(sleep_time_unit_regular * 1000);
                            continue;
                        }
                    }
                    if (file_mutex->check() == proc_mutex::lock_state::locked) {
                        pid_in_file_mutex = file_mutex->pid();
                        if (pid_in_file_mutex == 0) {
                            continue;
                        }
                        check_result = (child_pid == pid_in_file_mutex) ? ok : another;
                        n = i;
                        break;
                    }
                    usleep(sleep_time_unit_regular * 5 * 1000);
                }
                if (check_result == ok) {  // case in which child_pid matches the pid recorded in file_mutex
                    std::unique_ptr<status_info_bridge> status_info = std::make_unique<status_info_bridge>();
                    // wait for creation of shared memory for status info
                    bool checked_status_info = false;
                    for (size_t i = n; i < check_count; i++) {
                        if (status_info->attach(bst_conf.digest())) {
                            n = i;
                            checked_status_info = true;
                            break;
                        }
                        usleep(sleep_time_unit_regular * 1000);
                    }
                    if (checked_status_info) {
                        // wait until pid is stored in the status_info
                        checked_status_info = false;
                        for (size_t i = n; i < check_count; i++) {
                            auto pid_in_status_info = status_info->pid();
                            if (pid_in_status_info == 0) {
                                usleep(sleep_time_unit_regular * 1000);
                                continue;
                            }
                            // observed that pid has been stored in the status_info
                            if (child_pid == pid_in_status_info) {  // case in which 'oltp start' command terminates successfully
                                if (monitor_output) {
                                    monitor_output->finish(true);
                                }
                                // wait for the server running when boot mode is maintenance_server (work around)
                                if (mode == tateyama::framework::boot_mode::maintenance_server) {
                                    while (status_info->whole() != tateyama::status_info::state::activated) {
                                        usleep(sleep_time_unit_regular * 1000);
                                    }
                                }
                                return tateyama::bootstrap::return_code::ok;
                            }
                            // case in which child_pid (== pid_in_file_mutex) != pid_in_status_info, which must be some serious error
                            LOG(ERROR) << "The pid stored in status_info(" << pid_in_status_info << ") and file_mutex(" << pid_in_file_mutex << ") do not match";
                            rc = tateyama::bootstrap::return_code::err;
                            checked_status_info = true;
                            break;
                        }
                    }
                    if (!checked_status_info) {
                        LOG(ERROR) << "cannot confirm the server process within the specified time";
                        rc = tateyama::bootstrap::return_code::err;
                    }
                } else if (check_result == another) {
                    LOG(ERROR) << "another " << server_name_string_for_status << " is running";
                    if (auto rv = kill(pid_in_file_mutex, 0); rv != 0) {  // the process (pid_in_file_mutex) is not alive
                        rc = tateyama::bootstrap::return_code::err;
                    }
                    // does not change the return code when the process is alive
                } else {  // case in which check_result == init
                    LOG(ERROR) << "cannot invoke a server process";
                    rc = tateyama::bootstrap::return_code::err;
                }
            } else {  // case in which bst_conf.create_configuration() returns nullptr
                LOG(ERROR) << "cannot find the configuration file";
                rc = tateyama::bootstrap::return_code::err;
            }
        } else {  // case in which need_check is false
            return tateyama::bootstrap::return_code::ok;
        }
    } else {
        LOG(ERROR) << "error in configuration file name";
        rc = tateyama::bootstrap::return_code::err;
    }

    if (monitor_output) {
        // records error to the monitor_output even if rc == tateyama::bootstrap::return_code::ok
        monitor_output->finish(rc == tateyama::bootstrap::return_code::ok);
    }
    return rc;
}

return_code oltp_kill(utils::proc_mutex* file_mutex, utils::bootstrap_configuration& bst_conf) {
    auto rc = tateyama::bootstrap::return_code::ok;
    auto pid = file_mutex->pid(false);
    if (pid != 0) {
        DVLOG(log_trace) << "kill (SIGKILL) to process " << pid << " and remove " << file_mutex->name() << "and status_info_bridge";
        kill(pid, SIGKILL);
        std::size_t check_count = check_count_kill;
        if (FLAGS_timeout > 0) {
            check_count = (1000L * FLAGS_timeout) / sleep_time_unit_regular;  // in mS
        } else if(FLAGS_timeout == 0) {
            check_count = INT64_MAX;  // practically infinite time
        }
        int sleep_time_unit = sleep_time_unit_regular;
        for (size_t i = 0; i < check_count; i++) {
            if (auto rv = kill(pid, 0); rv != 0) {
                if (errno != ESRCH) {
                    LOG(ERROR) << "cannot confirm whether the process has terminated or not due to an error " << errno;
                    rc = tateyama::bootstrap::return_code::err;
                }
                unlink(file_mutex->name().c_str());
                std::unique_ptr<status_info_bridge> status_info = std::make_unique<status_info_bridge>(bst_conf.digest());
                status_info->apply_shm_entry(tateyama::common::wire::session_wire_container::remove_shm_entry);
                return rc;
            }
            usleep(sleep_time_unit * 1000);
        }
        LOG(ERROR) << "cannot kill the " << server_name_string << " process within " << (sleep_time_unit_regular * check_count) / 1000 << " seconds";
        return tateyama::bootstrap::return_code::err;
    }
    LOG(ERROR) << "contents of the file (" << file_mutex->name() << ") cannot be used";
    return tateyama::bootstrap::return_code::err;
}

return_code oltp_shutdown(utils::proc_mutex* file_mutex, status_info_bridge* status_info) {
    auto rc = tateyama::bootstrap::return_code::ok;
    bool dot = false;

    if (!status_info->request_shutdown()) {
        return  tateyama::bootstrap::return_code::err;
    }
    usleep(sleep_time_unit_mutex * 1000);

    std::size_t check_count = check_count_shutdown;
    int sleep_time_unit = sleep_time_unit_shutdown;
    size_t timeout = 1000L * check_count * sleep_time_unit;  // in uS
    if (FLAGS_timeout > 0) {
        timeout = 1000000L * FLAGS_timeout;  // in uS
    } else if(FLAGS_timeout == 0) {
        timeout = INT64_MAX;  // practically infinite time
    }
    for (size_t i = 0; i < timeout; i += sleep_time_unit * 1000) {
        if (file_mutex->check() == proc_mutex::lock_state::no_file) {
            if (dot) {
                std::cout << std::endl;
            }
            return rc;
        }
        usleep(sleep_time_unit * 1000);
        std::cout << "."  << std::flush;
        dot = true;
    }
    if (dot) {
        std::cout << std::endl;
    }
    LOG(ERROR) << "shutdown operation is still in progress, check it after some time";
    return tateyama::bootstrap::return_code::err;
}

return_code oltp_shutdown_kill(bool force, bool status_output) {
    std::unique_ptr<utils::monitor> monitor_output{};

    if (!FLAGS_monitor.empty() && status_output) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = tateyama::bootstrap::return_code::ok;
    auto bst_conf = utils::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (bst_conf.valid()) {
        if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
            try {
                auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                if (force) {
                    rc = oltp_kill(file_mutex.get(), bst_conf);
                    if (rc == tateyama::bootstrap::return_code::ok) {
                        if (monitor_output) {
                            monitor_output->finish(true);
                        }
                        return rc;
                    }
                } else {
                    std::unique_ptr<status_info_bridge> status_info = std::make_unique<status_info_bridge>(bst_conf.digest());
                    if (!status_info->is_shutdown_requested()) {
                        rc = oltp_shutdown(file_mutex.get(), status_info.get());
                        if (rc == tateyama::bootstrap::return_code::ok) {
                            if (monitor_output) {
                                monitor_output->finish(true);
                            }
                            return rc;
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
    } else {
        LOG(ERROR) << "error in configuration file name";
        rc = tateyama::bootstrap::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

return_code oltp_status() {
    std::unique_ptr<utils::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<utils::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = tateyama::bootstrap::return_code::ok;
    auto bst_conf = utils::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (bst_conf.valid()) {
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
                for (std::size_t i = 0; i < check_count_status; i++) {
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
                        break;
                    } catch (std::exception& e) {
                        if (i < (check_count_status - 1)) {
                            usleep(sleep_time_unit_regular * 1000);
                        } else {
                            LOG(ERROR) << e.what();
                            rc = tateyama::bootstrap::return_code::err;
                        }
                    }
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
    } else {
        LOG(ERROR) << "error in configuration file name";
        rc = tateyama::bootstrap::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

static pid_t get_pid() {
    auto bst_conf = utils::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (bst_conf.valid()) {
        if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
            auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
            return file_mutex->pid(false);  // may throws std::runtime_error
        }
        throw std::runtime_error("error in create_configuration");
    }
    throw std::runtime_error("error in configuration file name");
}

return_code oltp_diagnostic() {
    try {
        kill(get_pid(), SIGHUP);
        return tateyama::bootstrap::return_code::ok;
    } catch (std::runtime_error &e) {
        return tateyama::bootstrap::return_code::err;
    }
}

return_code oltp_pid() {
    try {
        std::cout << get_pid() << std::endl;
        return tateyama::bootstrap::return_code::ok;
    } catch (std::runtime_error &e) {
        return tateyama::bootstrap::return_code::err;
    }
}

}  // tateyama::bootstrap
