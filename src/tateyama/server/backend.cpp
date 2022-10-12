/*
 * Copyright 2019-2022 tsurugi project.
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
#include <memory>
#include <string>
#include <exception>
#include <iostream>
#include <chrono>
#include <csignal>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/framework/server.h>
#include <tateyama/status/resource/bridge.h>

#include <jogasaki/api/service/bridge.h>
#include <jogasaki/api/resource/bridge.h>

#ifdef OGAWAYAMA
#include <ogawayama/bridge/service.h>
#endif
#include <jogasaki/api.h>

#include "server.h"
#include "utils.h"
#include "proc_mutex.h"
#include "configuration.h"
#include "monitor.h"

DEFINE_string(conf, "", "the configuration file");  // NOLINT
DEFINE_string(location, "./db", "database location on file system");  // NOLINT
DEFINE_bool(load, false, "Database contents are loaded from the location just after boot");  // NOLINT
DEFINE_bool(tpch, false, "Database will be set up for tpc-h benchmark");  // NOLINT
DEFINE_bool(maintenance_server, false, "invoke in maintenance_server mode");  // NOLINT
DEFINE_bool(maintenance_standalone, false, "invoke in maintenance_standalone mode");  // NOLINT  not used here
DEFINE_bool(quiesce, false, "invoke in quiesce mode");  // NOLINT
DEFINE_string(label, "", "message used in quiesce mode");  // NOLINT  dummy
DEFINE_string(monitor, "", "the file name to which monitoring info. is to be output");  // NOLINT  dummy
DEFINE_bool(force, false, "an option for oltp, do not use here");  // NOLINT  dummy
DEFINE_bool(keep_backup, false, "an option for oltp, do not use here");  // NOLINT  dummy
DEFINE_string(start_mode, "", "start mode, only force is valid");  // NOLINT  dummy

namespace tateyama::bootstrap {

using namespace tateyama::bootstrap::utils;

// should be in sync one in ipc_provider/steram_provider
struct endpoint_context {
    std::unordered_map<std::string, std::string> options_{};
};

int backend_main(int argc, char **argv) {
    google::InitGoogleLogging("tateyama_database_server");

    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // configuration
    auto bst_conf = bootstrap_configuration(FLAGS_conf);
    auto conf = bst_conf.create_configuration();
    if (conf == nullptr) {
        LOG(ERROR) << "error in create_configuration";
        exit(1);
    }

    // mutex
    auto mutex = std::make_unique<proc_mutex>(bst_conf.lock_file());
    if (!mutex->lock()) {
        exit(1);
    }

    bool tpch_mode = false;
    bool tpcc_mode = true;
    if (FLAGS_tpch) {
        tpch_mode = true;
        tpcc_mode = false;
    }

    framework::boot_mode mode;
    if (FLAGS_maintenance_server) {
        mode = framework::boot_mode::maintenance_server;
        FLAGS_load = false;
    } else if (FLAGS_quiesce) {
        mode = framework::boot_mode::quiescent_server;
        FLAGS_load = false;
    } else {
        mode = framework::boot_mode::database_server;
    }
    framework::server sv{mode, conf};
    framework::add_core_components(sv);
    sv.add_resource(std::make_shared<jogasaki::api::resource::bridge>());
    auto sqlsvc = std::make_shared<jogasaki::api::service::bridge>();
    sv.add_service(sqlsvc);

#ifdef OGAWAYAMA
    // ogawayama bridge
    sv.add_service(std::make_shared<ogawayama::bridge::service>());
    LOG(INFO) << "ogawayama bridge created";
#endif

    // status_info
    auto status_info = sv.find_resource<tateyama::status_info::resource::bridge>();

    sv.setup();
    auto* db = sqlsvc->database();
    if (tpcc_mode) {
        db->config()->prepare_benchmark_tables(true);
    }
    if (tpch_mode) {
        db->config()->prepare_analytics_benchmark_tables(true);
    }
    status_info->whole(tateyama::status_info::state::ready);

    sv.start();
    status_info->whole(tateyama::status_info::state::activated);
    LOG(INFO) << "database started";

    if (FLAGS_load) {
        if (tpcc_mode) {
            // load tpc-c tables
            LOG(INFO) << "TPC-C data load begin";
            try {
                jogasaki::common_cli::load(*db, FLAGS_location);
            } catch (std::exception& e) {
                LOG(ERROR) << " [" << __FILE__ << ":" <<  __LINE__ << "] " << e.what();
                std::abort();
            }
            LOG(INFO) << "TPC-C data load end";
        }
        if (tpch_mode) {
            // load tpc-h tables
            LOG(INFO) << "TPC-H data load begin";
            try {
                jogasaki::common_cli::load_tpch(*db, FLAGS_location);
            } catch (std::exception& e) {
                LOG(ERROR) << " [" << __FILE__ << ":" <<  __LINE__ << "] " << e.what();
                std::abort();
            }
            LOG(INFO) << "TPC-H data load end";
        }
    }

    // wait for signal to terminate this
    int signo{};
    sigset_t ss;
    sigemptyset(&ss);
    do {
        if (auto ret = sigaddset(&ss, SIGINT); ret != 0) {
            LOG(ERROR) << "fail to sigaddset";
        }
        if (auto ret = sigprocmask(SIG_BLOCK, &ss, nullptr); ret != 0) {
            LOG(ERROR) << "fail to pthread_sigmask";
        }
        if (auto ret = sigwait(&ss, &signo); ret == 0) { // シグナルを待つ
            if (signo == SIGINT) {
                // termination process
                LOG(INFO) << "exiting";
                status_info->whole(tateyama::status_info::state::deactivating);
                sv.shutdown();
                status_info->whole(tateyama::status_info::state::deactivated);
                return 0;
            }
        } else {
            LOG(ERROR) << "fail to sigwait";
            return -1;
        }
    } while(true);
}

}  // tateyama::bootstrap


int main(int argc, char **argv) {
    return tateyama::bootstrap::backend_main(argc, argv);
}
