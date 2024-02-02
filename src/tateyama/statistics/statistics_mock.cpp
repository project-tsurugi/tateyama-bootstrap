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

#define BOOST_BIND_GLOBAL_PLACEHOLDERS  // to retain the current behavior
#include <boost/property_tree/json_parser.hpp>
#include <gflags/gflags.h>

#include "tateyama/monitor/monitor.h"
#include "statistics.h"

DECLARE_string(monitor);  // NOLINT
DECLARE_string(format);   // NOLINT

namespace tateyama::statistics {
using namespace std::string_view_literals;

static constexpr std::string_view list_result =
    "{\n"
    "  \"session_count\": \"number of active sessions\",\n"
    "  \"table_count\": \"number of user tables\",\n"
    "  \"index_size\": \"estimated each index size\",\n"
    "  \"storage_log_size\": \"transaction log disk usage\",\n"
    "  \"ipc_buffer_size\": \"allocated buffer size for all IPC sessions\",\n"
    "  \"sql_buffer_size\": \"allocated buffer size for SQL execution engine\"\n"
    "}\n"sv;

static constexpr std::string_view show_result =
    "{\n"
    "  \"session_count\": 100,\n"
    "  \"table_count\": 3,\n"
    "  \"index_size\":  [\n"
    "    {\n"
    "        \"table_name\": \"A\",\n"
    "        \"index_name\": \"IA1\",\n"
    "        \"value\": 65536\n"
    "    },\n"
    "    {\n"
    "        \"table_name\": \"A\",\n"
    "        \"index_name\": \"IA2\",\n"
    "        \"value\": 16777216\n"
    "    },\n"
    "    {\n"
    "        \"table_name\": \"B\",\n"
    "        \"index_name\": \"IB1\",\n"
    "        \"value\": 256\n"
    "    }\n"
    "  ],\n"
    "  \"storage_log_size\": 65535,\n"
    "  \"ipc_buffer_size\": 1024,\n"
    "  \"sql_buffer_size\": 268435456\n"
    "}\n"sv;

static std::string replaceString(const std::string& target,
                                 const std::string& from,
                                 const std::string& to) {
    std::string result = target;
    std::string::size_type pos = 0;
    while(pos = result.find(from, pos), pos != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

static std::string escape(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    return replaceString(s, "\"", "\\\"");
}

tgctl::return_code list() {
    std::unique_ptr<monitor::monitor> monitor_output{};
    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    if (FLAGS_format == "json") {
        std::cout << list_result;
        std::cout << std::flush;
        if (monitor_output) {
            std::stringstream si{std::string(list_result)};
            boost::property_tree::ptree pt{};
            boost::property_tree::read_json(si, pt);
            std::stringstream so{};
            boost::property_tree::write_json(so, pt, false);
            monitor_output->dbstats_description(escape(so.str()));
            monitor_output->finish(true);
        }
        return tateyama::tgctl::return_code::ok;
    }
    std::cerr << "format " << FLAGS_format << " is not supported" << std::endl;
    if (monitor_output) {
        monitor_output->finish(false);
    }
    return tateyama::tgctl::return_code::err;
}

tgctl::return_code show() {
    std::unique_ptr<monitor::monitor> monitor_output{};
    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    if (FLAGS_format == "json") {
        std::cout << show_result;
        std::cout << std::flush;
        if (monitor_output) {
            std::stringstream si{std::string(show_result)};
            boost::property_tree::ptree pt{};
            boost::property_tree::read_json(si, pt);
            std::stringstream so{};
            boost::property_tree::write_json(so, pt, false);
            std::string s = so.str();
            monitor_output->dbstats(escape(so.str()));
            monitor_output->finish(true);
        }
        return tateyama::tgctl::return_code::ok;
    }
    std::cerr << "format " << FLAGS_format << " is not supported" << std::endl;
    if (monitor_output) {
        monitor_output->finish(false);
    }
    return tateyama::tgctl::return_code::err;
}

} //  tateyama::statistics
