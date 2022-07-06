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

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/process/search_path.hpp>

#include <tateyama/logging.h>

#include "configuration.h"
#include "proc_mutex.h"
#include "oltp.h"

DECLARE_string(conf);
DEFINE_bool(quiesce, false, "invoke in quiesce mode");  // NOLINT for quiesce
DEFINE_string(message, "", "message used in quiesce mode");  // NOLINT for quiesce
DEFINE_bool(maintenance_server, false, "invoke in maintenance_server mode");  // NOLINT for oltp_start() invoked from start_maintenance_server()

namespace tateyama::bootstrap {

using namespace tateyama::bootstrap::utils;

const std::string server_name = "tateyama-server";
const std::size_t shutdown_check_count = 50;

static bool status_check(proc_mutex::lock_state state, boost::filesystem::path lock_file) {
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
    } else {
        VLOG(log_trace) << "start " << server_name << ", pid = " << pid;
    }

    if (need_check) {
        // command arguments
        gflags::SetUsageMessage("tateyama database server CLI");
        gflags::ParseCommandLineFlags(&argc, &argv, true);

        auto bst_conf = utils::bootstrap_configuration(FLAGS_conf);
        if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
            usleep(100 * 1000);
            if (status_check(proc_mutex::lock_state::locked, bst_conf.lock_file())) {
                return 0;
            }
            LOG(ERROR) << "cannot invoke a server process";
            return 1;
        }
        LOG(ERROR) << "error in create_configuration";
        return 3;
    }

    return 0;
}

int oltp_shutdown_kill(int argc, char* argv[], bool force) {
    // command arguments
    gflags::SetUsageMessage("tateyama database server CLI");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    auto bst_conf = utils::bootstrap_configuration(FLAGS_conf);
    if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
        auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
        std::string str{};
        if (file_mutex->contents(str)) {
            if (force) {
                VLOG(log_trace) << "kill (SIGKILL) to process " << str << " and remove " << file_mutex->name();
                kill(stoi(str), SIGKILL);
                unlink(file_mutex->name().c_str());
                usleep(100 * 1000);
                return 0;
            } else {
                VLOG(log_trace) << "kill (SIGINT) to process " << str;
                kill(stoi(str), SIGINT);
                if (status_check(proc_mutex::lock_state::no_file, bst_conf.lock_file())) {
                    return 0;
                }
                LOG(ERROR) << "failed to shutdown, the server may still be alive";
                return 1;
            }
        } else {
            LOG(ERROR) << "contents of the file (" << file_mutex->name() << ") cannot be used";
            return 2;
        }
    }
    LOG(ERROR) << "error in create_configuration";
    return 3;
}

int oltp_status(int argc, char* argv[]) {
    // command arguments
    gflags::SetUsageMessage("tateyama database server CLI");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    auto bst_conf = utils::bootstrap_configuration(FLAGS_conf);
    if (auto conf = bst_conf.create_configuration(); conf != nullptr) {
        auto file_mutex = std::make_unique<proc_mutex>(bst_conf.lock_file(), false);
        using state = proc_mutex::lock_state;
        switch (file_mutex->check()) {
        case state::no_file:
            std::cout << "no " << server_name << " is running on " << bst_conf.lock_file().string() << std::endl;
            break;
        case state::not_locked:
            std::cout << "not_locked, may be  intermediate state (in the middle of running or stopping)" << std::endl;
            break;
        case state::locked:
            std::cout << "a " << server_name << " is running on " << bst_conf.lock_file().string() << std::endl;
            break;
        default:
            std::cout << "error occured" << std::endl;
        }
    }
    return 0;
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
