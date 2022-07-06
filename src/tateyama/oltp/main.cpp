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

DEFINE_string(conf, "", "the file name of the configuration");  // NOLINT

namespace tateyama::bootstrap {

int oltp_main(int argc, char* argv[]) {
    google::InitGoogleLogging(argv[0]);

    if (strcmp(*(argv + 1), "start") == 0) {
        return oltp_start(argc - 1, argv + 1, argv[0], true);
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
        start_maintenance_server(argc - 4, argv + 4, argv[0]);

        int rv{};
        if (strcmp(*(argv + 2), "backup") == 0) {
            rv = backup::oltp_restore_backup(argc - 3, argv + 3);
        } else if (strcmp(*(argv + 2), "tag") == 0) {
            rv = backup::oltp_restore_tag(argc - 3, argv + 3);
        } else {
            LOG(ERROR) << "unknown backup subcommand '" << *(argv + 2) << "'";
            rv = -1;
        }

        oltp_shutdown_kill(argc - 4, argv + 4, false);
        return rv;
    }
    if (strcmp(*(argv + 1), "quiesce") == 0) {
        argv[1] = const_cast<char*>("--quiesce");
        return oltp_start(argc, argv, argv[0], true);
    }
    LOG(ERROR) << "unknown command '" << *(argv + 1) << "'";
    return -1;
}

}  // tateyama::bootstrap


int main(int argc, char* argv[]) {
    if (argc > 1) {
        return tateyama::bootstrap::oltp_main(argc, argv);
    }
    return -1;
}
