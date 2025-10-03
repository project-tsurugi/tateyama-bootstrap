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

#include <ostream>
#include <fstream>
#include <string>
#include <string_view>
#include <optional>

#include "constants.h"
#include "constants_request.h"

namespace tateyama::monitor {

class monitor {
public:
    explicit monitor(std::string& file_name);
    explicit monitor(std::ostream& os);
    ~monitor();

    monitor(monitor const& other) = delete;
    monitor& operator=(monitor const& other) = delete;
    monitor(monitor&& other) noexcept = delete;
    monitor& operator=(monitor&& other) noexcept = delete;

    void start();
    void finish(reason rc);
    void progress(float ratio);
    void status(status stat);
    void session_info(std::string_view session_id,
                      std::string_view label,
                      std::string_view application,
                      std::string_view user,
                      std::string_view start_at,
                      std::string_view connection_type,
                      std::string_view connection_info);
    void dbstats_description(std::string_view data);
    void dbstats(std::string_view data);

    // request
    void request_list(std::size_t session_id,
                      std::size_t request_id,
                      std::size_t service_id,
                      std::size_t payload_size,
                      std::size_t elapsed_time);
    void request_payload(std::string_view payload);
    void request_extract_sql(const std::optional<std::string>& transacion_id, const std::optional<std::string>& sql);
    
    // config
    void config_item(std::string_view section,
                     std::string_view key,
                     std::string_view value);

private:
    std::ostream& strm_;
    bool is_filestream_;

    std::ofstream fstrm_{};
};

}  // tateyama::monitor
