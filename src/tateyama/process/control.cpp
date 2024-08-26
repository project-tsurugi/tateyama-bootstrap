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
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <unistd.h>
#include <stdexcept> // std::runtime_error

#include <gflags/gflags.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <tateyama/logging.h>

#include "tateyama/server/status_info.h"
#include "tateyama/transport/client_wire.h"
#include "tateyama/monitor/monitor.h"
#include "process.h"

DECLARE_string(conf);
DECLARE_string(monitor);
DECLARE_string(label);
DECLARE_bool(quiesce);
DECLARE_bool(maintenance_server);
DECLARE_string(start_mode);
DECLARE_int32(timeout);
DECLARE_bool(quiet);
DECLARE_bool(graceful);
DECLARE_bool(forceful);

DECLARE_string(location);  // obsolete
DECLARE_bool(load);  // obsolete
DECLARE_bool(tpch);  // obsolete

namespace tateyama::process {

constexpr std::string_view server_name_string = "tsurugidb";
constexpr std::string_view server_name_string_for_status = "Tsurugi OLTP database";
const std::size_t sleep_time_unit_regular = 20;
const std::size_t sleep_time_unit_shutdown = 1000;
const std::size_t check_count_startup = 500;   // 10S
const std::size_t check_count_shutdown = 300;  // 300S (use sleep_time_unit_shutdown)
const std::size_t check_count_status = 10;     // 200mS
const std::size_t check_count_kill = 500;      // 10S
const std::size_t sleep_time_unit_mutex = 50;

enum status_check_result {
    undefined = 0,
    error_in_conf_file_name,     // bst_conf is not valid
    error_in_create_conf,        // bst_conf.get_configuration() returns nullptr
    no_file,                     // mutex file: x
    not_locked,                  // mutex file: o, mutex file lock: x
    initial,                     // mutex file: o, mutex file lock: o, status info: initial
    ready,                       // mutex file: o, mutex file lock: o, status info: ready
    activated,                   // mutex file: o, mutex file lock: o, status info: activated (no longer used)
    deactivating,                // mutex file: o, mutex file lock: o, status info: deactivating
    deactivated,                 // mutex file: o, mutex file lock: o, status info: deactivated
    boot_error,                  // mutex file: o, mutex file lock: o, status info: boot_error
    status_check_count_over,     // mutex file: o, mutex file lock: o, status info: check has resulted in time over
    error_in_file_mutex_check    // mutex file: o, mutex file lock: ?
};

static status_check_result status_check_internal(tateyama::configuration::bootstrap_configuration& bst_conf) {
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
                case tateyama::status_info::state::boot_error:
                    return status_check_result::boot_error;
                }
            } catch (std::exception& e) {
                if (i < (check_count_status - 1)) {
                    usleep(sleep_time_unit_regular * 1000);
                    continue;
                }
                return status_check_result::status_check_count_over;
            }
            usleep(sleep_time_unit_regular * 1000);
        }
        return status_check_result::status_check_count_over;
    }
    case state::error:
        return status_check_result::error_in_file_mutex_check;
    }
    return status_check_result::undefined;
}
static status_check_result status_check_internal() {
    auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    return status_check_internal(bst_conf);
}

bool is_running() {
    return status_check_internal() == status_check_result::activated;
}

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
}

