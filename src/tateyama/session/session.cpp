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

#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>
#include <tateyama/proto/session/diagnostic.pb.h>

#include "tateyama/authentication/authentication.h"
#include "tateyama/transport/transport.h"
#include "tateyama/monitor/monitor.h"
#include "tateyama/tgctl/runtime_error.h"

#include "session.h"

DEFINE_bool(verbose, false, "show session list in verbose format");  // NOLINT
DEFINE_bool(id, false, "show session list using session id");  // NOLINT
DECLARE_string(monitor);
DECLARE_bool(graceful);
DECLARE_bool(forceful);

namespace tateyama::session {

static std::string to_timepoint_string(std::uint64_t msu) {
    std::chrono::time_point<std::chrono::system_clock> e0{};
    std::chrono::time_point<std::chrono::system_clock> t = e0 + std::chrono::milliseconds(static_cast<std::int64_t>(msu));

    std::stringstream stream;
    time_t epoch_seconds = std::chrono::system_clock::to_time_t(t);
    struct tm buf{};
    gmtime_r(&epoch_seconds, &buf);
    stream << std::put_time(&buf, "%FT%TZ");
    return stream.str();
}

tgctl::return_code session_list() { //NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    auto reason = monitor::reason::absent;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_session);
        ::tateyama::proto::session::request::Request request{};
        (void) request.mutable_session_list();
        auto response_opt = transport->send<::tateyama::proto::session::response::SessionList>(request);
        request.clear_session_list();

        if (response_opt) {
            const auto& response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::session::response::SessionList::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::session::response::SessionList::ResultCase::kError:
                std::cerr << "SessionList error: " << response.error().message() << '\n' << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::server;
                break;
            default:
                std::cerr << "SessionList result_case() error: \n" << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::payload_broken;
            }

            if (rtnv == tgctl::return_code::ok) {
                std::multiset<std::string> labels{};
                auto& session_list = response.success().entries();

                std::size_t id_max{2};
                std::size_t label_max{5};
                std::size_t application_max{11};
                std::size_t user_max{4};
                std::size_t start_max{5};
                std::size_t type_max{4};
                std::size_t remote_max{6};
                for(auto& e : session_list) {
                    id_max = std::max(id_max, e.session_id().length());
                    label_max = std::max(label_max, e.label().length());
                    application_max = std::max(application_max, e.application().length());
                    user_max = std::max(user_max, e.user().length());
                    auto ts = to_timepoint_string(e.start_at());
                    start_max = std::max(start_max, ts.length());
                    type_max = std::max(type_max, e.connection_type().length());
                    remote_max = std::max(remote_max, e.connection_info().length());
                    if (!e.label().empty()) {
                        labels.emplace(e.label());
                    }
                }

                id_max += 2;
                label_max += 2;
                application_max += 2;
                user_max += 2;
                start_max += 2;
                type_max += 2;
                remote_max += 2;

                if (FLAGS_verbose) {
                    std::cout << std::left;
                    std::cout << std::setw(static_cast<int>(id_max)) << "id";
                    std::cout << std::setw(static_cast<int>(label_max)) << "label";
                    std::cout << std::setw(static_cast<int>(application_max)) << "application";
                    std::cout << std::setw(static_cast<int>(user_max)) << "user";
                    std::cout << std::setw(static_cast<int>(start_max)) << "start";
                    std::cout << std::setw(static_cast<int>(type_max)) << "type";
                    std::cout << std::setw(static_cast<int>(remote_max)) << "remote\n";
                    std::cout << std::flush;
                }
                std::map<std::size_t, std::string> output{};
                for(auto& e : session_list) {
                    const auto& e_session_id = e.session_id();
                    auto session_id = stol(e_session_id.substr(1));
                    if (FLAGS_verbose) {
                        std::ostringstream ss{};
                        ss << std::left;
                        ss << std::setw(static_cast<int>(id_max)) << e_session_id;
                        ss << std::setw(static_cast<int>(label_max)) << e.label();
                        ss << std::setw(static_cast<int>(application_max)) << e.application();
                        ss << std::setw(static_cast<int>(user_max)) << e.user();
                        ss << std::setw(static_cast<int>(start_max)) << to_timepoint_string(e.start_at());
                        ss << std::setw(static_cast<int>(type_max)) << e.connection_type();
                        ss << std::setw(static_cast<int>(remote_max)) << e.connection_info();
                        output.emplace(session_id, ss.str());
                    } else {
                        std::ostringstream ss{};
                        auto& label = e.label();
                        ss << ((label.empty() || label.find(' ') != std::string::npos || label.find('\t') != std::string::npos || FLAGS_id || labels.count(label) > 1) ?   // NOLINT(abseil-string-find-str-contains)
                                      e_session_id : e.label()) << ' ';
                        output.emplace(session_id, ss.str());
                    }
                    if (monitor_output) {
                        monitor_output->session_info(e_session_id, e.label(), e.application(), e.user(), to_timepoint_string(e.start_at()), e.connection_type(), e.connection_info());
                    }
                }
                if (!output.empty()) {
                    for(auto&& e: output) {
                        std::cout << e.second << '\n' << std::flush;
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

tgctl::return_code session_show(std::string_view session_ref) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    auto reason = monitor::reason::absent;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_session);
        ::tateyama::proto::session::request::Request request{};
        auto* command = request.mutable_session_get();
        command->set_session_specifier(std::string(session_ref));
        auto response_opt = transport->send<::tateyama::proto::session::response::SessionGet>(request);
        request.clear_session_get();

        if (response_opt) {
            const auto& response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::session::response::SessionGet::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::session::response::SessionGet::ResultCase::kError:
                std::cerr << "SessionShow error: " << response.error().message() << '\n' << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::server;
                break;
            default:
                std::cerr << "SessionShow result_case() error: \n" << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::payload_broken;
            }

            if (rtnv == tgctl::return_code::ok) {
                auto e = response.success().entry();

                std::cout << std::left;
                std::cout << std::setw(13) << "id" << e.session_id() << '\n';
                std::cout << std::setw(13) << "application" << e.application() << '\n';
                std::cout << std::setw(13) << "label" << e.label() << '\n';
                std::cout << std::setw(13) << "user" << e.user() << '\n';
                std::cout << std::setw(13) << "start" << to_timepoint_string(e.start_at()) << '\n';
                std::cout << std::setw(13) << "type" << e.connection_type() << '\n';
                std::cout << std::setw(13) << "remote" << e.connection_info() << '\n' << std::flush;
                if (monitor_output) {
                    monitor_output->session_info(e.session_id(), e.label(), e.application(), e.user(), to_timepoint_string(e.start_at()), e.connection_type(), e.connection_info());
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

tgctl::return_code session_shutdown(std::string_view session_ref) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    auto reason = monitor::reason::absent;
    if (FLAGS_graceful && FLAGS_forceful) {
        std::cerr << "both forceful and graceful options specified\n" << std::flush;
        rtnv = tgctl::return_code::err;
    } else {
        authentication::auth_options();
        try {
            auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_session);
            ::tateyama::proto::session::request::Request request{};
            auto* command = request.mutable_session_shutdown();
            command->set_session_specifier(std::string(session_ref));
            if (FLAGS_graceful) {
                command->set_request_type(::tateyama::proto::session::request::SessionShutdownType::GRACEFUL);
            } else if (FLAGS_forceful) {
                command->set_request_type(::tateyama::proto::session::request::SessionShutdownType::FORCEFUL);
            }
            auto response_opt = transport->send<::tateyama::proto::session::response::SessionShutdown>(request);
            request.clear_session_shutdown();

            if (response_opt) {
                const auto& response = response_opt.value();
                switch(response.result_case()) {
                case ::tateyama::proto::session::response::SessionShutdown::ResultCase::kSuccess:
                    break;
                case ::tateyama::proto::session::response::SessionShutdown::ResultCase::kError:
                    std::cerr << "SessionShutdown error: " << response.error().message() << '\n' << std::flush;
                    rtnv = tgctl::return_code::err;
                    reason = monitor::reason::server;
                    break;
                default:
                    std::cerr << "SessionShutdown result_case() error: \n" << std::flush;
                    rtnv = tgctl::return_code::err;
                    reason = monitor::reason::payload_broken;
                }

                if (rtnv == tgctl::return_code::ok) {
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
    }

    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return rtnv;
}

tgctl::return_code session_swtch(std::string_view session_ref, std::string_view set_key, std::string_view set_value, bool set) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    auto reason = monitor::reason::absent;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_session);
        ::tateyama::proto::session::request::Request request{};
        auto* command = request.mutable_session_set_variable();
        command->set_session_specifier(std::string(session_ref));
        command->set_name(std::string(set_key));
        if (set) {
            command->set_value(std::string(set_value));
        }
        auto response_opt = transport->send<::tateyama::proto::session::response::SessionSetVariable>(request);
        request.clear_session_set_variable();

        if (response_opt) {
            const auto& response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::session::response::SessionSetVariable::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::session::response::SessionSetVariable::ResultCase::kError:
                std::cerr << "SessionSetVariable error: " << response.error().message() << '\n' << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::server;
                break;
            default:
                std::cerr << "SessionSetVariable result_case() error: \n" << std::flush;
                rtnv = tgctl::return_code::err;
                reason = monitor::reason::payload_broken;
            }

            if (rtnv == tgctl::return_code::ok) {
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

} //  tateyama::bootstrap::backup
