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
#include <iostream>

#include "monitor.h"

namespace tateyama::monitor {

monitor::monitor(std::string& file_name) : strm_(fstrm_), is_filestream_(true) {
    fstrm_.open(file_name, std::ios_base::out | std::ios_base::trunc);
}
monitor::monitor(std::ostream& os) : strm_(os), is_filestream_(false) {
}

monitor::~monitor() {
    if (is_filestream_) {
        fstrm_.close();
    }
}

void monitor::start() {
    strm_ << "{ " << TIME_STAMP << time(nullptr)
          << ", " << KIND_START << " }\n";
    strm_.flush();
}

void monitor::finish(reason rc) {
    if (rc == reason::absent) {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_FINISH << ", " << STATUS << R"(success" })" << '\n';
    } else {
        strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
              << KIND_FINISH << ", " << STATUS << R"(failure", )"
              << REASON << to_string_view(rc) << R"(" })" << '\n';
    }
    strm_.flush();
}

void monitor::progress(float ratio) {
    strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
          << KIND_PROGRESS << ", " << PROGRESS << ratio << " }\n";
    strm_.flush();
}

void monitor::status(tateyama::monitor::status stat) {
    strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
          << KIND_DATA << ", " << FORMAT_STATUS << ", " << STATUS << to_string_view(stat) << "\" }\n";
    strm_.flush();
}

void monitor::session_info(std::string_view session_id,
                           std::string_view label,
                           std::string_view application,
                           std::string_view  user,
                           std::string_view start_at,
                           std::string_view connection_type,
                           std::string_view connection_info) {
    strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
          << KIND_DATA << ", " << FORMAT_SESSION_INFO << ", "
          << SESSION_ID << "\":" << session_id << "\" , "
          << LABEL << label << "\", "
          << APPLICATION << application << "\", "
          << USER << user << "\", "
          << START_AT << start_at << "\", "
          << CONNECTION_TYPE << connection_type << "\", "
          << CONNECTION_INFO << connection_info << "\" }\n";
    strm_.flush();
}

void monitor::dbstats_description(std::string_view data) {
    strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
          << KIND_DATA << ", " << FORMAT_DBSTATS_DESCRIPTION << ", "
          << METRICS << data << "\" }\n";
    strm_.flush();
}

void monitor::dbstats(std::string_view data) {
    strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
          << KIND_DATA << ", " << FORMAT_DBSTATS << ", "
          << METRICS << data << "\" }\n";
    strm_.flush();
}

}  // tateyama::monitor