tgctl::return_code tgctl_start(const std::string& argv0, bool need_check, tateyama::framework::boot_mode mode) { //NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty() && need_check) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }
    auto rtnv = tgctl::return_code::ok;
    auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);

    if (bst_conf.valid()) {
        if (!FLAGS_start_mode.empty()) {
            if (FLAGS_start_mode == "force") {
                auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                if (rtnv = tgctl_kill(file_mutex.get(), bst_conf); rtnv != tgctl::return_code::ok) {
                    std::cerr << "cannot tgctl kill before start" << std::endl;
                    if (monitor_output) {
                        monitor_output->finish(false);
                    }
                    return tgctl::return_code::err;
                }
            } else {
                std::cerr << "only \"force\" can be specified for the start-mode" << std::endl;
                if (monitor_output) {
                    monitor_output->finish(false);
                }
                return tgctl::return_code::err;
            }
        }

        auto state = status_check_internal(bst_conf);
        if (state == status_check_result::initial ||
            state == status_check_result::ready ||
            state == status_check_result::activated ||
            state == status_check_result::deactivating) {
            if (!FLAGS_quiet) {
                std::cout << "could not launch " << server_name_string << ", as "
                          << server_name_string << " is already running." << std::endl;
            }
            if (monitor_output) {
                monitor_output->finish(false);
            }
            return tgctl::return_code::err;
        }

        boost::filesystem::path base_path{};
        try {
            base_path = get_base_path(argv0);
        } catch (std::runtime_error &e) {
            std::cerr << e.what() << std::endl;
            return tgctl::return_code::err;
        }

        std::string server_name(server_name_string);
        auto exec = base_path / boost::filesystem::path("libexec") / boost::filesystem::path(server_name);
        std::vector<std::string> args{};
        build_args(args, mode);
        boost::process::child cld(exec, boost::process::args (args));
        pid_t child_pid = cld.id();
        cld.detach();

        rtnv = tgctl::return_code::ok;
        if (need_check) {
            std::size_t check_count = check_count_startup;
            if (FLAGS_timeout > 0) {
                check_count = (1000L * FLAGS_timeout) / sleep_time_unit_regular;  // in mS
            } else if(FLAGS_timeout == 0) {
                check_count = INT64_MAX;  // practically infinite time
            }
            if (auto conf = bst_conf.get_configuration(); conf != nullptr) {
                std::size_t chkn = 0;

                std::unique_ptr<proc_mutex> file_mutex{};
                // a flag indicating whether the child_pid matches the pid recorded in file_mutex
                enum {
                    init,
                    ok,
                    another,
                } check_result = init;
                pid_t pid_in_file_mutex{};
                for (size_t i = chkn; i < check_count; i++) {
                    if (!file_mutex) {
                        try {
                            file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                        } catch (std::runtime_error &e) {
                            usleep(sleep_time_unit_regular * 1000);
                            continue;
                        }
                    }
                    if (file_mutex->check() != proc_mutex::lock_state::no_file) {
                        pid_in_file_mutex = file_mutex->pid(false);
                        if (pid_in_file_mutex != 0) {
                            if (child_pid == pid_in_file_mutex) {
                                check_result = ok;
                                chkn = i;
                                break;
                            }
                            if (auto krv = kill(pid_in_file_mutex, 0); krv == 0) {  // the process (pid_in_file_mutex) is alive
                                check_result = another;
                                chkn = i;
                                break;
                            }
                        }
                    }
                    usleep(sleep_time_unit_regular * 1000);
                }
                if (check_result == ok) {  // case in which child_pid matches the pid recorded in file_mutex
                    auto status_info = std::make_unique<server::status_info_bridge>();
                    // wait for creation of shared memory for status info
                    bool status_info_ready = false;
                    for (size_t i = chkn; i < check_count; i++) {
                        if (status_info->attach(bst_conf.digest())) {
                            chkn = i;
                            status_info_ready = true;
                            break;
                        }
                        usleep(sleep_time_unit_regular * 1000);
                    }
                    if (status_info_ready) {
                        // wait until pid is stored in the status_info
                        for (size_t i = chkn; i < check_count; i++) {
                            auto pid_in_status_info = status_info->pid();
                            if (pid_in_status_info == 0) {
                                usleep(sleep_time_unit_regular * 1000);
                                continue;
                            }
                            // observed that pid has been stored in the status_info
                            if (child_pid == pid_in_status_info) {  // case in which 'tsurugi db is booting up
                                switch (status_check_internal(bst_conf)) {
                                case status_check_result::ready:
                                case status_check_result::activated:
                                    if (monitor_output) {
                                        monitor_output->finish(true);
                                    }
                                    if (!FLAGS_quiet) {
                                        std::cout << "successfully launched " << server_name_string << "." << std::endl;
                                    }
                                    return tgctl::return_code::ok;

                                case status_check_result::not_locked:
                                case status_check_result::boot_error:
                                    if (monitor_output) {
                                        monitor_output->finish(false);
                                    }
                                    if (!FLAGS_quiet) {
                                        std::cout << "could not launch " << server_name_string << ", as "
                                                  << server_name_string << " exited due to some error." << std::endl;
                                    }
                                    return tgctl::return_code::err;

                                case status_check_result::no_file:
                                case status_check_result::initial:
                                    usleep(sleep_time_unit_regular * 1000);
                                    continue;

                                case status_check_result::deactivating:
                                case status_check_result::deactivated:
                                    if (!FLAGS_quiet) {
                                        std::cout << "could not launch " << server_name_string << ", as "
                                                  << "shutdown procedure for " << server_name_string << " is taking place now." << std::endl;
                                    }
                                    rtnv = tgctl::return_code::err;
                                    break;

                                case status_check_result::status_check_count_over:
                                    i += check_count_status;
                                    continue;

                                case status_check_result::undefined:
                                case status_check_result::error_in_conf_file_name:
                                case status_check_result::error_in_create_conf:
                                case status_check_result::error_in_file_mutex_check:
                                    if (!FLAGS_quiet) {
                                        std::cout << "failed to confirm " << server_name_string << " launch within " << (sleep_time_unit_regular * check_count) / 1000 << " seconds, because "
                                                  << "the status information is inconsistent." << std::endl;
                                    }
                                    rtnv = tgctl::return_code::err;
                                    break;
                                }
                            } else {
                                // case in which child_pid (== pid_in_file_mutex) != pid_in_status_info, which must be some serious error
                                if (!FLAGS_quiet) {
                                    std::cout << "failed to confirm " << server_name_string << " launch within " << (sleep_time_unit_regular * check_count) / 1000 << " seconds, because "
                                              << "the pid stored in status_info(" << pid_in_status_info << ") and file_mutex(" << pid_in_file_mutex << ") do not match." << std::endl;
                                }
                                rtnv = tgctl::return_code::err;
                            }
                            break;
                        }
                        if (!FLAGS_quiet) {
                            std::cout << "failed to confirm " << server_name_string << " launch within " << (sleep_time_unit_regular * check_count) / 1000 << " seconds, because "
                                      << "the launch is still in progres." << std::endl;
                        }
                        rtnv = tgctl::return_code::err;
                    } else {
                        if (!FLAGS_quiet) {
                            if (file_mutex->check() == proc_mutex::lock_state::locked) {
                                std::cout << "failed to confirm " << server_name_string << " launch within " << (sleep_time_unit_regular * check_count) / 1000 << " seconds, because "
                                          << "the launch is still in progres." << std::endl;
                            } else {    // if the lock is not held by the tsurugidb process,  this means that the tsurugidb boot has failed.
                                std::cout << "could not launch " << server_name_string << ", as " << server_name_string << " exited due to some error." << std::endl;
                            }
                        }
                        rtnv = tgctl::return_code::err;
                    }
                } else if (check_result == another) {
                    if (!FLAGS_quiet) {
                        std::cout << "could not launch " << server_name_string << " as " << server_name_string << " is already running." << std::endl;
                    }
                    rtnv = tgctl::return_code::err;
                } else {  // case in which check_result == init, meaning status check error
                    if (!FLAGS_quiet) {
                        std::cout << "failed to confirm " << server_name_string << " launch within " << (sleep_time_unit_regular * check_count) / 1000 << " seconds, because "
                                  << "it failed to check server status." << std::endl;
                    }
                    rtnv = tgctl::return_code::err;
                }
            } else {  // case in which bst_conf.create_configuration() returns nullptr
                if (!FLAGS_quiet) {
                    std::cout << "could not launch " << server_name_string << ", as the configuration file cannot be found." << std::endl;
                }
                rtnv = tgctl::return_code::err;
            }
        } else {  // case in which need_check is false
            return tgctl::return_code::ok;
        }
    } else {
        if (!FLAGS_quiet) {
            std::cout << "could not launch " << server_name_string << ", as there is no valid configuration file." << std::endl;
        }
        rtnv = tgctl::return_code::err;
    }

    if (monitor_output) {
        // records error to the monitor_output even if rtnv == tgctl::return_code::ok
        monitor_output->finish(rtnv == tgctl::return_code::ok);
    }
    return rtnv;
}

