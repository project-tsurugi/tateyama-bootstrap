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

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/process.hpp>

#include <tateyama/logging.h>

#include "tateyama/server/status_info.h"
#include "tateyama/transport/client_wire.h"
#include "tateyama/monitor/monitor.h"
#include "process.h"

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

namespace tateyama::process {

constexpr static int data_size = sizeof(pid_t);
constexpr std::string_view server_name_string = "tsurugidb";
constexpr std::string_view server_name_string_for_status = "Tsurugi OLTP database";
constexpr std::string_view undertaker_name_string = "tgundertaker";
const int sleep_time_unit_regular = 20;
const int sleep_time_unit_shutdown = 1000;
const std::size_t check_count_startup = 500;   // 10S
const std::size_t check_count_shutdown = 300;  // 300S (use sleep_time_unit_shutdown)
const std::size_t check_count_status = 10;     // 200mS
const std::size_t check_count_kill = 500;      // 10S
const int sleep_time_unit_mutex = 50;

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
        std::cerr << "illegal framework boot-up mode: " << tateyama::framework::to_string_view(mode) << std::endl;
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

oltp::return_code oltp_start(const std::string& argv0, bool need_check, tateyama::framework::boot_mode mode) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty() && need_check) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }
    auto rc = oltp::return_code::ok;
    auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);

    if (bst_conf.valid()) {
        if (!FLAGS_start_mode.empty()) {
            if (FLAGS_start_mode == "force") {
                auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                if (rc = oltp_kill(file_mutex.get(), bst_conf); rc != oltp::return_code::ok) {
                    std::cerr << "cannot oltp kill before start" << std::endl;
                    if (monitor_output) {
                        monitor_output->finish(false);
                    }
                    return oltp::return_code::err;
                }
            } else {
                std::cerr << "only \"force\" can be specified for the start-mode" << std::endl;
                if (monitor_output) {
                    monitor_output->finish(false);
                }
                return oltp::return_code::err;
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
            std::cerr << "cannot find " << server_name_string << std::endl;
            return oltp::return_code::err;
        }

        int shm_id = shmget(IPC_PRIVATE, data_size, IPC_CREAT|0666);  // NOLINT
        // Shared memory create a new with IPC_CREATE
        if (shm_id == -1) {
            std::cerr << "error in shmget()" << std::endl;
            if (monitor_output) {
                monitor_output->finish(false);
            }
            return oltp::return_code::err;
        }
        // Shared memory attach and convert char address
        auto *shm_data = shmat(shm_id, nullptr, 0);
        if (reinterpret_cast<std::uintptr_t>(shm_data) == static_cast<std::uintptr_t>(-1)) {  // NOLINT
            std::cerr << "error in shmat()" << std::endl;
            if (monitor_output) {
                monitor_output->finish(false);
            }
            return oltp::return_code::err;
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
            std::cerr << "error in shmdt()" << std::endl;
        }
        // Remove shred memory
        if (shmctl(shm_id, IPC_RMID, nullptr)==-1) {
            std::cerr << "error in shctl()" << std::endl;
        }

        rc = oltp::return_code::ok;
        if (need_check) {
            std::size_t check_count = check_count_startup;
            if (FLAGS_timeout > 0) {
                check_count = (1000L * FLAGS_timeout) / sleep_time_unit_regular;  // in mS
            } else if(FLAGS_timeout == 0) {
                check_count = INT64_MAX;  // practically infinite time
            }
            if (auto conf = bst_conf.get_configuration(); conf != nullptr) {
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
                    auto status_info = std::make_unique<server::status_info_bridge>();
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
                                return oltp::return_code::ok;
                            }
                            // case in which child_pid (== pid_in_file_mutex) != pid_in_status_info, which must be some serious error
                            std::cerr << "The pid stored in status_info(" << pid_in_status_info << ") and file_mutex(" << pid_in_file_mutex << ") do not match" << std::endl;
                            rc = oltp::return_code::err;
                            checked_status_info = true;
                            break;
                        }
                    }
                    if (!checked_status_info) {
                        std::cerr << "cannot confirm the server process within the specified time" << std::endl;
                        rc = oltp::return_code::err;
                    }
                } else if (check_result == another) {
                    std::cerr << "another " << server_name_string_for_status << " is running" << std::endl;
                    if (auto rv = kill(pid_in_file_mutex, 0); rv != 0) {  // the process (pid_in_file_mutex) is not alive
                        rc = oltp::return_code::err;
                    }
                    // does not change the return code when the process is alive
                } else {  // case in which check_result == init
                    std::cerr << "cannot invoke a server process" << std::endl;
                    rc = oltp::return_code::err;
                }
            } else {  // case in which bst_conf.create_configuration() returns nullptr
                std::cerr << "cannot find the configuration file" << std::endl;
                rc = oltp::return_code::err;
            }
        } else {  // case in which need_check is false
            return oltp::return_code::ok;
        }
    } else {
        std::cerr << "error in configuration file name" << std::endl;
        rc = oltp::return_code::err;
    }

    if (monitor_output) {
        // records error to the monitor_output even if rc == oltp::return_code::ok
        monitor_output->finish(rc == oltp::return_code::ok);
    }
    return rc;
}

