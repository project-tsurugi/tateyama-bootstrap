/*
 * Copyright 2022-2022 tsurugi project.
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

#include <time.h>
#include <iostream>
#include <fstream>

namespace tateyama::bootstrap::utils {

enum class status : std::int64_t {
    stop = 0,
    starting = 1,
    running = 2,
    shutdown = 3,

    disconnected = -1,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
[[nodiscard]] constexpr inline std::string_view to_string_view(status value) noexcept {
    using namespace std::string_view_literals;
    switch (value) {
        case status::stop: return "stop"sv;
        case status::starting: return "starting"sv;
        case status::running: return "running"sv;
        case status::shutdown: return "shutdown"sv;
        case status::disconnected: return "disconnected"sv;
    }
    std::abort();
}

class monitor {
    constexpr static std::string_view TIME_STAMP = "\"timestamp\": ";
    constexpr static std::string_view KIND_START = "\"kind\": \"start\"";
    constexpr static std::string_view KIND_FINISH = "\"kind\": \"finish\"";
    constexpr static std::string_view KIND_PROGRESS = "\"kind\": \"progress\"";
    constexpr static std::string_view KIND_DATA = "\"kind\": \"data\"";
    constexpr static std::string_view PROGRESS = "\"progress\": ";
    constexpr static std::string_view FORMAT_STATUS = "\"format\" : \"status\"";
    constexpr static std::string_view STATUS = "\"status\": \"";
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
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_FINISH << ", " << STATUS << (status ? "success" : "failure" ) << "\" }" << std::endl;
        strm_.flush();
    }
    void progress(float r) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_PROGRESS << ", " << PROGRESS << r << " }" << std::endl;
        strm_.flush();
    }
    void status(status s) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_DATA << ", " << FORMAT_STATUS << ", " << STATUS << to_string_view(s) << "\" }" << std::endl;
        strm_.flush();
    }

private:
    std::ofstream strm_;
};

}  // tateyama::bootstrap::utils;
