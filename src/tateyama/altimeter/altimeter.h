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

namespace tateyama::altimeter {

    tgctl::return_code set_enabled(const std::string&, bool);
    tgctl::return_code set_log_level(const std::string&, const std::string&);
    tgctl::return_code set_statement_duration(const std::string&);
    tgctl::return_code rotate(const std::string&);

} //  tateyama::altimeter
