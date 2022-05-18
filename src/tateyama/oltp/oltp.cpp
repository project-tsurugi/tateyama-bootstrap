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
#include <string>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

#include <tateyama/logging.h>

#include "configuration.h"
#include "proc_mutex.h"
#include "oltp.h"

DEFINE_string(conf, "", "the file name of the configuration");  // NOLINT

namespace tateyama::bootstrap {

using namespace tateyama::bootstrap::utils;

const std::string server_name = "tateyama-server";

static int oltp_start([[maybe_unused]] int argc, char* argv[]) {
    if (auto pid = fork(); pid == 0) {
        argv[0] = const_cast<char *>(server_name.c_str());
        auto base = boost::filesystem::canonical(boost::filesystem::path(getenv("_"))).parent_path().parent_path();
        execv((base / boost::filesystem::path("libexec") / boost::filesystem::path(server_name)).generic_string().c_str(), argv);
        perror("execvp");
    } else {
        VLOG(log_trace) << "start " << server_name << ", pid = " << pid;
    }
    usleep(100 * 1000);
    return 0;
}

static int oltp_shutdown_kill(int argc, char* argv[], bool force) {
    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (auto conf = utils::bootstrap_configuration(FLAGS_conf).create_configuration(); conf != nullptr) {
        auto file_mutex = std::make_unique<proc_mutex>(conf->get_directory(), false);
        std::string str{};
        if (file_mutex->contents(str)) {
            if (force) {
                VLOG(log_trace) << "kill (SIGKILL) to process " << str << " and remove " << file_mutex->name();
                kill(stoi(str), SIGKILL);
                unlink(file_mutex->name().c_str());
            } else {
                VLOG(log_trace) << "kill (SIGINT) to process " << str;
                kill(stoi(str), SIGINT);
            }
            usleep(100 * 1000);
            return 0;
        } else {
            LOG(ERROR) << "contents of the file (" << file_mutex->name() << ") cannot be used";
            return 1;
        }
    } else {
        LOG(ERROR) << "error in create_configuration";
        return 2;
    }
}

static int oltp_status(int argc, char* argv[]) {
    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (auto conf = bootstrap_configuration(FLAGS_conf).create_configuration(); conf != nullptr) {
        auto directory = conf->get_directory();
        auto file_mutex = std::make_unique<proc_mutex>(directory, false);
        using state = proc_mutex::lock_state;
        switch (file_mutex->check()) {
        case state::no_file:
            std::cout << "no " << server_name << " is running on " << directory.string() << std::endl;
            break;
        case state::not_locked:
            std::cout << "not_locked, may be  intermediate state (in the middle of running or stopping)" << std::endl;
            break;
        case state::locked:
            std::cout << "a " << server_name << " is running on " << directory.string() << std::endl;
            break;
        default:
            std::cout << "error occured" << std::endl;
        }
    }
    return 0;
}

int oltp_main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    if (strcmp(*(argv + 1), "start") == 0) {
        return oltp_start(argc - 1, argv + 1);
    }
    if (strcmp(*(argv + 1), "shutdown") == 0) {
        return oltp_shutdown_kill(argc - 1, argv + 1, false);
    }
    if (strcmp(*(argv + 1), "kill") == 0) {
        return oltp_shutdown_kill(argc - 1, argv + 1, true);
    }
    if (strcmp(*(argv + 1), "status") == 0) {
        return oltp_status(argc - 1, argv + 1);
    }
    if (strcmp(*(argv + 1), "backup") == 0) {
        if (strcmp(*(argv + 2), "create") == 0) {
            return backup::oltp_backup_create(argc - 2, argv + 2);
        }
        if (strcmp(*(argv + 2), "estimate") == 0) {
            return backup::oltp_backup_estimate(argc - 2, argv + 2);
        }
        LOG(ERROR) << "unknown backup subcommand '" << *(argv + 2) << "'";
        return -1;
    }
    if (strcmp(*(argv + 1), "restore") == 0) {
        if (strcmp(*(argv + 2), "backup") == 0) {
            return backup::oltp_restore_backup(argc - 2, argv + 2);
        }
        if (strcmp(*(argv + 2), "tag") == 0) {
            return backup::oltp_restore_tag(argc - 2, argv + 2);
        }
        LOG(ERROR) << "unknown backup subcommand '" << *(argv + 2) << "'";
        return -1;
    }
    LOG(ERROR) << "unknown command '" << *(argv + 1) << "'";
    return -1;
}

}  // tateyama::bootstrap
