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
#include <array>

#include <gflags/gflags.h>

#include "tateyama/authentication/authentication.h"
#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>
#include <tateyama/proto/session/diagnostic.pb.h>

#include <tateyama/transport/transport.h>
#include <tateyama/monitor/monitor.h>
#include "session.h"

DECLARE_string(monitor);
DECLARE_bool(force);

namespace tateyama::session {

static std::string to_timepoint_string(std::uint64_t msu) {
    auto ms = static_cast<std::int64_t>(msu);
    std::timespec ts{ms / 1000, (ms % 1000) * 1000000};

    auto* when = std::localtime(&ts.tv_sec);
    constexpr int array_size = 32;
    std::array<char, array_size> buf{};
    if (std::strftime(buf.data(), array_size - 1, "%Y-%m-%d %H:%M:%S", when) == 0) {
        return {};
    }
    std::array<char, array_size> output{};
    const int msec = static_cast<std::int32_t>(ts.tv_nsec) / 1000000;
    auto len = snprintf(output.data(), array_size, "%s.%03d %s", buf.data(), msec, when->tm_zone);  // NOLINT(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
    std::string rv{output.data(), static_cast<std::size_t>(len)};
    return rv;
}

tgctl::return_code session_list() {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_session);
        ::tateyama::proto::session::request::Request request{};
        (void) request.mutable_session_list();
        auto response_opt = transport->send<::tateyama::proto::session::response::SessionList>(request);
        request.clear_session_list();

        if (response_opt) {
            auto response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::session::response::SessionList::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::session::response::SessionList::ResultCase::kError:
                std::cerr << "SessionList error: " << response.error().message() << std::endl;
                rtnv = tgctl::return_code::err;
                break;
            default:
                std::cerr << "SessionList result_case() error: " << std::endl;
                rtnv = tgctl::return_code::err;
            }

