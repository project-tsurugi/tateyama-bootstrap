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

DECLARE_string(conf);
DEFINE_bool(quiesce, false, "invoke in quiesce mode");  // NOLINT for quiesce
DECLARE_string(label);
DEFINE_bool(maintenance_server, false, "invoke in maintenance_server mode");  // NOLINT for oltp_start() invoked from start_maintenance_server()
DECLARE_string(monitor);  // NOLINT

namespace tateyama::bootstrap {

constexpr std::string_view server_name_string = "tateyama-server";
constexpr std::string_view server_name_string_for_status = "Tsurugi OLTP database";
const std::size_t shutdown_check_count = 50;

using namespace tateyama::bootstrap::utils;

static bool status_check(proc_mutex::lock_state state, const boost::filesystem::path& lock_file) {
    auto file_mutex = std::make_unique<proc_mutex>(lock_file, false);
    for (size_t i = 0; i < shutdown_check_count; i++) {
        usleep(100 * 1000);
        if (file_mutex->check() == state) {
            return true;
        }
    }
    return false;
}


int oltp_start([[maybe_unused]] int argc, char* argv[], char *argv0, bool need_check) {
    std::string server_name(server_name_string);
    if (auto pid = fork(); pid == 0) {
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
        DVLOG(log_trace) << "start " << server_name << ", pid = " << pid;
    }

    if (need_check) {
        // command arguments
        gflags::SetUsageMessage("tateyama database server CLI");
        gflags::ParseCommandLineFlags(&argc, &argv, true);

        auto bst_conf = utils::bootstrap_configuration(FLAGS_conf);
        if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
            usleep(100 * 1000);
            try {
                if (status_check(proc_mutex::lock_state::locked, bst_conf.lock_file())) {
                    return 0;
                }
                LOG(ERROR) << "cannot invoke a server process";
                return 1;
            } catch (std::runtime_error &e) {
                LOG(ERROR) << e.what();
                return 2;
            }
        }
        LOG(ERROR) << "error in create_configuration";
        return 3;
    }
    return 0;
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
                if (pid != 0) {
                    DVLOG(log_trace) << "kill (SIGKILL) to process " << pid << " and remove " << file_mutex->name();
                    kill(pid, SIGKILL);
                    usleep(100 * 1000);
                    if (monitor_output) {
                        monitor_output->finish(true);
                    }
                    return rc;
                }
                LOG(ERROR) << "contents of the file (" << file_mutex->name() << ") cannot be used";
                rc = tateyama::bootstrap::return_code::err;
            } else {
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
            std::cout << server_name_string_for_status << " is inactive" << std::endl;
            break;
        case state::locked:
            if (monitor_output) {
                monitor_output->status(tateyama::bootstrap::utils::status::running);
                break;
            }
            std::cout << server_name_string_for_status << " is running" << std::endl;
            break;
        case state::not_locked:  // this is an error that makes it difficult to understand the situation.
        {
            if (monitor_output) {
                monitor_output->status(tateyama::bootstrap::utils::status::disconnected);
                break;
            }
            std::stringstream ss;
            FILE *fp;
            ss << "ps -ef | grep " << server_name_string << " | grep " << file_mutex->pid(false) << " | grep -v grep | wc -l";
            if(fp = popen(ss.str().c_str(), "r"); fp == nullptr){
                std::cout << "the service is unknown" << std::endl;
                rc = tateyama::bootstrap::return_code::err;
            } else {
                int l;
                if (fscanf(fp, "%d", &l) != 1) {
                    std::cout << "the service is unknown" << std::endl;
                    rc = tateyama::bootstrap::return_code::err;
                } else {
                    std::cout << server_name_string_for_status <<
                        ((l == 0) ? " is booting up" : " is shutting down") << std::endl;
                }
            }
            break;
        }
        default:
            LOG(ERROR) << "error in proc_mutex check";
            rc = tateyama::bootstrap::return_code::err;
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
