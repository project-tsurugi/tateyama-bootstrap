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

class monitor {
    constexpr static std::string_view TIME_STAMP = "\"timestamp\": ";  // NOLINT
    constexpr static std::string_view KIND_START = "\"kind\": \"start\"";  // NOLINT
    constexpr static std::string_view KIND_FINISH = "\"kind\": \"finish\"";  // NOLINT
    constexpr static std::string_view KIND_PROGRESS = "\"kind\": \"progress\"";  // NOLINT
    constexpr static std::string_view STATUS = "\"status\": ";  // NOLINT
    constexpr static std::string_view PROGRESS = "\"progress\": ";  // NOLINT
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
              << KIND_FINISH << ", " << STATUS << (status ? "\"success\"" : "\"failure\"" ) << " }" << std::endl;
        strm_.flush();
    }
    void progress(float r) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_PROGRESS << ", " << PROGRESS << r << " }" << std::endl;
        strm_.flush();
    }

private:
    std::ofstream strm_;
};

}  // tateyama::bootstrap::utils;
