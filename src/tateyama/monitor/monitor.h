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
#include <ctime>
#include <iostream>
#include <fstream>

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
    absent = 0,
    connection = 1,
    server = 2,
    not_found = 3,
    ambiguous = 4,
    permission = 5,
    variable_not_defined = 6,
    variable_invalid_value = 7,

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
    case reason::unknown: return "unknown"sv;
    }
    return "illegal reason"sv;
}

class monitor {
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
    constexpr static std::string_view SESSION_ID = R"("session_id": ":)";
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

public:
    explicit monitor(std::string& file_name) {
        strm_.open(file_name, std::ios_base::out | std::ios_base::trunc);
    }
    ~monitor() {
        strm_.close();
    }

    monitor(monitor const& other) = delete;
    monitor& operator=(monitor const& other) = delete;
    monitor(monitor&& other) noexcept = delete;
    monitor& operator=(monitor&& other) noexcept = delete;

    void start() {
        strm_ << "{ " << TIME_STAMP << time(nullptr)
              << ", " << KIND_START << " }" << std::endl;
        strm_.flush();
    }
    void finish(bool status) {
        if (status) {
            strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
                  << KIND_FINISH << ", " << STATUS << R"(success" })" << std::endl;
        } else {
            strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
                  << KIND_FINISH << ", " << STATUS << R"(failure" })" << std::endl;
        }
        strm_.flush();
    }
    void finish(reason rc) {
        if (rc == reason::absent) {
            strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
                  << KIND_FINISH << ", " << STATUS << R"(success" })" << std::endl;
        } else {
            strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
                  << KIND_FINISH << ", " << STATUS << R"(failure", )"
                  << REASON << to_string_view(rc) << R"(" })" << std::endl;
        }
        strm_.flush();
    }
    void progress(float ratio) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_PROGRESS << ", " << PROGRESS << ratio << " }" << std::endl;
        strm_.flush();
    }
    void status(status stat) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_DATA << ", " << FORMAT_STATUS << ", " << STATUS << to_string_view(stat) << "\" }" << std::endl;
        strm_.flush();
    }
    void session_info(std::string_view session_id,
                      std::string_view label,
                      std::string_view application,
                      std::string_view  user,
                      std::string_view start_at,
                      std::string_view connection_type,
                      std::string_view connection_info) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_DATA << ", " << FORMAT_SESSION_INFO << ", "
              << SESSION_ID << session_id << "\", "
              << LABEL << label << "\", "
              << APPLICATION << application << "\", "
              << USER << user << "\", "
              << START_AT << start_at << "\", "
              << CONNECTION_TYPE << connection_type << "\", "
              << CONNECTION_INFO << connection_info << "\" }" << std::endl;
        strm_.flush();
    }
    void dbstats_description(std::string_view data) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_DATA << ", " << FORMAT_DBSTATS_DESCRIPTION << ", "
              << METRICS << data << "\" }" << std::endl;
        strm_.flush();
    }
    void dbstats(std::string_view data) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_DATA << ", " << FORMAT_DBSTATS << ", "
              << METRICS << data << "\" }" << std::endl;
        strm_.flush();
    }

private:
    std::ofstream strm_;
};

}  // tateyama::bootstrap::utils;
