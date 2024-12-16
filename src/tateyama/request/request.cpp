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

#include <ctime>
#include <set>
#include <sstream>
#include <map>

#include <gflags/gflags.h>

#include "tateyama/authentication/authentication.h"
#include <tateyama/proto/request/request.pb.h>
#include <tateyama/proto/request/response.pb.h>

#include <tateyama/transport/transport.h>
#include <tateyama/monitor/monitor.h>

#include "base64.h"
#include "request.h"

DEFINE_int32(top, 0, "show top");  // NOLINT
DEFINE_int32(service, 3, "service id");  // NOLINT
DECLARE_bool(quiet);
DECLARE_string(monitor);

namespace tateyama::request {

tgctl::return_code request_list() { // NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_request);
        ::tateyama::proto::request::request::Request request{};
        (void) request.mutable_list_request();
        auto response_opt = transport->send<::tateyama::proto::request::response::ListRequest>(request);
        request.clear_list_request();

        if (response_opt) {
            auto response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::request::response::ListRequest::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::request::response::ListRequest::ResultCase::kError:
                std::cerr << "ListRequest error: " << response.error().message() << std::endl;
                rtnv = tgctl::return_code::err;
                break;
            default:
                std::cerr << "ListRequest result_case() error: " << std::endl;
                rtnv = tgctl::return_code::err;
            }

            if (rtnv == tgctl::return_code::ok) {
                //                    10          10           9            12                 17
                std::cout << "session-id  request-id  service-id  payload-size  elapsed-time (ms)" << std::endl;
                std::cout << "----------  ----------  ----------  ------------  -----------------" << std::endl;
                for (auto&& e : response.success().requests()) {
                    std::cout << std::setw(10) << e.session_id()
                              << std::setw(10 + 2) << e.request_id()
                              << std::setw( 9 + 2) << e.service_id()
                              << std::setw(12 + 2) << e.payload_size()
                              << std::setw(17 + 2) << e.started_time()
                              << std::endl;
                }
            }
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code request_payload(std::size_t session_id, std::size_t request_id) { // NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_request);
        ::tateyama::proto::request::request::Request request{};
        auto* get_payload = request.mutable_get_payload();
        get_payload->set_session_id(session_id);
        get_payload->set_request_id(request_id);
        auto response_opt = transport->send<::tateyama::proto::request::response::GetPayload>(request);
        request.clear_list_request();

        if (response_opt) {
            auto response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::request::response::GetPayload::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::request::response::GetPayload::ResultCase::kError:
                std::cerr << "GetPayload error: " << response.error().message() << std::endl;
                rtnv = tgctl::return_code::err;
                break;
            default:
                std::cerr << "GetPayload result_case() error: " << std::endl;
                rtnv = tgctl::return_code::err;
            }

            if (rtnv == tgctl::return_code::ok) {
                std::stringstream ssi;
                std::stringstream sso;

                ssi << response.success().data();
                encode(ssi, sso);
                std::cout << sso.str();
            }
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code request_extract_sql(std::string_view payload) { // NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();
    try {
        std::stringstream ssi;
        std::stringstream sso;
        ssi << payload;
        decode(ssi, sso);

        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_request);
        ::tateyama::proto::request::request::Request request{};
        auto* extract_statement_info = request.mutable_extract_statement_info();
        extract_statement_info->set_payload(sso.str());
        auto response_opt = transport->send<::tateyama::proto::request::response::ExtractStatementInfo>(request);
        request.clear_list_request();

        if (response_opt) {
            auto response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::request::response::ExtractStatementInfo::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::request::response::ExtractStatementInfo::ResultCase::kError:
                std::cerr << "ExtractStatementInfo error: " << response.error().message() << std::endl;
                rtnv = tgctl::return_code::err;
                break;
            default:
                std::cerr << "ExtractStatementInfo result_case() error: " << std::endl;
                rtnv = tgctl::return_code::err;
            }

            if (rtnv == tgctl::return_code::ok) {
                std::string transaction_id{};
                std::string sql{};

                auto& success = response.success();

                if (success.transaction_id_opt_case() ==
                    tateyama::proto::request::response::ExtractStatementInfo_Success::TransactionIdOptCase::kTransactionId) {
                    transaction_id = success.transaction_id().id();
                }
                if (success.sql_opt_case() ==
                    tateyama::proto::request::response::ExtractStatementInfo_Success::SqlOptCase::kSql) {
                    sql = success.sql();

                    std::cout << sql << std::endl;
                }

            }
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'" << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

} //  tateyama::request
