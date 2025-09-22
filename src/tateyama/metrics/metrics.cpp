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

#include <iostream>
#include <memory>
#include <string_view>
#include <sstream>
#include <cmath>

#define BOOST_BIND_GLOBAL_PLACEHOLDERS  // to retain the current behavior
#include <boost/property_tree/json_parser.hpp>
#include <gflags/gflags.h>

#include <tateyama/proto/metrics/request.pb.h>
#include <tateyama/proto/metrics/response.pb.h>

#include "tateyama/authentication/authentication.h"
#include "tateyama/transport/transport.h"
#include "tateyama/monitor/monitor.h"
#include "tateyama/tgctl/runtime_error.h"

#include "metrics.h"

DECLARE_string(monitor);  // NOLINT
DECLARE_string(format);   // NOLINT

namespace tateyama::metrics {
using namespace std::string_view_literals;

tgctl::return_code list() {
    std::unique_ptr<monitor::monitor> monitor_output{};
    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto reason = monitor::reason::absent;
    try {
        authentication::auth_options();
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_metrics);
        ::tateyama::proto::metrics::request::Request request{};
        request.mutable_list();
        auto response_opt = transport->send<::tateyama::proto::metrics::response::MetricsInformation>(request);
        request.clear_list();

        if (response_opt) {
            auto&& info_list = response_opt.value();

            if (FLAGS_format == "json") {
                std::cout << "{\n" << std::flush;
                int len = info_list.items_size();
                for (int i = 0; i < len; i++) {
                    auto&& item = info_list.items(i);
                    std::cout << "  \"" << item.key() << "\": \"" << item.description() << "\"";
                    if (i == (len - 1)) {
                        std::cout << "\n" << std::flush;
                    } else {
                        std::cout << ",\n" << std::flush;
                    }
                }
                std::cout << "}\n" << std::flush;

                if (monitor_output) {
                    monitor_output->finish(monitor::reason::absent);
                }
                return tateyama::tgctl::return_code::ok;
            }
            if (FLAGS_format == "text") {
                std::size_t key_max{0};
                std::size_t description_max{0};
                int len = info_list.items_size();
                for (int i = 0; i < len; i++) {
                    auto&& item = info_list.items(i);

                    key_max = std::max(key_max, item.key().length());
                    description_max = std::max(description_max, item.description().length());
                }

                std::string prev_key{};
                for (int i = 0; i < len; i++) {
                    auto&& item = info_list.items(i);
                    if (prev_key != item.key()) {
                        std::cout << std::right;
                        std::cout << std::setw(static_cast<int>(key_max)) << item.key();
                        std::cout << " : ";
                        std::cout << std::left;
                        std::cout << std::setw(static_cast<int>(description_max)) << item.description();
                        std::cout << '\n' << std::flush;
                        prev_key = item.key();
                    }
                }

                if (monitor_output) {
                    monitor_output->finish(monitor::reason::absent);
                }
                return tateyama::tgctl::return_code::ok;
            }
            std::cerr << "format " << FLAGS_format << " is not supported\n" << std::flush;
            reason = monitor::reason::invalid_argument;
        } else {
            std::cerr << "could not receive a valid response\n" << std::flush;
            reason = monitor::reason::payload_broken;
        }
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'\n" << std::flush;
        reason = ex.code();
    }
    
    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return tgctl::return_code::err;
}

tgctl::return_code show() {  // NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};
    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto reason = monitor::reason::absent;
    try {
        authentication::auth_options();
        auto transport = std::make_unique<tateyama::bootstrap::wire::transport>(tateyama::framework::service_id_metrics);
        ::tateyama::proto::metrics::request::Request request{};
        request.mutable_show();
        auto response_opt = transport->send<::tateyama::proto::metrics::response::MetricsInformation>(request);
        request.clear_show();

        if (response_opt) {
            auto&& info_show = response_opt.value();
            std::string separator{};

            if (FLAGS_format == "json") {
                std::cout << "{\n" << std::flush;
                int len = info_show.items_size();
                for (int i = 0; i < len; i++) {
                    auto&& item = info_show.items(i);
                    auto&& key = item.key();
                    auto&& value = item.value();
                    if (0 < i && i < len) {
                        std::cout << ",\n" << std::flush;
                    }
                    if (value.value_or_array_case() == ::tateyama::proto::metrics::response::MetricsValue::ValueOrArrayCase::kArray) {
                        auto&& array = value.array();
                        int alen = array.elements_size();
                        std::cout << "  \"" << item.key() << "\": [\n" << std::flush;
                        separator = "  ]";
                        for (int j = 0; j < alen; j++) {
                            auto&& element = array.elements(j);
                            std::cout << "    {\n" << std::flush;
                            auto&& attributes = element.attributes();
                            for (auto&& itr = attributes.cbegin(); itr != attributes.cend(); itr++) {
                                std::cout << "      \""  << itr->first << "\": \"" << itr->second << "\",\n" << std::flush;
                            }
                            if (element.value() > 1.0) {
                                std::cout.precision(0);
                            } else {
                                std::cout.precision(6);
                            }
                            std::cout << "      \"value\": " << std::fixed << element.value() << '\n' << std::flush;
                            std::cout << ((j < (alen - 1)) ? "    }," : "    }" ) << '\n' << std::flush;
                        }
                    } else {
                        auto v = value.value();
                        if (v - static_cast<double>(static_cast<int>(v)) > 0.0) {
                            std::cout.precision(6);
                        } else {
                            std::cout.precision(0);
                        }
                        std::cout << "  \"" << key << "\": " << std::fixed << value.value();
                    }
                    if (!separator.empty()) {
                        std::cout << separator;
                        separator = "";
                    }
                }
                std::cout << "\n}\n" << std::flush;

                if (monitor_output) {
                    monitor_output->finish(monitor::reason::absent);
                }

                return tateyama::tgctl::return_code::ok;
            }
            if (FLAGS_format == "text") {
                std::cerr << "human readable format has not been supported\n" << std::flush;
                reason = monitor::reason::invalid_argument;
            } else {
                std::cerr << "format " << FLAGS_format << " is not supported\n" << std::flush;
                reason = monitor::reason::invalid_argument;
            }
        } else {
            std::cerr << "could not receive a valid response\n" << std::flush;
            reason = monitor::reason::payload_broken;
        }
    } catch (tgctl::runtime_error &ex) {
        std::cerr << "could not connect to database with name '" << tateyama::bootstrap::wire::transport::database_name() << "'\n" << std::flush;
        reason = ex.code();
    }

    if (monitor_output) {
        monitor_output->finish(reason);
    }
    return tgctl::return_code::err;
}

} //  tateyama::metrics
