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

#include "configuration/configuration.h"
#include "proc_mutex.h"
#include "oltp/oltp.h"

namespace tateyama::process {

    oltp::return_code oltp_start(const std::string& argv0, bool need_check, tateyama::framework::boot_mode mode = tateyama::framework::boot_mode::database_server);
    oltp::return_code oltp_status();
    oltp::return_code oltp_kill(proc_mutex*, configuration::bootstrap_configuration&);
    oltp::return_code oltp_shutdown_kill(bool force, bool status_output = true);
    oltp::return_code oltp_diagnostic();
    oltp::return_code oltp_pid();

} //  tateyama::process
