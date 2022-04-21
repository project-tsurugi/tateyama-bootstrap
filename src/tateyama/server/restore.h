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

#include <iostream>

namespace tateyama::bootstrap {

    void restore_backup(std::string_view name, bool keep) {
        std::cout << __func__ << ": name = " << name << ", keep = " << (keep ? "true" : "false") << std::endl;  // for test
    }

    void restore_tag(std::string_view tag) {
        std::cout << __func__ << ": tag = " << tag << std::endl;  // for test
    }

}  // tateyama::bootstrap {
