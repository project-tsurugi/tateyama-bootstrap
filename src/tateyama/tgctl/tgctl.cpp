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
#include <string>
#include <exception>

#include <gflags/gflags.h>

#include "tateyama/process/process.h"
#include "tateyama/datastore/backup.h"
#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/version/version.h"
#include "tateyama/session/session.h"
#include "tateyama/metrics/metrics.h"
#ifdef ENABLE_ALTIMETER
#include "tateyama/altimeter/altimeter.h"
#endif
#include "tateyama/request/request.h"

#include "help_text.h"

// help
DECLARE_bool(help);

// common
DECLARE_string(monitor);
DECLARE_int32(timeout);

// control
DECLARE_bool(q);
DECLARE_bool(quiet);

// backup
DECLARE_string(use_file_list);

namespace tateyama::tgctl {

int tgctl_main(const std::vector<std::string>& args) { //NOLINT(readability-function-cognitive-complexity)
    FLAGS_quiet |= FLAGS_q;

    if (args.size() < 2) {
        std::cerr << "no subcommand" << std::endl;
        return tateyama::tgctl::return_code::err;
    }
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
        if (args.size() < 3) {
            std::cerr << "need to specify backup subcommand" << std::endl;
            return tateyama::tgctl::return_code::err;
        }
        if (args.at(2) == "create") {
            if (args.size() < 4) {
                std::cerr << "need to specify path/to/backup" << std::endl;
                return tateyama::tgctl::return_code::err;
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
        return tateyama::tgctl::return_code::err;
    }
    if (args.at(1) == "restore") {
        if (FLAGS_timeout != -1) {
            std::cerr << "timeout option cannot be specified to restore subcommand" << std::endl;
        }
        FLAGS_timeout = 0;  // no timeout for 'tgctl restore xxx'
        FLAGS_quiet = true;
        int rtnv{};
        if (tateyama::process::tgctl_start(args.at(0), true, tateyama::framework::boot_mode::maintenance_server) == tgctl::return_code::ok) {
            if (args.size() < 3) {
                std::cerr << "need to specify restore subcommand" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
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
    if (args.at(1) == "session") {
        if (args.size() < 3) {
            std::cerr << "need to specify session subcommand" << std::endl;
            return tateyama::tgctl::return_code::err;
        }
        if (args.at(2) == "list") {
            return tateyama::session::session_list();
        }
        if (args.at(2) == "show") {
            if (args.size() < 4) {
                std::cerr << "need to specify session-ref" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            return tateyama::session::session_show(args.at(3));
        }
        if (args.at(2) == "shutdown") {
            if (args.size() < 4) {
                std::cerr << "need to specify session-ref(s)" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            return tateyama::session::session_shutdown(args.at(3));
        }
        if (args.at(2) == "set") {
            if (args.size() < 5) {
                std::cerr << "need to specify session-ref and set-key" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            if (args.size() < 6) {
                return tateyama::session::session_swtch(args.at(3), args.at(4));
            }
            return tateyama::session::session_swtch(args.at(3), args.at(4), args.at(5), true);
        }
        std::cerr << "unknown session sub command '" << args.at(2) << "'" << std::endl;
        return tateyama::tgctl::return_code::err;
    }
    if (args.at(1) == "dbstats") {
        if (args.size() < 3) {
            std::cerr << "need to specify dbstats subcommand" << std::endl;
            return tateyama::tgctl::return_code::err;
        }
        if (args.at(2) == "list") {
            return tateyama::metrics::list();
        }
        if (args.at(2) == "show") {
            return tateyama::metrics::show();
        }
        std::cerr << "unknown dbstats-sub command '" << args.at(2) << "'" << std::endl;
        return tateyama::tgctl::return_code::err;
    }
#ifdef ENABLE_ALTIMETER
    if (args.at(1) == "altimeter") {
        if (args.size() < 3) {
            std::cerr << "need to specify altimeter subcommand" << std::endl;
            return tateyama::tgctl::return_code::err;
        }
        if (args.at(2) == "enable") {
            if (args.size() < 4) {
                std::cerr << "need to specify log type for altimeter enable" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            return tateyama::altimeter::set_enabled(args.at(3), true);
        }
        if (args.at(2) == "disable") {
            if (args.size() < 4) {
                std::cerr << "need to specify log type for altimeter disable" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            return tateyama::altimeter::set_enabled(args.at(3), false);
        }
        if (args.at(2) == "set") {
            if (args.size() < 4) {
                std::cerr << "need to specify parameter for altimeter set" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            if (args.at(3) == "event_level") {
                if (args.size() < 5) {
                    std::cerr << "need to specify parameter for altimeter set event_level" << std::endl;
                    return tateyama::tgctl::return_code::err;
                }
                return tateyama::altimeter::set_log_level("event", args.at(4));
            }
            if (args.at(3) == "audit_level") {
                if (args.size() < 5) {
                    std::cerr << "need to specify parameter for altimeter set audit_level" << std::endl;
                    return tateyama::tgctl::return_code::err;
                }
                return tateyama::altimeter::set_log_level("audit", args.at(4));
            }
            if (args.at(3) == "statement_duration") {
                if (args.size() < 5) {
                    std::cerr << "need to specify parameter for altimeter set statement_duration" << std::endl;
                    return tateyama::tgctl::return_code::err;
                }
                return tateyama::altimeter::set_statement_duration(args.at(4));
            }
        }
        if (args.at(2) == "rotate") {
            if (args.size() < 4) {
                std::cerr << "need to specify parameter for altimeter rotate" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            return tateyama::altimeter::rotate(args.at(3));
        }
        std::cerr << "unknown altimeter-sub command '" << args.at(2) << "'" << std::endl;
        return tateyama::tgctl::return_code::err;
    }
#endif

    if (args.at(1) == "request") {
        if (args.size() < 3) {
            std::cerr << "need to specify request subcommand" << std::endl;
            return tateyama::tgctl::return_code::err;
        }
        if (args.at(2) == "list") {
            return tateyama::request::request_list();
        }
        if (args.at(2) == "payload") {
            if (args.size() < 5) {
                std::cerr << "need to specify session-id and request-id" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            return tateyama::request::request_payload(std::stol(args.at(3)), std::stol(args.at(4)));
        }
        if (args.at(2) == "extract-sql") {
            if (args.size() < 5) {
                std::cerr << "need to specify session-id and payload" << std::endl;
                return tateyama::tgctl::return_code::err;
            }
            return tateyama::request::request_extract_sql(std::stol(args.at(3)), args.at(4));
        }
        std::cerr << "unknown request-sub command '" << args.at(2) << "'" << std::endl;
        return tateyama::tgctl::return_code::err;
    }

    std::cerr << "unknown command '" << args.at(1) << "'" << std::endl;
    return tateyama::tgctl::return_code::err;
}

}  // tateyama::tgctl


int main(int argc, char* argv[]) {
    if (argc > 1) {
        // command arguments (must conduct after the argment copy)
        gflags::SetUsageMessage("tateyama database server CLI");
        gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);

        if (FLAGS_help) {
            std::cout << tateyama::tgctl::help_text << std::flush;
            return 0;
        }

        // copy argv to args
        std::vector<std::string> args(argv, argv + argc);

        try {
            return static_cast<int>(tateyama::tgctl::tgctl_main(args));
        } catch (std::exception &e) {
            std::cerr << e.what() << std::endl;
        }
    }
    return -1;
}
