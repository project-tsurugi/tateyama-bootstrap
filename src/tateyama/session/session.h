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

#include <string>
#include <string_view>
#include <vector>

#include "tateyama/tgctl/tgctl.h"

namespace tateyama::session {

    tgctl::return_code list();
    tgctl::return_code show(std::string_view session_specifier);
    tgctl::return_code kill(const std::vector<std::string>&& sesstion_refs);
    tgctl::return_code swtch(std::string_view session_ref, std::string_view switch_key, std::string_view switch_value);

    struct session_list_entry {
        std::string id;
        std::string label;
        std::string application;
        std::string user;
        std::string start;
        std::string type;
        std::string remote;
    };

} //  tateyama::session