tgctl::return_code tgctl_kill(proc_mutex* file_mutex, configuration::bootstrap_configuration& bst_conf) {
    auto rtnv = tgctl::return_code::ok;
    auto pid = file_mutex->pid(false);
    if (pid != 0) {
        kill(pid, SIGKILL);
        std::size_t check_count = check_count_kill;
        int sleep_time_unit = sleep_time_unit_regular;
        if (FLAGS_timeout > 0) {
            check_count = (1000L * FLAGS_timeout) / sleep_time_unit;  // in mS
        } else if(FLAGS_timeout == 0) {
            check_count = INT64_MAX;  // practically infinite time
        }
        for (size_t i = 0; i < check_count; i++) {
            switch(status_check_internal()) {
            case status_check_result::not_locked:
            {
                unlink(file_mutex->name().c_str());
                auto status_info = std::make_unique<server::status_info_bridge>(bst_conf.digest());
                status_info->apply_shm_entry(tateyama::common::wire::session_wire_container::remove_shm_entry);
                status_info->force_delete();
                if (!FLAGS_quiet) {
                    std::cout << "successfully killed " << server_name_string << "." << std::endl;
                }
                return rtnv;
            }
            default:
                break;
            }
            usleep(sleep_time_unit * 1000);
        }
        if (!FLAGS_quiet) {
            std::cout << "could not kill " << server_name_string << " within " << (sleep_time_unit_regular * check_count_kill) / 1000 << " seconds, as "
                      << "kill is still in progress." << std::endl;
        }
        return tgctl::return_code::err;
    }
    if (!FLAGS_quiet) {
        std::cout << "could not kill " << server_name_string << " within " << (sleep_time_unit_regular * check_count_kill) / 1000 << " seconds, as "
                  << "contents of the file (" << file_mutex->name() << ") cannot be used." << std::endl;
    }
    return tgctl::return_code::err;
}

