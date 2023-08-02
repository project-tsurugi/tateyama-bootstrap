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

#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/tgctl/tgctl.h"
#include "proc_mutex.h"

namespace tateyama::process {

    tgctl::return_code tgctl_start(const std::string& argv0, bool need_check, tateyama::framework::boot_mode mode = tateyama::framework::boot_mode::database_server);
    tgctl::return_code tgctl_status();
    tgctl::return_code tgctl_kill(proc_mutex* file_mutex, configuration::bootstrap_configuration& bst_conf);
    tgctl::return_code tgctl_shutdown_kill(bool force, bool status_output = true);
    tgctl::return_code tgctl_diagnostic();
    tgctl::return_code tgctl_pid();

} //  tateyama::process
