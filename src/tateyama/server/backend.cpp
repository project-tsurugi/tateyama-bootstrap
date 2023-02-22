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
#include <boost/property_tree/json_parser.hpp>  // for printing out the configuration

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <tateyama/framework/server.h>
#include <tateyama/status/resource/bridge.h>
#include <tateyama/diagnostic/resource/diagnostic_resource.h>

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

// for diagnostic resource
static std::shared_ptr<tateyama::diagnostic::resource::diagnostic_resource> diagnostic_resource_body{};  // NOLINT
static void sighup_handler(int) {
    if (diagnostic_resource_body) {
        diagnostic_resource_body->print_diagnostics(std::cerr);
    }
}

int backend_main(int argc, char **argv) {
    google::InitGoogleLogging("tateyama_database_server");
    google::InstallFailureSignalHandler();

    // command arguments
    gflags::SetUsageMessage("tateyama database server");
    gflags::ParseCommandLineFlags(&argc, &argv, true);

    // configuration
    auto bst_conf = utils::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (!bst_conf.valid()) {
        LOG(ERROR) << "error in create_configuration";
        exit(1);
    }
    auto conf = bst_conf.create_configuration();
    if (conf == nullptr) {
        LOG(ERROR) << "error in create_configuration";
        exit(1);
    }
    try {
        std::ostringstream oss;
        boost::property_tree::json_parser::write_json(oss, conf->get_ptree());
        LOG(INFO) << "==== configuration begin ====";
        LOG(INFO) << oss.str();
        LOG(INFO) << "==== configuration end ====";
    } catch (boost::property_tree::json_parser_error& e) {
        LOG(ERROR) << e.what();
    }

    // mutex
    auto mutex_file = bst_conf.lock_file();
    auto mutex = std::make_unique<proc_mutex>(mutex_file);
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

    if (!sv.setup()) {
        // detailed message must have been logged in the components where setup error occurs
        LOG(ERROR) << "Starting server failed due to errors in setting up server application framework.";
        exit(1);
    }
    // should do after setup()
    status_info->mutex_file(mutex_file.string());

    auto* db = sqlsvc->database();
    if (tpcc_mode) {
        db->config()->prepare_benchmark_tables(true);
    }
    if (tpch_mode) {
        db->config()->prepare_analytics_benchmark_tables(true);
    }
    status_info->whole(tateyama::status_info::state::ready);

    if (!sv.start()) {
        // detailed message must have been logged in the components where start error occurs
        LOG(ERROR) << "Starting server failed due to errors in starting server application framework.";
        exit(1);
    }

    // diagnostic
    diagnostic_resource_body = sv.find_resource<tateyama::diagnostic::resource::diagnostic_resource>();
    diagnostic_resource_body->add_print_callback("sharksfin", sharksfin::print_diagnostics);
    if (signal(SIGHUP, sighup_handler) == SIG_ERR) {  // NOLINT  #define SIG_ERR  ((__sighandler_t) -1) in a system header file
        LOG(ERROR) << "cannot register signal handler";
    }

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

    status_info->whole(tateyama::status_info::state::activated);
    LOG(INFO) << "database started";

    // wait for a shutdown request
    status_info->wait_for_shutdown();
    // termination process
    LOG(INFO) << "exiting";
    status_info->whole(tateyama::status_info::state::deactivating);
    sv.shutdown();
    status_info->whole(tateyama::status_info::state::deactivated);
    return 0;
}

}  // tateyama::bootstrap


int main(int argc, char **argv) {
    return tateyama::bootstrap::backend_main(argc, argv);
}
