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

#include <tateyama/framework/boot_mode.h>

#include "configuration.h"
#include "proc_mutex.h"

namespace tateyama::bootstrap {

/**
 * @brief return code
 */
    enum return_code {
        ok = 0,
        err = 1,
    };

    return_code oltp_start(const std::string& argv0, bool need_check, tateyama::framework::boot_mode mode = tateyama::framework::boot_mode::database_server);
    return_code oltp_status();
    return_code oltp_kill(utils::proc_mutex*, utils::bootstrap_configuration&);
    return_code oltp_shutdown_kill(bool force, bool status_output = true);

} //  tateyama::bootstrap


namespace tateyama::bootstrap::backup {

    return_code oltp_backup_create(const std::string& path_to_backup);
    return_code oltp_backup_estimate();
    return_code oltp_restore_backup(const std::string& path_to_backup);
    return_code oltp_restore_backup_use_file_list(const std::string& path_to_backup);
    return_code oltp_restore_tag(const std::string& tag_name);

} //  tateyama::bootstrap::backup
