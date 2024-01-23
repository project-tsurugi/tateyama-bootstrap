/*
 * Copyright 2022-2023 Project Tsurugi.
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
#include <exception>

#include <gflags/gflags.h>

#include "tateyama/process/process.h"
#include "tateyama/datastore/backup.h"
#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/version/version.h"

// common
DEFINE_string(conf, "", "the file name of the configuration");  // NOLINT
DEFINE_string(monitor, "", "the file name to which monitoring info. is to be output");  // NOLINT
DEFINE_string(label, "", "label for this operation");  // NOLINT

// for control
DEFINE_bool(quiesce, false, "invoke in quiesce mode");  // NOLINT for quiesce
DEFINE_bool(maintenance_server, false, "invoke in maintenance_server mode");  // NOLINT for tgctl_start() invoked from start_maintenance_server()
DEFINE_string(start_mode, "", "start mode, only force is valid");  // NOLINT for tgctl_start()
DEFINE_int32(timeout, -1, "timeout for tgctl shutdown, no timeout control takes place if 0 is specified");  // NOLINT for tgctl_start()
DEFINE_bool(q, false, "do not display command execution results on the console");  // NOLINT
DEFINE_bool(quiet, false, "do not display command execution results on the console");  // NOLINT

// for backup
DEFINE_bool(force, false, "no confirmation step");  // NOLINT
DEFINE_bool(keep_backup, true, "backup files will be kept");  // NOLINT
DEFINE_string(use_file_list, "", "json file describing the individual files to be specified for restore");  // NOLINT

// for tsurugidb
DEFINE_string(location, "./db", "database location on file system");  // NOLINT
DEFINE_bool(load, false, "Database contents are loaded from the location just after boot");  // NOLINT
DEFINE_bool(tpch, false, "Database will be set up for tpc-h benchmark");  // NOLINT


namespace tateyama::tgctl {

int tgctl_main(const std::vector<std::string>& args) { //NOLINT(readability-function-cognitive-complexity)
    FLAGS_quiet |= FLAGS_q;

    if (args.at(1) == "start") {
        return tateyama::process::tgctl_start(args.at(0), true);
    }
    if (args.at(1) == "shutdown") {
        return tateyama::process::tgctl_shutdown_kill(false);
    }
    if (args.at(1) == "kill") {
        return tateyama::process::tgctl_shutdown_kill(true);
    }
    if (args.at(1) == "status") {
        return tateyama::process::tgctl_status();
    }
    if (args.at(1) == "diagnostic") {
        return tateyama::process::tgctl_diagnostic();
    }
    if (args.at(1) == "pid") {
        return tateyama::process::tgctl_pid();
    }
    if (args.at(1) == "backup") {
        if (args.at(2) == "create") {
            if (args.size() < 4) {
                std::cerr << "need to specify path/to/backup" << std::endl;
                return 4;
            }
            bool is_running = tateyama::process::is_running();
            if (!is_running) {
                auto FLAGS_quiet_previous = FLAGS_quiet;
                auto FLAGS_monitor_previous = FLAGS_monitor;
                FLAGS_quiet = true;
                FLAGS_monitor = "";
                if (tateyama::process::tgctl_start(args.at(0), true, tateyama::framework::boot_mode::maintenance_server) != tgctl::return_code::ok) {
                    if (!tateyama::process::is_running()) {
                        LOG_LP(ERROR) << "failed to start tsurugidb in maintenance_server mode";
                        return tgctl::return_code::err;
                    }
                    is_running = true;  // somebody else has invoked tsurugidb between confirmation and activation!!
                }
                FLAGS_quiet = FLAGS_quiet_previous;
                FLAGS_monitor = FLAGS_monitor_previous;
            }
            auto rv = tateyama::datastore::tgctl_backup_create(args.at(3));
            if (!is_running) {
                FLAGS_quiet = true;
                FLAGS_monitor = "";
                if (tateyama::process::tgctl_shutdown_kill(false) != tgctl::return_code::ok) {
                    LOG_LP(ERROR) << "failed to shutdown tsurugidb in maintenance_server mode, thus kill the tsurugidb";
                    tateyama::process::tgctl_shutdown_kill(true);
                }
            }
            return rv;
        }
        if (args.at(2) == "estimate") {
            return tateyama::datastore::tgctl_backup_estimate();
        }
        std::cerr << "unknown backup subcommand '" << args.at(2) << "'" << std::endl;
        return -1;
    }
    if (args.at(1) == "restore") {
        if (FLAGS_timeout != -1) {
            std::cerr << "timeout option cannot be specified to restore subcommand" << std::endl;
        }
        FLAGS_timeout = 0;  // no timeout for 'tgctl restore xxx'
        FLAGS_quiet = true;
        int rtnv{};
        if (tateyama::process::tgctl_start(args.at(0), true, tateyama::framework::boot_mode::maintenance_server) == tgctl::return_code::ok) {
            if (args.at(2) == "backup") {
                if (args.size() > 3) {
                    const auto& arg = args.at(3);
                    if (!FLAGS_use_file_list.empty()) {
                        rtnv = tateyama::datastore::tgctl_restore_backup_use_file_list(arg);
                    } else {
                        rtnv = tateyama::datastore::tgctl_restore_backup(args.at(3));
                    }
                } else {
                    std::cerr << "directory is not specficed" << std::endl;
                }
            } else if (args.at(2) == "tag") {
                if (args.size() > 3) {
                    rtnv = tateyama::datastore::tgctl_restore_tag(args.at(3));
                } else {
                    std::cerr << "tag is not specficed" << std::endl;
                }
            } else {
                std::cerr << "unknown backup subcommand '" << args.at(2) << "'" << std::endl;
                rtnv = -1;
            }

            tateyama::process::tgctl_shutdown_kill(false, false);
        } else {
            std::cerr << "failed to boot tsurugidb in maintenance_server mode" << std::endl;
            rtnv = -1;
        }
        return rtnv;
    }
    if (args.at(1) == "quiesce") {
        return tateyama::process::tgctl_start(args.at(0), true, tateyama::framework::boot_mode::quiescent_server);
    }
    if (args.at(1) == "version") {
        return tateyama::version::show_version(args.at(0));
    }
    std::cerr << "unknown command '" << args.at(1) << "'" << std::endl;
    return -1;
}

}  // tateyama::tgctl


int main(int argc, char* argv[]) {
    if (argc > 1) {
        // copy argv to args
        std::vector<std::string> args(argv, argv + argc);

        // command arguments (must conduct after the argment copy)
        gflags::SetUsageMessage("tateyama database server CLI");
        gflags::ParseCommandLineFlags(&argc, &argv, false);

        try {
            return static_cast<int>(tateyama::tgctl::tgctl_main(args));
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
        }
    }
    std::cerr << "no arguments" << std::endl;
    return -1;
}
