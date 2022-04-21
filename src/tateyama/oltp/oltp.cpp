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

#include <unistd.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <tateyama/api/configuration.h>
#include <tateyama/util/proc_mutex.h>
#include <tateyama/logging.h>

DEFINE_string(conf, "", "the directory where the configuration file is");  // NOLINT

namespace tateyama::oltp {

const std::string server_name = "tateyama-server";

static int oltp_start([[maybe_unused]] int argc, char* argv[]) {
    if (auto pid = fork(); pid == 0) {
        auto cmd = server_name.c_str();
        *argv = const_cast<char *>(cmd);
        execvp(cmd, argv);
        perror("execvp");
    } else {
        VLOG(log_trace) << "start " << server_name << ", pid = " << pid;
    }
    return 0;
}

static int oltp_shutdown_kill(int argc, char* argv[], bool force) {
    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    if (auto conf = tateyama::api::configuration::create_configuration(FLAGS_conf); conf != nullptr) {
        boost::filesystem::path path = conf->get_directory() / tateyama::server::LOCK_FILE_NAME;
        if (boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)) {
            std::size_t sz = static_cast<std::size_t>(boost::filesystem::file_size(path));
            std::string str{};
            str.resize(sz, '\0');

            boost::filesystem::ifstream file(path);
            file.read(&str[0], sz);
            if (force) {
                VLOG(log_trace) << "kill (SIGKILL) to process " << str << " and remove " << path;
                kill(stoi(str), SIGKILL);
                boost::filesystem::remove(path);
            } else {
                VLOG(log_trace) << "kill (SIGINT) to process " << str;
                kill(stoi(str), SIGINT);
            }
            return 0;
        } else {
            LOG(ERROR) << path << " (LOCK FILE) does not exist, means no " << server_name << " is running";
            return 1;
        }
    } else {
        LOG(ERROR) << "error in create_configuration";
        return 2;
    }
}

int oltp_main(int argc, char* argv[]) {
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
        LOG(INFO) << "'status' has not been implemented";
        return 0;
    }
    LOG(ERROR) << "unknown command '" << *(argv + 1) << "'";
    return -1;
}

}  // tateyama::oltp


int main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);
    if (argc > 1) {
        return tateyama::oltp::oltp_main(argc, argv);
    }
    return -1;
}
