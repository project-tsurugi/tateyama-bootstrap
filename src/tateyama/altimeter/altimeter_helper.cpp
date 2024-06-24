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

namespace tateyama::altimeter {

altimeter_helper::altimeter_helper(tateyama::api::configuration::whole* conf) {
    setup(cfgs_.at(0), conf->get_section("event_log"), log_type::event_log);
    setup(cfgs_.at(1), conf->get_section("audit_log"), log_type::audit_log);
}

altimeter_helper::~altimeter_helper() {
    shutdown();
}

//
// The following method is created with reference to altimeter/logger/examples/altimeter/main.cpp
//
void altimeter_helper::setup(::altimeter::configuration& configuration, tateyama::api::configuration::section* section, log_type type) {
    configuration.category((type == log_type::event_log) ? ::altimeter::event::category : ::altimeter::audit::category);
    configuration.output(section->get<bool>("output").value());
    configuration.directory(section->get<std::filesystem::path>("directory").value().string());
    configuration.level(section->get<int>("level").value());
    configuration.file_number(section->get<std::uint32_t>("file_number").value());
    configuration.sync(section->get<bool>("sync").value());
    configuration.buffer_size(section->get<std::size_t>("buffer_size").value());
    configuration.flush_interval(section->get<std::size_t>("flush_interval").value());
    configuration.flush_file_size(section->get<std::size_t>("flush_file_size").value());
    configuration.max_file_size(section->get<std::size_t>("max_file_size").value());
    if (type == log_type::event_log) {
        configuration.error_handler([](std::string_view error_message) {
            std::cout << "Failed to flush or rotate event log files: "
                      << error_message << "\n";
        });
        ::altimeter::event::event_logger::set_stmt_duration_threshold(section->get<std::size_t>("stmt_duration_threshold").value());
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
}

void altimeter_helper::start() {
    ::altimeter::logger::start(cfgs_);
}

void altimeter_helper::shutdown() {
    if (!shutdown_) {
        ::altimeter::logger::shutdown();
        shutdown_ = true;
    }
}

} // tateyama::altimeter
