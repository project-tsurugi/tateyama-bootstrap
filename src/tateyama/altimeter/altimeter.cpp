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

#include <string>
#include <string_view>

#include <gflags/gflags.h>

#include <tateyama/proto/altimeter/request.pb.h>
#include <tateyama/proto/altimeter/response.pb.h>
#include <tateyama/proto/altimeter/common.pb.h>

#include "tateyama/authentication/authentication.h"
#include "tateyama/transport/transport.h"
#include "tateyama/monitor/monitor.h"
#include "tateyama/tgctl/runtime_error.h"

#include "altimeter.h"

DECLARE_string(monitor);

namespace tateyama::altimeter {

template <class T>
static monitor::reason post_processing(std::optional<T>& response_opt, const std::string_view sub_command) {
    if (response_opt) {
        auto response = response_opt.value();
    
        switch(response.result_case()) {
        case T::ResultCase::kSuccess:
            return monitor::reason::absent;
        case T::ResultCase::kError:
            std::cerr << "altimeter " << sub_command << " error: " << response.error().message() << std::endl;
            return monitor::reason::server;
        default:
            std::cerr << "altimeter " << sub_command << " returns illegal response" << std::endl;
            return monitor::reason::payload_broken;
        }
    }
    std::cerr << "altimeter " << sub_command << " returns nullopt" << std::endl;
    return monitor::reason::payload_broken;
}

tgctl::return_code set_enabled(const std::string& type, bool enabled) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto reason = monitor::reason::absent;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_altimeter);
        ::tateyama::proto::altimeter::request::Request request{};
        auto* mutable_configure = request.mutable_configure();
        if (type == "event") {
            mutable_configure->mutable_event_log()->set_enabled(enabled);
        } else if(type == "audit") {
            mutable_configure->mutable_audit_log()->set_enabled(enabled);
        } else {
            throw tgctl::runtime_error(monitor::reason::internal, "illegal type for altimeter set_enabled");
        }
        auto response_opt = transport->send<::tateyama::proto::altimeter::response::Configure>(request);
        request.clear_configure();

        reason = post_processing<::tateyama::proto::altimeter::response::Configure>(response_opt, "set_enabled");
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
        reason = ex.code();
    }

    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return (reason == monitor::reason::absent) ? tgctl::return_code::err : tgctl::return_code::err;  // NOLINT(misc-redundant-expression)
}

tgctl::return_code set_log_level(const std::string& type, const std::string& level) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto reason = monitor::reason::absent;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_altimeter);
        ::tateyama::proto::altimeter::request::Request request{};
        auto* mutable_configure = request.mutable_configure();
        std::uint64_t l = std::stoul(level);
        if (type == "event") {
            mutable_configure->mutable_event_log()->set_level(l);
        } else if(type == "audit") {
            mutable_configure->mutable_audit_log()->set_level(l);
        } else {
            throw tgctl::runtime_error(monitor::reason::internal, "illegal type for altimeter set_log_level");
        }
        auto response_opt = transport->send<::tateyama::proto::altimeter::response::Configure>(request);
        request.clear_configure();

        reason = post_processing<::tateyama::proto::altimeter::response::Configure>(response_opt, "set_log_level");
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
        reason = ex.code();
    }

    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return (reason == monitor::reason::absent) ? tgctl::return_code::err : tgctl::return_code::err;  // NOLINT(misc-redundant-expression)
}

tgctl::return_code set_statement_duration(const std::string& value) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto reason = monitor::reason::absent;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_altimeter);
        ::tateyama::proto::altimeter::request::Request request{};
        auto* mutable_configure = request.mutable_configure();
        std::uint64_t v = std::stoul(value);
        mutable_configure->mutable_event_log()->set_statement_duration(v);
        auto response_opt = transport->send<::tateyama::proto::altimeter::response::Configure>(request);
        request.clear_configure();

        reason = post_processing<::tateyama::proto::altimeter::response::Configure>(response_opt, "set_statement_duration");
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
        reason = ex.code();
    }

    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return (reason == monitor::reason::absent) ? tgctl::return_code::err : tgctl::return_code::err;  // NOLINT(misc-redundant-expression)
}

tgctl::return_code rotate(const std::string& type) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto reason = monitor::reason::absent;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_altimeter);
        ::tateyama::proto::altimeter::request::Request request{};
        auto* mutable_log_rotate = request.mutable_log_rotate();
        ::tateyama::proto::altimeter::common::LogCategory category{}; 
        if (type == "event") {
            category = ::tateyama::proto::altimeter::common::LogCategory::EVENT;
        } else if(type == "audit") {
            category = ::tateyama::proto::altimeter::common::LogCategory::AUDIT;
        } else {
            throw tgctl::runtime_error(monitor::reason::internal, "illegal type for altimeter rotate");
        }
        mutable_log_rotate->set_category(category);
        auto response_opt = transport->send<::tateyama::proto::altimeter::response::LogRotate>(request);
        request.clear_log_rotate();

        reason = post_processing<::tateyama::proto::altimeter::response::LogRotate>(response_opt, "rotete");
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
        reason = ex.code();
    }

    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return (reason == monitor::reason::absent) ? tgctl::return_code::err : tgctl::return_code::err;  // NOLINT(misc-redundant-expression)
}

} //  tateyama::bootstrap::backup