tgctl::return_code tgctl_shutdown(proc_mutex* file_mutex, server::status_info_bridge* status_info) {
    auto rtnv = tgctl::return_code::ok;
    bool dot = false;

    if (FLAGS_graceful && FLAGS_forceful) {
        std::cout << "shutdown was not performed, as both forceful and graceful options specified" << std::endl;
        return  tgctl::return_code::err;
    }
    auto shutdown_type = tateyama::status_info::shutdown_type::forceful;
    if (FLAGS_graceful) {
        shutdown_type = tateyama::status_info::shutdown_type::graceful;
    }
    if (!status_info->request_shutdown(shutdown_type)) {
        if (!FLAGS_quiet) {
            std::cout << "shutdown was not performed, as shutdown is already requested." << std::endl;
        }
        return  tgctl::return_code::err;
    }
    usleep(sleep_time_unit_mutex * 1000);

    std::size_t check_count = check_count_shutdown;
    int sleep_time_unit = sleep_time_unit_shutdown;
    if (FLAGS_timeout > 0) {
        check_count = (1000L * FLAGS_timeout) / sleep_time_unit;  // in uS
    } else if(FLAGS_timeout == 0) {
        check_count = INT64_MAX;  // practically infinite time
    }
    for (size_t i = 0; i < check_count; i++) {
        if (file_mutex->check() == proc_mutex::lock_state::no_file) {
            if (dot) {
                std::cout << std::endl;
            }
            if (!FLAGS_quiet) {
                std::cout << "successfully shutdown " << server_name_string << "." << std::endl;
            }
            return rtnv;
        }
        usleep(sleep_time_unit * 1000);
        std::cout << "."  << std::flush;
        dot = true;
    }
    if (dot) {
        std::cout << std::endl;
    }
    if (!FLAGS_quiet) {
        std::cout << "could not shutdown " << server_name_string << " within " << (sleep_time_unit_shutdown * check_count_shutdown) / 1000 << " seconds, as shutdown is still in progress." << std::endl;
    }
    return tgctl::return_code::err;
}