enum status_check_result {
    undefined = 0,
    error_in_conf_file_name,
    error_in_create_conf,
    no_file,
    not_locked,
    initial,
    ready,
    activated,
    deactivating,
    deactivated,
    status_check_count_over,
    error_in_file_mutex_check
};

static status_check_result status_check_internal() {
    auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (!bst_conf.valid()) {
        return status_check_result::error_in_conf_file_name;
    }
    if (auto conf = bst_conf.get_configuration(); conf == nullptr) {
        return status_check_result::error_in_create_conf;
    }
    auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false, false);
    using state = proc_mutex::lock_state;
    switch (file_mutex->check()) {
    case state::no_file:
        return status_check_result::no_file;
    case state::not_locked:
        return status_check_result::not_locked;
    case state::locked:
    {
        for (std::size_t i = 0; i < check_count_status; i++) {
            std::unique_ptr<server::status_info_bridge> status_info{};
            try {
                status_info = std::make_unique<server::status_info_bridge>(bst_conf.digest());
                
                switch(status_info->whole()) {
                case tateyama::status_info::state::initial:
                    return status_check_result::initial;
                case tateyama::status_info::state::ready:
                    return status_check_result::ready;
                case tateyama::status_info::state::activated:
                    return status_check_result::activated;
                case tateyama::status_info::state::deactivating:
                    return status_check_result::deactivating;
                case tateyama::status_info::state::deactivated:
                    return status_check_result::deactivated;
                }
            } catch (std::exception& e) {
                if (i < (check_count_status - 1)) {
                    usleep(sleep_time_unit_regular * 1000);
                } else {
                    return status_check_result::status_check_count_over;
                }
            }
        }
        return status_check_result::status_check_count_over;
    }
    case state::error:
        return status_check_result::error_in_file_mutex_check;
    }
    return status_check_result::undefined;
}

oltp::return_code oltp_kill(proc_mutex* file_mutex, configuration::bootstrap_configuration& bst_conf) {
    auto rc = oltp::return_code::ok;
    auto pid = file_mutex->pid(false);
    if (pid != 0) {
        kill(pid, SIGKILL);
        std::size_t check_count = check_count_kill;
        if (FLAGS_timeout > 0) {
            check_count = (1000L * FLAGS_timeout) / sleep_time_unit_regular;  // in mS
        } else if(FLAGS_timeout == 0) {
            check_count = INT64_MAX;  // practically infinite time
        }
        int sleep_time_unit = sleep_time_unit_regular;
        for (size_t i = 0; i < check_count; i++) {
            switch(status_check_internal()) {
            case status_check_result::not_locked:
            {
                unlink(file_mutex->name().c_str());
                auto status_info = std::make_unique<server::status_info_bridge>(bst_conf.digest());
                status_info->apply_shm_entry(tateyama::common::wire::session_wire_container::remove_shm_entry);
                status_info->force_delete();
                return rc;
            }
            default:
                break;
            }
            usleep(sleep_time_unit * 1000);
        }
        std::cerr << "cannot kill the " << server_name_string << " process within " << (sleep_time_unit_regular * check_count) / 1000 << " seconds" << std::endl;
        return oltp::return_code::err;
    }
    std::cerr << "contents of the file (" << file_mutex->name() << ") cannot be used" << std::endl;
    return oltp::return_code::err;
}

