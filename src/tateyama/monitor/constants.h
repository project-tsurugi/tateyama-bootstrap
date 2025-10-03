/*
 * Copyright 2022-2025 Project Tsurugi.
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
#pragma once

#include <cstdint>

namespace tateyama::monitor {

enum class status : std::int64_t {
    stop = 0,
    ready = 1,
    activated = 2,
    deactivating = 3,
    deactivated = 4,
    boot_error = 10,

    unknown = -1,
};

enum class reason : std::int64_t {
    absent = 0,  // no error

    // old type reason
    connection = 1,
    not_found = 2,
    ambiguous = 3,
    permission = 4,
    variable_not_defined = 5,
    variable_invalid_value = 6,

    // general
    authentication_failure = 11,
    connection_timeout = 12,
    connection_failure = 13,
    io = 14,
    server = 15,
    interrupted = 16,
    internal = 17,

    // request
    request_missing = 20,
    payload_broken = 21,
    sql_missing = 22,
    sql_unresolved = 23,

    // process control, etc.
    initialization = 30,
    another_process = 31,
    invalid_status = 32,
    invalid_argument = 33,
    timeout = 34,

    unknown = -1,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
[[nodiscard]] constexpr inline std::string_view to_string_view(status value) noexcept {
    using namespace std::string_view_literals;
    switch (value) {
    case status::stop:return "stop"sv;
    case status::ready: return "starting"sv;
    case status::activated: return "running"sv;
    case status::deactivating: return "shutdown"sv;
    case status::deactivated: return "shutdown"sv;
    case status::boot_error: return "boot_error"sv;
    case status::unknown: return "disconnected"sv;
    }
    return "illegal state"sv;
}

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
[[nodiscard]] constexpr inline std::string_view to_string_view(reason value) noexcept {
    using namespace std::string_view_literals;
    switch (value) {
    case reason::absent:return "absent"sv;
    case reason::connection:return "connection"sv;
    case reason::server:return "server"sv;
    case reason::not_found:return "not_found"sv;
    case reason::ambiguous:return "ambiguous"sv;
    case reason::permission:return "permission"sv;
    case reason::variable_not_defined:return "variable_not_defined"sv;
    case reason::variable_invalid_value:return "variable_invalid_value"sv;

    // general
    case reason::authentication_failure: return "authentication_failure"sv;
    case reason::connection_timeout: return "connection_timeout"sv;
    case reason::connection_failure: return "connection_failure"sv;
    case reason::io: return "io"sv;
    case reason::interrupted: return "interrupted"sv;
    case reason::internal: return "internal"sv;

    // request
    case reason::request_missing: return "request_missing"sv;
    case reason::payload_broken: return "payload_broken"sv;
    case reason::sql_missing: return "sql_missing"sv;
    case reason::sql_unresolved: return "sql_unresolved"sv;

    // process control
    case reason::initialization: return "initialization"sv;
    case reason::another_process: return "another_process"sv;
    case reason::invalid_status: return "invalid_status"sv;
    case reason::invalid_argument: return "invalid_argument"sv;
    case reason::timeout: return "timeout"sv;

    case reason::unknown: return "unknown"sv;
    }
    return "illegal reason"sv;
}

constexpr static std::string_view TIME_STAMP = R"("timestamp": )";
constexpr static std::string_view KIND_START = R"("kind": "start")";
constexpr static std::string_view KIND_FINISH = R"("kind": "finish")";
constexpr static std::string_view KIND_PROGRESS = R"("kind": "progress")";
constexpr static std::string_view KIND_DATA = R"("kind": "data")";
constexpr static std::string_view PROGRESS = R"("progress": )";
// status
constexpr static std::string_view FORMAT_STATUS = R"("format": "status")";
constexpr static std::string_view STATUS = R"("status": ")";
constexpr static std::string_view REASON = R"("reason": ")";
// session info
constexpr static std::string_view FORMAT_SESSION_INFO = R"("format": "session-info")";
constexpr static std::string_view SESSION_ID = R"("session_id": )";
constexpr static std::string_view LABEL = R"("label": ")";
constexpr static std::string_view APPLICATION = R"("application": ")";
constexpr static std::string_view USER = R"("user": ")";
constexpr static std::string_view START_AT = R"("start_at": ")";
constexpr static std::string_view CONNECTION_TYPE = R"("connection_type": ")";
constexpr static std::string_view CONNECTION_INFO = R"("connection_info": ")";
// dbstats
constexpr static std::string_view FORMAT_DBSTATS_DESCRIPTION = R"("format": "dbstats_description")";
constexpr static std::string_view FORMAT_DBSTATS = R"("format": "dbstats")";
constexpr static std::string_view METRICS = R"("metrics": ")";
// config
constexpr static std::string_view FORMAT_CONFIG = R"("format": "config")";
constexpr static std::string_view SECTION = R"("section": ")";
constexpr static std::string_view KEY = R"("key": ")";
constexpr static std::string_view VALUE = R"("value": ")";

}  // tateyama::monitor