            if (rtnv == tgctl::return_code::ok) {
                auto session_list = response.success().entries();

                std::size_t id_max{2};
                std::size_t label_max{5};
                std::size_t application_max{11};
                std::size_t user_max{4};
                std::size_t start_max{5};
                std::size_t type_max{4};
                std::size_t remote_max{6};
                for( auto& e : session_list ) {
                    if (id_max < e.session_id().length()) {
                        id_max = e.session_id().length();
                    }
                    if (label_max < e.label().length()) {
                        label_max = e.label().length();
                    }
                    if (application_max < e.application().length()) {
                        application_max = e.application().length();
                    }
                    if (user_max < e.user().length()) {
                        user_max = e.user().length();
                    }
                    auto ts = to_timepoint_string(e.start_at());
                    if (start_max < ts.length()) {
                        start_max = ts.length();
                    }
                    if (type_max < e.connection_type().length()) {
                        type_max = e.connection_type().length();
                    }
                    if (remote_max < e.connection_info().length()) {
                        remote_max = e.connection_info().length();
                    }
                }

                id_max += 2;
                label_max += 2;
                application_max += 2;
                user_max += 2;
                start_max += 2;
                type_max += 2;
                remote_max += 2;

                std::cout << std::left;
                std::cout << std::setw(static_cast<int>(id_max)) << "id";
                std::cout << std::setw(static_cast<int>(label_max)) << "label";
                std::cout << std::setw(static_cast<int>(application_max)) << "application";
                std::cout << std::setw(static_cast<int>(user_max)) << "user";
                std::cout << std::setw(static_cast<int>(start_max)) << "start";
                std::cout << std::setw(static_cast<int>(type_max)) << "type";
                std::cout << std::setw(static_cast<int>(remote_max)) << "remote";
                std::cout << std::endl;

                for( auto& e : session_list ) {
                    std::cout << std::setw(static_cast<int>(id_max)) << e.session_id();
                    std::cout << std::setw(static_cast<int>(label_max)) << e.label();
                    std::cout << std::setw(static_cast<int>(application_max)) << e.application();
                    std::cout << std::setw(static_cast<int>(user_max)) << e.user();
                    std::cout << std::setw(static_cast<int>(start_max)) << to_timepoint_string(e.start_at());
                    std::cout << std::setw(static_cast<int>(type_max)) << e.connection_type();
                    std::cout << std::setw(static_cast<int>(remote_max)) << e.connection_info();
                    std::cout << std::endl;
                    if (monitor_output) {
                        monitor_output->session_info(e.session_id(), e.label(), e.application(), e.user(), to_timepoint_string(e.start_at()), e.connection_type(), e.connection_info());
                    }
                }

                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rtnv;
            }
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "could not connect to database with name " << tateyama::bootstrap::wire::transport::database_name() << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
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
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_session);
        ::tateyama::proto::session::request::Request request{};
        auto* command = request.mutable_session_get();
        command->set_session_specifier(std::string(session_ref));
        auto response_opt = transport->send<::tateyama::proto::session::response::SessionGet>(request);
        request.clear_session_get();

        if (response_opt) {
            auto response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::session::response::SessionGet::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::session::response::SessionGet::ResultCase::kError:
                std::cerr << "SessionShow error: " << response.error().message() << std::endl;
                rtnv = tgctl::return_code::err;
                break;
            default:
                std::cerr << "SessionShow result_case() error: " << std::endl;
                rtnv = tgctl::return_code::err;
            }

            if (rtnv == tgctl::return_code::ok) {
                auto e = response.success().entry();

                std::cout << std::left;
                std::cout << std::setw(static_cast<int>(13)) << "id" << e.session_id() << std::endl;
                std::cout << std::setw(static_cast<int>(13)) << "application" << e.application() << std::endl;
                std::cout << std::setw(static_cast<int>(13)) << "label" << e.label() << std::endl;
                std::cout << std::setw(static_cast<int>(13)) << "user" << e.user() << std::endl;
                std::cout << std::setw(static_cast<int>(13)) << "start" << to_timepoint_string(e.start_at()) << std::endl;
                std::cout << std::setw(static_cast<int>(13)) << "type" << e.connection_type() << std::endl;
                std::cout << std::setw(static_cast<int>(13)) << "remote" << e.connection_info() << std::endl;
                if (monitor_output) {
                    monitor_output->session_info(e.session_id(), e.label(), e.application(), e.user(), to_timepoint_string(e.start_at()), e.connection_type(), e.connection_info());
                    monitor_output->finish(true);
                }
                return rtnv;
            }
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "could not connect to database with name " << tateyama::bootstrap::wire::transport::database_name() << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code session_kill(std::string_view session_ref) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_session);
        ::tateyama::proto::session::request::Request request{};
        auto* command = request.mutable_session_shutdown();
        command->set_session_specifier(std::string(session_ref));
        command->set_request_type(FLAGS_force ?
                                  ::tateyama::proto::session::request::SessionShutdownType::FORCEFUL :
                                  ::tateyama::proto::session::request::SessionShutdownType::GRACEFUL);
        auto response_opt = transport->send<::tateyama::proto::session::response::SessionShutdown>(request);
        request.clear_session_shutdown();

        if (response_opt) {
            auto response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::session::response::SessionShutdown::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::session::response::SessionShutdown::ResultCase::kError:
                std::cerr << "SessionShutdown error: " << response.error().message() << std::endl;
                rtnv = tgctl::return_code::err;
                break;
            default:
                std::cerr << "SessionShutdown result_case() error: " << std::endl;
                rtnv = tgctl::return_code::err;
            }

            if (rtnv == tgctl::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rtnv;
            }
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "could not connect to database with name " << tateyama::bootstrap::wire::transport::database_name() << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

tgctl::return_code session_swtch(std::string_view session_ref, std::string_view set_key, std::string_view set_value) {
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto rtnv = tgctl::return_code::ok;
    authentication::auth_options();
    try {
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_session);
        ::tateyama::proto::session::request::Request request{};
        auto* command = request.mutable_session_set_variable();
        command->set_session_specifier(std::string(session_ref));
        command->set_name(std::string(set_key));
        command->set_value(std::string(set_value));
        auto response_opt = transport->send<::tateyama::proto::session::response::SessionSetVariable>(request);
        request.clear_session_set_variable();

        if (response_opt) {
            auto response = response_opt.value();
            switch(response.result_case()) {
            case ::tateyama::proto::session::response::SessionSetVariable::ResultCase::kSuccess:
                break;
            case ::tateyama::proto::session::response::SessionSetVariable::ResultCase::kError:
                std::cerr << "SessionSetVariable error: " << response.error().message() << std::endl;
                rtnv = tgctl::return_code::err;
                break;
            default:
                std::cerr << "SessionSetVariable result_case() error: " << std::endl;
                rtnv = tgctl::return_code::err;
            }

            if (rtnv == tgctl::return_code::ok) {
                if (monitor_output) {
                    monitor_output->finish(true);
                }
                return rtnv;
            }
        }
    } catch (std::runtime_error &ex) {
        std::cerr << "could not connect to database with name " << tateyama::bootstrap::wire::transport::database_name() << std::endl;
    }
    rtnv = tgctl::return_code::err;

    if (monitor_output) {
        monitor_output->finish(false);
    }
    return rtnv;
}

} //  tateyama::bootstrap::backup
