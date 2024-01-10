/*
 * Copyright 2022-2023 Project Tsurugi.
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
    std::abort();
}

class monitor {
    constexpr static std::string_view TIME_STAMP = R"("timestamp": )";
    constexpr static std::string_view KIND_START = R"("kind": "start")";
    constexpr static std::string_view KIND_FINISH = R"("kind": "finish")";
    constexpr static std::string_view KIND_PROGRESS = R"("kind": "progress")";
    constexpr static std::string_view KIND_DATA = R"("kind": "data")";
    constexpr static std::string_view PROGRESS = R"("progress": )";
    constexpr static std::string_view FORMAT_STATUS = R"("format" : "status")";
    constexpr static std::string_view STATUS = R"("status": ")";
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
                  << KIND_FINISH << ", " << STATUS << "success\" }" << std::endl;
        } else {
            strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
                  << KIND_FINISH << ", " << STATUS << "failure\" }" << std::endl;
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

private:
    std::ofstream strm_;
};

}  // tateyama::bootstrap::utils;
