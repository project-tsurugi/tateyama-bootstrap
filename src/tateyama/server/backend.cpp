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
#include <tateyama/api/configuration.h>
#include <tateyama/util/proc_mutex.h>
#ifdef OGAWAYAMA
#include <ogawayama/bridge/provider.h>
#endif
#include <jogasaki/api.h>

#include "server.h"
#include "utils.h"
#include "restore.h"

DEFINE_string(conf, "", "the directory where the configuration file is");  // NOLINT
DEFINE_string(location, "./db", "database location on file system");  // NOLINT
DEFINE_bool(load, false, "Database contents are loaded from the location just after boot");  //NOLINT
DEFINE_bool(tpch, false, "Database will be set up for tpc-h benchmark");  //NOLINT
DEFINE_string(restore_backup, "", "path to back up directory where files used in the restore are located");  //NOLINT
DEFINE_bool(keep_backup, false, "back up file should be kept or not");  //NOLINT
DEFINE_string(restore_tag, "", "tag name specifying restore");  //NOLINT

namespace tateyama::server {

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
    auto conf = tateyama::api::configuration::create_configuration(FLAGS_conf);
    if (conf == nullptr) {
        LOG(ERROR) << "error in create_configuration";
        exit(1);
    }
    env->configuration(conf);

    // mutex
    auto mutex = std::make_unique<proc_mutex>(env->configuration()->get_directory());
    if (!mutex->lock()) {
        LOG(ERROR) << "another tateyama-server is running on " << env->configuration()->get_directory();
        exit(1);
    }

    framework::boot_mode mode = (!FLAGS_restore_backup.empty() || !FLAGS_restore_tag.empty()) ? framework::boot_mode::maintenance_standalone : framework::boot_mode::database_server;
    framework::server sv{mode, conf};
    framework::install_core_components(sv);
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

    bool tpch_mode = false;
    bool tpcc_mode = true;
    if (FLAGS_tpch) {
        tpch_mode = true;
        tpcc_mode = false;
    }

    // database
    auto jogasaki_config = env->configuration()->get_section("sql");
    if (jogasaki_config == nullptr) {
        LOG(ERROR) << "cannot find sql section in the configuration";
        exit(1);
    }

    auto cfg = std::make_shared<jogasaki::configuration>();
    if (tpcc_mode) {
        cfg->prepare_benchmark_tables(true);
    }
    if (tpch_mode) {
        cfg->prepare_analytics_benchmark_tables(true);
    }
    std::size_t thread_pool_size;
    if (jogasaki_config->get<>("thread_pool_size", thread_pool_size)) {
        cfg->thread_pool_size(thread_pool_size);
    } else {
        LOG(ERROR) << "cannot find thread_pool_size in the jogasaki section of the configuration";
        exit(1);
    }
    bool lazy_worker;
    if (jogasaki_config->get<>("lazy_worker", lazy_worker)) {
        cfg->lazy_worker(lazy_worker);
    } else {
        LOG(ERROR) << "cannot find lazy_worker in the jogasaki section of the configuration";
        exit(1);
    }

    // data_store
    auto data_store_config = env->configuration()->get_section("data_store");
    if (data_store_config == nullptr) {
        LOG(ERROR) << "cannot find data_store section in the configuration";
        exit(1);
    }
    std::string log_location;
    if (data_store_config->get<std::string>("log_location", log_location)) {
        cfg->db_location(log_location);
    }

    auto db = jogasaki::api::create_database(cfg);
    db->start();
    DBCloser dbcloser{db};
    LOG(INFO) << "database started";

    // service
    auto app = tateyama::api::registry<tateyama::api::server::service>::create("jogasaki");
    env->add_application(app);
    app->initialize(*env, db.get());

#ifdef OGAWAYAMA
    // ogawayama bridge
    ogawayama::bridge::api::prepare();
    auto bridge = tateyama::api::registry<ogawayama::bridge::api::provider>::create("ogawayama");
    if (bridge) {
        if (auto rc = bridge->initialize(*env, db.get(), nullptr); rc != status::ok) {
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
                LOG(INFO) << "app->shutdown()";
                app->shutdown();
                LOG(INFO) << "db->stop()";
                db->stop();
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

}  // tateyama::server


int main(int argc, char **argv) {
    return tateyama::server::backend_main(argc, argv);
}
