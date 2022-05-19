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

namespace tateyama::bootstrap {

    int oltp_start(int argc, char* argv[]);
    int oltp_status(int argc, char* argv[]);
    int oltp_shutdown_kill(int argc, char* argv[], bool force);

} //  tateyama::bootstrap

namespace tateyama::bootstrap::backup {

    int oltp_backup_create(int argc,  char* argv[]);
    int oltp_backup_estimate(int argc,  char* argv[]);
    int oltp_restore_backup(int argc,  char* argv[]);
    int oltp_restore_tag(int argc,  char* argv[]);

} //  tateyama::bootstrap::backup