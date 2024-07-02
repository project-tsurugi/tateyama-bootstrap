/*
 * Copyright 2018-2024 Project Tsurugi.
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
#include <filesystem>

#include <glog/logging.h>

#include <tateyama/api/configuration.h>

#include <altimeter/configuration.h>
#include <altimeter/log_item.h>
#include <altimeter/logger.h>

#include <altimeter/audit/constants.h>
#include <altimeter/event/constants.h>
#include <altimeter/event/event_logger.h>

#include "altimeter_helper.h"
#include "logging.h"

namespace tateyama::altimeter {

altimeter_helper::altimeter_helper(tateyama::api::configuration::whole* conf) : conf_(conf) {
}

altimeter_helper::~altimeter_helper() {
    shutdown();
}

void altimeter_helper::start() {
    auto dbname = conf_->get_section("ipc_endpoint")->get<std::string>("database_name").value();
    setup(cfgs_.at(0), conf_->get_section("event_log"), log_type::event_log, dbname);
    setup(cfgs_.at(1), conf_->get_section("audit_log"), log_type::audit_log);

    ::altimeter::logger::start(cfgs_);
}

void altimeter_helper::shutdown() {
    if (!shutdown_) {
        ::altimeter::logger::shutdown();
        shutdown_ = true;
    }
}

//
// The following method is created with reference to altimeter/logger/examples/altimeter/main.cpp
//
void altimeter_helper::setup(::altimeter::configuration& configuration, tateyama::api::configuration::section* section, log_type type, [[maybe_unused]] const std::string& dbname) {
    configuration.category((type == log_type::event_log) ? ::altimeter::event::category : ::altimeter::audit::category);
    auto output = section->get<bool>("output").value();
    configuration.output(output);
    auto directory = section->get<std::filesystem::path>("directory").value().string();
    configuration.directory(directory);
    auto level = section->get<int>("level").value();
    configuration.level(level);
    auto file_number = section->get<std::uint32_t>("file_number").value();
    configuration.file_number(file_number);
    auto sync = section->get<bool>("sync").value();
    configuration.sync(sync);
    auto buffer_size = section->get<std::size_t>("buffer_size").value();
    configuration.buffer_size(buffer_size);
    auto flush_interval = section->get<std::size_t>("flush_interval").value();
    configuration.flush_interval(flush_interval);
    auto flush_file_size = section->get<std::size_t>("flush_file_size").value();
    configuration.flush_file_size(flush_file_size);
    auto max_file_size = section->get<std::size_t>("max_file_size").value();
    configuration.max_file_size(max_file_size);

    if (type == log_type::event_log) {
        configuration.error_handler([](std::string_view error_message) {
            std::cout << "Failed to flush or rotate event log files: "
                      << error_message << "\n";
        });
        ::altimeter::event::event_logger::set_stmt_duration_threshold(section->get<std::size_t>("stmt_duration_threshold").value());
//        ::altimeter::event::event_logger::set_level(level, dbname);
    } else {
        configuration.error_handler([](std::string_view error_message) {
            std::cout << "Failed to flush or rotate audit log files: "
                      << error_message << "\n";
        });
    }

    std::string_view log_type_name = (type == log_type::event_log) ? "event log" : "audit log";
    configuration.error_handler([log_type_name](std::string_view error_message) {
        LOG(ERROR) << "Failed to flush or rotate " << log_type_name << " files: "
                   << error_message << "\n";
    });
    configuration.log_write_error_handler(
        [log_type_name](std::string_view error_message, std::string_view log) {
            LOG(ERROR) << "Failed to " << log_type_name << " write: " << error_message
                       << ", log: " << log << "\n";
        });

    // output configuration to be used
    const std::string_view& prefix = (type == log_type::event_log) ? altimeter_event_config_prefix : altimeter_audit_config_prefix;
    LOG(INFO) << prefix << "output = " << std::boolalpha << output          << ", log output flag.";
    LOG(INFO) << prefix << "directory = "                << directory       << ", log-storage directory path";
    LOG(INFO) << prefix << "level = "                    << level           << ", log level";
    LOG(INFO) << prefix << "file_number = "              << file_number     << ", number of log output files";
    LOG(INFO) << prefix << "sync = " << std::boolalpha   << sync            << ", log-synchronous storage";
    LOG(INFO) << prefix << "buffer_size = "              << buffer_size     << ", buffer size per log file";
    LOG(INFO) << prefix << "flush_interval = "           << flush_interval  << ", flash interval (milliseconds)";
    LOG(INFO) << prefix << "flush_file_size = "          << flush_file_size << ", file size to be flashed";
    LOG(INFO) << prefix << "max_file_size = "            << max_file_size   << ", file size to rotate";
    if (type == log_type::event_log) {
        LOG(INFO) << prefix << "stmt_duration_threshold = " << section->get<std::size_t>("stmt_duration_threshold").value() << ", duration threshold for statement log";
    }
}

} // tateyama::altimeter