oltp::return_code oltp_shutdown(proc_mutex* file_mutex, server::status_info_bridge* status_info) {
    auto rc = oltp::return_code::ok;
    bool dot = false;

    if (!status_info->request_shutdown()) {
        return  oltp::return_code::err;
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
    std::cerr << "shutdown operation is still in progress, check it after some time" << std::endl;
    return oltp::return_code::err;
}

oltp::return_code oltp_shutdown_kill(bool force, bool status_output) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty() && status_output) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = oltp::return_code::ok;
    auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (bst_conf.valid()) {
        if (auto conf = bst_conf.get_configuration(); conf != nullptr) {
            try {
                auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                if (force) {
                    rc = oltp_kill(file_mutex.get(), bst_conf);
                    if (rc == oltp::return_code::ok) {
                        if (monitor_output) {
                            monitor_output->finish(true);
                        }
                        return rc;
                    }
                } else {
                    auto status_info = std::make_unique<server::status_info_bridge>(bst_conf.digest());
                    if (!status_info->is_shutdown_requested()) {
                        rc = oltp_shutdown(file_mutex.get(), status_info.get());
                        if (rc == oltp::return_code::ok) {
                            if (monitor_output) {
                                monitor_output->finish(true);
                            }
                            return rc;
                        }
                    } else {
                        std::cerr << "another shutdown is being conducted" << std::endl;
                        rc = oltp::return_code::err;
                    }
                }
            } catch (std::runtime_error &e) {
                std::cerr << e.what() << std::endl;
                rc = oltp::return_code::err;
            }
        } else {
            std::cerr << "error in create_configuration" << std::endl;
            rc = oltp::return_code::err;
        }
    } else {
        std::cerr << "error in configuration file name" << std::endl;
        rc = oltp::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

oltp::return_code oltp_status() {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rc = oltp::return_code::ok;
    switch(status_check_internal()) {
    case status_check_result::no_file:
        if (monitor_output) {
            monitor_output->status(monitor::status::stop);
            break;
        }
        std::cout << server_name_string_for_status << " is INACTIVE" << std::endl;
        break;
    case status_check_result::initial:
    case status_check_result::ready:
        if (monitor_output) {
            monitor_output->status(monitor::status::ready);
            break;
        }
        std::cout << server_name_string_for_status << " is BOOTING_UP" << std::endl;
        break;
    case status_check_result::activated:
        if (monitor_output) {
            monitor_output->status(monitor::status::activated);
            break;
        }
        std::cout << server_name_string_for_status << " is RUNNING" << std::endl;
        break;
    case status_check_result::deactivating:
        if (monitor_output) {
            monitor_output->status(monitor::status::deactivating);
            break;
        }
        std::cout << server_name_string_for_status << " is  SHUTTING_DOWN" << std::endl;
        break;
    case status_check_result::deactivated:
        if (monitor_output) {
            monitor_output->status(monitor::status::deactivated);
            break;
        }
        std::cout << server_name_string_for_status << " is INACTIVE" << std::endl;
        break;
    case status_check_result::status_check_count_over:
        std::cerr << "cannot check the state within the specified time" << std::endl;
        rc = oltp::return_code::err;
        break;
    case status_check_result::not_locked:
        if (monitor_output) {
            monitor_output->status(monitor::status::unknown);
            break;
        }
        std::cout << server_name_string_for_status << " is UNKNOWN" << std::endl;
        break;
    case status_check_result::error_in_create_conf:
        std::cerr << "error in create_configuration" << std::endl;
        rc = oltp::return_code::err;
        break;
    case status_check_result::error_in_conf_file_name:
        std::cerr << "error in configuration file name" << std::endl;
        rc = oltp::return_code::err;
        break;
    default:
        std::cerr << "should not reach here" << std::endl;
        rc = oltp::return_code::err;
        break;
    }

    if (rc == oltp::return_code::ok) {
        if (monitor_output) {
            monitor_output->finish(true);
        }
        return rc;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rc;
}

static pid_t get_pid() {
    auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (bst_conf.valid()) {
        if (auto conf = bst_conf.get_configuration(); conf != nullptr) {
            auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
            return file_mutex->pid(false);  // may throws std::runtime_error
        }
        throw std::runtime_error("error in create_configuration");
    }
    throw std::runtime_error("error in configuration file name");
}

oltp::return_code oltp_diagnostic() {
    try {
        kill(get_pid(), SIGHUP);
        return oltp::return_code::ok;
    } catch (std::runtime_error &e) {
        return oltp::return_code::err;
    }
}

oltp::return_code oltp_pid() {
    try {
        std::cout << get_pid() << std::endl;
        return oltp::return_code::ok;
    } catch (std::runtime_error &e) {
        return oltp::return_code::err;
    }
}

}  // tateyama::process
