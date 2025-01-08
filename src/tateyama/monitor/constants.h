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
    absent,  // no error

        // old type reason
        connection,
        not_found,
        ambiguous,
        permission,
        variable_not_defined,
        variable_invalid_value,

        // general
        authentication_failure,
        connection_timeout,
        connection_failure,
        io,
        server,
        interrupted,
        internal,

        // request
        request_missing,
        payload_broken,
        sql_missing,
        sql_unresolved,
        
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

}  // tateyama::monitor
