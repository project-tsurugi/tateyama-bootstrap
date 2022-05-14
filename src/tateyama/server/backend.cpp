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
#include <signal.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/framework/server.h>
#include <tateyama/api/server/service.h>
#include <tateyama/api/endpoint/service.h>
#include <tateyama/api/endpoint/provider.h>
#include <tateyama/api/registry.h>

#include <jogasaki/api/service/bridge.h>
#include <jogasaki/api/resource/bridge.h>

#ifdef OGAWAYAMA
#include <ogawayama/bridge/provider.h>
#endif
#include <jogasaki/api.h>

#include "server.h"
#include "utils.h"
#include "restore.h"
#include "proc_mutex.h"
#include "configuration.h"

DEFINE_string(conf, "", "the directory where the configuration file is");  // NOLINT
DEFINE_string(location, "./db", "database location on file system");  // NOLINT
DEFINE_bool(load, false, "Database contents are loaded from the location just after boot");  // NOLINT
DEFINE_bool(tpch, false, "Database will be set up for tpc-h benchmark");  // NOLINT
DEFINE_string(restore_backup, "", "path to back up directory where files used in the restore are located");  // NOLINT
DEFINE_bool(keep_backup, false, "back up file should be kept or not");  // NOLINT
DEFINE_string(restore_tag, "", "tag name specifying restore");  // NOLINT

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
    auto env = std::make_shared<tateyama::api::environment>();
    auto conf = bootstrap_configuration(FLAGS_conf).create_configuration();
    if (conf == nullptr) {
        LOG(ERROR) << "error in create_configuration";
        exit(1);
    }
    env->configuration(conf);

    // mutex
    auto mutex = std::make_unique<proc_mutex>(conf->get_directory());
    if (!mutex->lock()) {
        LOG(ERROR) << "another tateyama-server is running on " << conf->get_directory().string();
        exit(1);
    }

    bool tpch_mode = false;
    bool tpcc_mode = true;
    if (FLAGS_tpch) {
        tpch_mode = true;
        tpcc_mode = false;
    }

    framework::boot_mode mode = (!FLAGS_restore_backup.empty() || !FLAGS_restore_tag.empty()) ? framework::boot_mode::maintenance_standalone : framework::boot_mode::database_server;
    framework::server sv{mode, conf};
    framework::add_core_components(sv);
    sv.add_resource(std::make_shared<jogasaki::api::resource::bridge>());
    auto sqlsvc = std::make_shared<jogasaki::api::service::bridge>();
    sv.add_service(sqlsvc);
    sv.setup();
    auto* db = sqlsvc->database();
    if (tpcc_mode) {
        db->config()->prepare_benchmark_tables(true);
    }
    if (tpch_mode) {
        db->config()->prepare_analytics_benchmark_tables(true);
    }
    sv.start();

    // maintenance_standalone mode
    if (mode == framework::boot_mode::maintenance_standalone) {
        // backup, recovery
        if (!FLAGS_restore_backup.empty()) {
            tateyama::bootstrap::restore_backup(sv, FLAGS_restore_backup, FLAGS_keep_backup);
        }
        if (!FLAGS_restore_tag.empty()) {
            tateyama::bootstrap::restore_tag(sv, FLAGS_restore_tag);
        }
        sv.shutdown();
        return 0;
    }

    LOG(INFO) << "database started";

#ifdef OGAWAYAMA
    // ogawayama bridge
    ogawayama::bridge::api::prepare();
    auto bridge = tateyama::api::registry<ogawayama::bridge::api::provider>::create("ogawayama");
    if (bridge) {
        if (auto rc = bridge->initialize(*env, db, nullptr); rc != status::ok) {
            std::abort();
        }
        LOG(INFO) << "ogawayama bridge created";
    }
#endif
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

#ifdef OGAWAYAMA
    if (bridge) {
        if (auto rc = bridge->start(); rc != status::ok) {
            std::abort();
        }
        LOG(INFO) << "ogawayama bridge listener started";
    }
#endif

    // wait for signal to terminate this
    int signo;
    sigset_t ss;
    sigemptyset(&ss);
    do {
        if (auto ret = sigaddset(&ss, SIGINT); ret != 0) {
            LOG(ERROR) << "fail to sigaddset";
        }
        if (auto ret = sigprocmask(SIG_BLOCK, &ss, NULL); ret != 0) {
            LOG(ERROR) << "fail to pthread_sigmask";
        }
        if (auto ret = sigwait(&ss, &signo); ret == 0) { // シグナルを待つ
            switch(signo) {
            case SIGINT:
                // termination process
#ifdef OGAWAYAMA
                if (bridge) {
                    LOG(INFO) << "bridge->shutdown()";
                    bridge->shutdown();
                }
#endif
                LOG(INFO) << "exiting";
                sv.shutdown();
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
