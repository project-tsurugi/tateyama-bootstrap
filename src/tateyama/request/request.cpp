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
#include <chrono>

#include <gflags/gflags.h>

#include "tateyama/authentication/authenticator.h"
#include <tateyama/proto/request/request.pb.h>
#include <tateyama/proto/request/response.pb.h>
#include <jogasaki/proto/sql/request.pb.h>
#include <jogasaki/proto/sql/response.pb.h>

#include "tateyama/transport/transport.h"
#include "tateyama/monitor/monitor.h"
#include "tateyama/tgctl/runtime_error.h"

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
    auto reason = monitor::reason::absent;
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_request);
        ::tateyama::proto::request::request::Request request{};
        (void) request.mutable_list_request();
        auto response_opt = transport->send<::tateyama::proto::request::response::ListRequest>(request);
        request.clear_list_request();

        if (response_opt) {
            const auto& response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::request::response::ListRequest::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::request::response::ListRequest::ResultCase::kError:
                std::cerr << "ListRequest error: " << response.error().message() << '\n' << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::server;
                break;
            default:
                std::cerr << "ListRequest result_case() error: \n" << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::payload_broken;
            }

            if (rtnv == tgctl::return_code::ok) {
                auto tp = std::chrono::system_clock::now();
                auto secs = std::chrono::time_point_cast<std::chrono::seconds>(tp);
                auto ns = std::chrono::time_point_cast<std::chrono::nanoseconds>(tp) - std::chrono::time_point_cast<std::chrono::nanoseconds>(secs);
                auto nms = secs.time_since_epoch().count() * 1000 + (ns.count() + 500000) / 1000000;
                if (!FLAGS_quiet) {
                    //                    10          10          10            12                 17
                    std::cout << "session-id  request-id  service-id  payload-size  elapsed-time (ms)\n";
                    std::cout << "----------  ----------  ----------  ------------  -----------------\n";
                }
                for (auto&& e : response.success().requests()) {
                    std::size_t elapsed = nms - e.started_time();
                    if (!FLAGS_quiet) {
                        std::cout << std::setw(10) << e.session_id()
                                  << std::setw(10 + 2) << e.request_id()
                                  << std::setw(10 + 2) << e.service_id()
                                  << std::setw(12 + 2) << e.payload_size()
                                  << std::setw(17 + 2) << elapsed
                                  << '\n' << std::flush;
                    }
                    if (!FLAGS_monitor.empty()) {
                        monitor_output->request_list(e.session_id(), e.request_id(), e.service_id(), e.payload_size(), elapsed);
                    }
                }
                if (monitor_output) {
                    monitor_output->finish(monitor::reason::absent);
                }
                return rtnv;
            }
        } else {
            reason = monitor::reason::payload_broken;
        }
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'\n" << std::flush;
        reason = ex.code();
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(reason);
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
    auto reason = monitor::reason::absent;
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_request);
        ::tateyama::proto::request::request::Request request{};
        auto* get_payload = request.mutable_get_payload();
        get_payload->set_session_id(session_id);
        get_payload->set_request_id(request_id);
        auto response_opt = transport->send<::tateyama::proto::request::response::GetPayload>(request);
        request.clear_get_payload();

        if (response_opt) {
            const auto& response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::request::response::GetPayload::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::request::response::GetPayload::ResultCase::kError:
                std::cerr << "GetPayload error: " << response.error().message() << '\n' << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::server;
                break;
            default:
                std::cerr << "GetPayload result_case() error: \n" << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::payload_broken;
            }

            if (rtnv == tgctl::return_code::ok) {
                std::stringstream ssi;
                std::stringstream sso;

                ssi << response.success().data();
                encode(ssi, sso);
                if (!FLAGS_quiet) {
                    std::cout << sso.str();
                }
                if (monitor_output) {
                    monitor_output->request_payload(sso.str());
                    monitor_output->finish(monitor::reason::absent);
                }
                return rtnv;
            }
        } else {
            reason = monitor::reason::payload_broken;
        }
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'\n" << std::flush;
        reason = ex.code();
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return rtnv;
}

tgctl::return_code request_extract_sql(std::size_t session_id, std::string_view payload) { // NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    auto reason = monitor::reason::absent;
    try {
        std::stringstream ssi;
        std::stringstream sso;
        ssi << payload;
        decode(ssi, sso);

        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_sql);
        ::jogasaki::proto::sql::request::Request request{};
        auto* extract_statement_info = request.mutable_extract_statement_info();
        extract_statement_info->set_session_id(session_id);
        extract_statement_info->set_payload(sso.str());
        auto response_opt = transport->send<::jogasaki::proto::sql::response::Response>(request);
        request.clear_extract_statement_info();

        if (response_opt) {
            const auto& response = response_opt.value();
            if (response.response_case() == ::jogasaki::proto::sql::response::Response::ResponseCase::kExtractStatementInfo) {
                const auto& extract_statement_info = response.extract_statement_info();    
                switch(extract_statement_info.result_case()) {
                case ::jogasaki::proto::sql::response::ExtractStatementInfo::ResultCase::kSuccess:
                    break;
                case ::jogasaki::proto::sql::response::ExtractStatementInfo::ResultCase::kError:
                    std::cerr << "ExtractStatementInfo error: " << extract_statement_info.error().detail() << '\n' << std::flush;
                    rtnv = tgctl::return_code::err;
                    reason = monitor::reason::server;
                    break;
                default:
                    std::cerr << "ExtractStatementInfo result_case() error: \n" << std::flush;
                    rtnv = tgctl::return_code::err;
                    reason = monitor::reason::payload_broken;
                }

                if (rtnv == tgctl::return_code::ok) {
                    std::optional<std::string> transaction_id{};
                    std::optional<std::string> sql{};

                    auto& success = extract_statement_info.success();

                    if (success.transaction_id_opt_case() ==
                        jogasaki::proto::sql::response::ExtractStatementInfo_Success::TransactionIdOptCase::kTransactionId) {
                        transaction_id = success.transaction_id().id();
                    }
                    if (success.sql_opt_case() ==
                        jogasaki::proto::sql::response::ExtractStatementInfo_Success::SqlOptCase::kSql) {
                        sql = success.sql();
                        if (!FLAGS_quiet) {
                            std::cout << sql.value() << '\n' << std::flush;
                        }
                    }
                    if (monitor_output) {
                        monitor_output->request_extract_sql(transaction_id, sql);
                        monitor_output->finish(monitor::reason::absent);
                    }
                    return rtnv;
                }
            } else {
                std::cerr << "the response type does not match with that expected\n" << std::flush;                
                reason = monitor::reason::payload_broken;
            }
        } else {
            reason = monitor::reason::payload_broken;
        }
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'\n" << std::flush;
        reason = ex.code();
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return rtnv;
}

} //  tateyama::request
