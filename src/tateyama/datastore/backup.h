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

#include "tateyama/tgctl/tgctl.h"

namespace tateyama::datastore {

    tgctl::return_code tgctl_backup_create(const std::string& path_to_backup);
    tgctl::return_code tgctl_backup_estimate();
    tgctl::return_code tgctl_restore_backup(const std::string& path_to_backup);
    tgctl::return_code tgctl_restore_backup_use_file_list(const std::string& path_to_backup);
    tgctl::return_code tgctl_restore_tag(const std::string& tag_name);

} //  tateyama::datastore