tgctl::return_code tgctl_shutdown_kill(bool force, bool status_output) { //NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty() && status_output) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (bst_conf.valid()) {
        if (auto conf = bst_conf.get_configuration(); conf != nullptr) {
            try {
                auto state = status_check_internal(bst_conf);
                if (state == status_check_result::no_file ||
                    state == status_check_result::not_locked ||
                    state == status_check_result::deactivated) {
                    if (!FLAGS_quiet) {
                        std::cout << (force ? "kill " : "shutdown ") << "was not performed, as no " << server_name_string << " was running." << std::endl;
                    }
                    if (monitor_output) {
                        monitor_output->finish(true);
                    }
                    return tgctl::return_code::ok;
                }
                auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
                if (force) {
                    rtnv = tgctl_kill(file_mutex.get(), bst_conf);
                    if (rtnv == tgctl::return_code::ok) {
                        if (monitor_output) {
                            monitor_output->finish(true);
                        }
                        return rtnv;
                    }
                } else {
                    auto status_info = std::make_unique<server::status_info_bridge>(bst_conf.digest());
                    if (!status_info->is_shutdown_requested()) {
                        rtnv = tgctl_shutdown(file_mutex.get(), status_info.get());
                        if (rtnv == tgctl::return_code::ok) {
                            if (monitor_output) {
                                monitor_output->finish(true);
                            }
                            return rtnv;
                        }
                    } else {
                        if (!FLAGS_quiet) {
                            std::cout << "shutdown was not performed, as shutdown is already requested." << std::endl;
                        }
                        rtnv = tgctl::return_code::err;
                    }
                }
            } catch (std::runtime_error &e) {
                if (!FLAGS_quiet) {
                    if (std::string("the lock file does not exist") == e.what()) {
                        std::cout << (force ? "kill" : "shutdown") << " was not performed, as no " << server_name_string << " was running." << std::endl;
                    } else {
                        std::cout << (force ? "kill" : "shutdown") << " was not performed, as " << e.what() << "." << std::endl;
                    }
                }
                rtnv = tgctl::return_code::err;
            }
        } else {
            if (!FLAGS_quiet) {
                std::cout << (force ? "kill" : "shutdown") << " was not performed, as error in create_configuration." << std::endl;
            }
            rtnv = tgctl::return_code::err;
        }
    } else {
        if (!FLAGS_quiet) {
            std::cout << (force ? "kill" : "shutdown") << " was not performed, as any valid configuration file cannot be found." << std::endl;
        }
        rtnv = tgctl::return_code::err;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code tgctl_status() {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
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
        std::cout << server_name_string_for_status << " is SHUTTING_DOWN" << std::endl;
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
        rtnv = tgctl::return_code::err;
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
        rtnv = tgctl::return_code::err;
        break;
    case status_check_result::error_in_conf_file_name:
        std::cerr << "cannot find any valid configuration file" << std::endl;
        rtnv = tgctl::return_code::err;
        break;
    default:
        std::cerr << "should not reach here" << std::endl;
        rtnv = tgctl::return_code::err;
        break;
    }

    if (rtnv == tgctl::return_code::ok) {
        if (monitor_output) {
            monitor_output->finish(true);
        }
        return rtnv;
    }

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
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
    throw std::runtime_error("cannot find any valid configuration file");
}

tgctl::return_code tgctl_diagnostic() {
    try {
        kill(get_pid(), SIGHUP);
        return tgctl::return_code::ok;
    } catch (std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return tgctl::return_code::err;
    }
}

tgctl::return_code tgctl_pid() {
    try {
        std::cout << get_pid() << std::endl;
        return tgctl::return_code::ok;
    } catch (std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return tgctl::return_code::err;
    }
}

}  // tateyama::process
