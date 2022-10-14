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
#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "oltp.h"

// common
DEFINE_string(conf, "", "the file name of the configuration");  // NOLINT
DEFINE_string(monitor, "", "the file name to which monitoring info. is to be output");  // NOLINT
DEFINE_string(label, "", "label for this operation");  // NOLINT

// for control
DEFINE_bool(quiesce, false, "invoke in quiesce mode");  // NOLINT for quiesce
DEFINE_bool(maintenance_server, false, "invoke in maintenance_server mode");  // NOLINT for oltp_start() invoked from start_maintenance_server()
DEFINE_string(start_mode, "", "start mode, only force is valid");  // NOLINT for oltp_start()

// for backup
DEFINE_bool(force, false, "no confirmation step");  // NOLINT
DEFINE_bool(keep_backup, true, "backup files will be kept");  // NOLINT

// for tateyama-server
DEFINE_string(location, "./db", "database location on file system");  // NOLINT
DEFINE_bool(load, false, "Database contents are loaded from the location just after boot");  // NOLINT
DEFINE_bool(tpch, false, "Database will be set up for tpc-h benchmark");  // NOLINT


namespace tateyama::bootstrap {

int oltp_main(const std::vector<std::string>& args) {
    if (args.at(1) == "start") {
        return oltp_start(args.at(0), true);
    }
    if (args.at(1) == "shutdown") {
        return oltp_shutdown_kill(false);
    }
    if (args.at(1) == "kill") {
        return oltp_shutdown_kill(true);
    }
    if (args.at(1) == "status") {
        return oltp_status();
    }
    if (args.at(1) == "backup") {
        if (args.at(2) == "create") {
            if (args.size() < 4) {
                LOG(ERROR) << "need to specify path/to/backup";
                return 4;
            }
            return backup::oltp_backup_create(args.at(3));
        }
        if (args.at(2) == "estimate") {
            return backup::oltp_backup_estimate();
        }
        LOG(ERROR) << "unknown backup subcommand '" << args.at(2) << "'";
        return -1;
    }
    if (args.at(1) == "restore") {
        oltp_start(args.at(0), true, tateyama::framework::boot_mode::maintenance_server);

        int rv{};
        if (args.at(2) == "backup") {
            rv = backup::oltp_restore_backup(args.at(3));
        } else if (args.at(2) == "tag") {
            rv = backup::oltp_restore_tag(args.at(3));
        } else {
            LOG(ERROR) << "unknown backup subcommand '" << args.at(2) << "'";
            rv = -1;
        }

        oltp_shutdown_kill(false, false);
        return rv;
    }
    if (args.at(1) == "quiesce") {
        return oltp_start(args.at(0), true, tateyama::framework::boot_mode::quiescent_server);
    }
    LOG(ERROR) << "unknown command '" << args.at(1) << "'";
    return -1;
}

}  // tateyama::bootstrap


int main(int argc, char* argv[]) {
    if (argc > 1) {
        // logging
        google::InitGoogleLogging(argv[0]);  // NOLINT

        // copy argv to args
        std::vector<std::string> args(argv, argv + argc);

        // command arguments (must conduct after the argment copy)
        gflags::SetUsageMessage("tateyama database server CLI");
        gflags::ParseCommandLineFlags(&argc, &argv, false);

        return static_cast<int>(tateyama::bootstrap::oltp_main(args));
    }
    LOG(ERROR) << "no arguments";
    return -1;
}
