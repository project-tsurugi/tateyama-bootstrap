/*
 * Copyright 2018-2024 Project Tsurugi.
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

#include <string_view>

namespace tateyama::tgctl {

static constexpr std::string_view help_text {  // NOLINT
"usage: tgctl [--version]\n"
"             [--help]\n"
"             [<subcommand> [<args>] [<options>]]\n"
"\n"
"These are common options for various subcommands:\n"
"    --conf (the file name of the configuration) type: string default: \"\"\n"
"    --quiet (do not display command execution results on the console) type: bool default: false\n"
"    --monitor (the file name to which monitoring info. is to be output) type: string default: \"\"\n"
"    --auth (--no-auth when authentication is not used) type: bool default: true\n"
"    --auth_token (authentication token) type: string default: \"\"\n"
"    --credentials (path to credentials.json) type: string default: \"\"\n"
"\n"
"Subcommands:\n"
"  start : start a tsurugidb process up.\n"
"    <args>\n"
"      none\n"
"    <options>\n"
"      --timeout (timeout for tgctl start in second, no timeout control takes place if 0 is specified) type: int32 default: 10\n"
"      --quiesce (invoke in quiesce mode) type: bool default: false\n"
"      --maintenance_server (invoke in maintenance_server mode) type: bool default: false\n"
"    the following options are to specify how to handle tsurugidb log\n"
"      --logtostderr (log messages go to stderr instead of logfiles) type: bool default: false\n"
"      --stderrthreshold (log messages at or above this level are copied to stderr in addition to logfiles.  This flag obsoletes --alsologtostderr.) type: int32 default: 2\n"
"      --minloglevel (Messages logged at a lower level than this don't actually get logged anywhere) type: int32 default: 0\n"
"      --log_dir (If specified, logfiles are written into this directory instead of the default logging directory.) type: string default: \"\"\n"
"      --max_log_size (approx. maximum log file size (in MB). A value of 0 will be silently overridden to 1.) type: int32 default: 1800\n"
"      --v (Show all VLOG(m) messages for m <= this. Overridable by --vmodule.)  type: int32 default: 0\n"
"      --logbuflevel (Buffer log messages logged at this level or lower (-1 meanss don't buffer; 0 means buffer INFO only; ...)) type: int32 default: 0\n"
"\n"
"  shutdown : shutdown the tsurugidb.\n"
"    <args>\n"
"      none\n"
"    <options>\n"
"      --timeout (timeout for tgctl shutdown in second, no timeout control takes place if 0 is specified) type: int32 default: 300\n"
"      --forceful (forceful shutdown) type: bool default: false\n"
"      --graceful (graceful shutdown) type: bool default: false\n"
"\n"
"  kill : kill the tsurugidb process.\n"
"    <args>\n"
"      none\n"
"    <options>\n"
"      --timeout (timeout for tgctl kill in second, no timeout control takes place if 0 is specified) type: int32 default: 10\n"
"\n"
"  status : inquire the operating status of tsurugidb\n"
"    <args>\n"
"      none\n"
"    <options>\n"
"      none\n"
"\n"
"  backup create : create a backup of the database\n"
"    <args>\n"
"      path : backup directory\n"
"    <options>\n"
"      --label (label for this operation) type: string default: \"\"\n"
"\n"
"  restore backup : restore database from the backup\n"
"    <args>\n"
"      path : backup directory\n"
"    <options>\n"
"      --keep_backup (backup files will be kept) type: bool default: true\n"
"\n"
"  session list\n"
"    <args>\n"
"      none\n"
"    <options>\n"
"      --id (show session list using session id) type: bool default: false\n"
"      --verbose (show session list in verbose format) type: bool default: false\n"
"\n"
"  session show : display information about a specific session\n"
"    <args>\n"
"        session-ref : ID or session label of the target session\n"
"    <options>\n"
"        none\n"
"\n"
"  session shutdown : shuddown a specific session forcefully\n"
"    <args>\n"
"        session-ref [session-ref [...]] : IDs or session labels of the target sessions\n"
"    <options>\n"
"        --forceful (forceful shutdown) type: bool default: false\n"
"        --graceful (graceful shutdown) type: bool default: false\n"
"\n"
"  session set : change the value of a specific session variable\n"
"    <args>\n"
"        session-ref : ID or session label of the target session\n"
"        variable-name : name of the target session variable\n"
"        variable-value : value to be set for the target session variable\n"
"    <options>\n"
"        none\n"
"\n"
"  dbstats show : display Tsurugi internal metrics information\n"
"    <args>\n"
"        none\n"
"    <options>\n"
"        --format (metrics information display format) type: string default: \"json\"\n"
"\n"
"  dbstats list : display a list of available Tsurugi metrics items and their descriptions\n"
"    <args>\n"
"        none\n"
"    <options>\n"
"        --format (metrics information display format) type: string default: \"json\"\n"
};

} // tateyama::tgctl
