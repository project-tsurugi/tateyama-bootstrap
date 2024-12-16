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

void monitor::request_list(std::size_t session_id,
                           std::size_t request_id,
                           std::size_t service_id,
                           std::size_t payload_size,
                           std::size_t elapsed_time) {
    strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
          << KIND_DATA << ", " << FORMAT_REQUEST_LIST << ", "
          << SESSION_ID << session_id << ", "
          << REQUEST_ID << request_id << ", "
          << SERVICE_ID << service_id << ", "
          << PAYLOAD_SIZE << payload_size << ", "
          << ELAPSED_TIME << elapsed_time
          << " }" << std::endl;
    strm_.flush();
}

void monitor::request_payload(std::string_view payload) {
    strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
          << KIND_DATA << ", " << FORMAT_REQUEST_PAYLOAD << ", "
          << PAYLOAD << payload
          << "\" }" << std::endl;
    strm_.flush();
}

void monitor::request_extract_sql(const std::optional<std::string>& transacion_id, const std::optional<std::string>& sql) {
    strm_ << "{ " << TIME_STAMP << time(nullptr) << ", "
          << KIND_DATA << ", " << FORMAT_REQUEST_EXTRACT_SQL;
    if (transacion_id) {
        strm_ << ", " << TRANSACTION_ID << transacion_id.value() << "\"";
    }
    if (sql) {
        strm_ << ", " << SQL << sql.value() << "\", " << TYPE << to_string_view(identify_sql_type(sql.value())) << "\"";;
    }
    strm_ << " }" << std::endl;
    strm_.flush();
}

}  // tateyama::monitor
