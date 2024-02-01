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

#include <iostream>
#include <iomanip>

#include <gflags/gflags.h>

#include "tateyama/monitor/monitor.h"
#include "session.h"

DECLARE_string(monitor);  // NOLINT

namespace tateyama::session {

std::vector<session_list_entry> sessoin_list = {
    { "1", "belayer-dump-1", "belayer", "arakawa", "2022-06-20T12:34:56Z", "ipc", "6502" },
    { "2", "example-1", "tgsql", "arakawa", "2022-06-20T12:34:50Z", "tcp", "192.168.1.23:10000" },
    { "3", "example-2", "tgsql", "kurosawa", "2022-06-20T12:34:53Z", "tcp", "192.168.1.78:10000" },
    { "4", "", "", "admin", "2022-06-20T12:34:57Z", "ipc", "32816" },
    { "cafebabe", "load-1", "tgload", "kambayashi", "2022-06-10-T01:23:45", "tcp", "192.168.1.24:32768" }
};
  
tgctl::return_code list() {
    std::unique_ptr<monitor::monitor> monitor_output{};
    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    std::size_t id_max{2}, label_max{5}, application_max{11}, user_max{4}, start_max{5}, type_max{4}, remote_max{6};
    for( auto& e : sessoin_list ) {
        if (id_max < e.id.length()) {
            id_max = e.id.length();
        }
        if (label_max < e.label.length()) {
            label_max = e.label.length();
        }
        if (application_max < e.application.length()) {
            application_max = e.application.length();
        }
        if (user_max < e.user.length()) {
            user_max = e.user.length();
        }
        if (start_max < e.start.length()) {
            start_max = e.start.length();
        }
        if (type_max < e.type.length()) {
            type_max = e.type.length();
        }
        if (remote_max < e.remote.length()) {
            remote_max = e.remote.length();
        }
    }
    id_max += 2;
    label_max += 2;
    application_max += 2;
    user_max += 2;
    start_max += 2;
    type_max += 2;
    remote_max += 2;

    std::cout << std::left;
    std::cout << std::setw(id_max + 1) << "id";
    std::cout << std::setw(label_max) << "label";
    std::cout << std::setw(application_max) << "application";
    std::cout << std::setw(user_max) << "user";
    std::cout << std::setw(start_max) << "start";
    std::cout << std::setw(type_max) << "type";
    std::cout << std::setw(remote_max) << "remote";
    std::cout << std::endl;

    for( auto& e : sessoin_list ) {
        std::cout << ":" << std::setw(id_max) << e.id;
        std::cout << std::setw(label_max) << e.label;
        std::cout << std::setw(application_max) << e.application;
        std::cout << std::setw(user_max) << e.user;
        std::cout << std::setw(start_max) << e.start;
        std::cout << std::setw(type_max) << e.type;
        std::cout << std::setw(remote_max) << e.remote;
        std::cout << std::endl;
        if (monitor_output) {
            monitor_output->session_info(e.id, e.label, e.application, e.user, e.start, e.type, e.remote);
        }
    }
    if (monitor_output) {
        monitor_output->finish(true);
    }
    return tateyama::tgctl::return_code::ok;
}

tgctl::return_code show(std::string_view session_ref) {
    std::unique_ptr<monitor::monitor> monitor_output{};
    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    if (session_ref.empty()) {
        return tateyama::tgctl::return_code::err;
    }
    bool comp_id = (session_ref.at(0) == ':');
    for( auto& e : sessoin_list ) {
        if(comp_id) {
            if (e.id != session_ref.substr(1)) {
                continue;
            }
        } else {
            if (e.label != session_ref) {
                continue;
            }
        }
        std::cout << std::left;
        std::cout << std::setw(13) << "id" << e.id << std::endl;
        std::cout << std::setw(13) << "application" << e.application << std::endl;
        std::cout << std::setw(13) << "label" << e.label << std::endl;
        std::cout << std::setw(13) << "user" << e.user << std::endl;
        std::cout << std::setw(13) << "start" << e.start << std::endl;
        std::cout << std::setw(13) << "type" << e.type << std::endl;
        std::cout << std::setw(13) << "remote" << e.remote << std::endl;
        if (monitor_output) {
            monitor_output->session_info(e.id, e.label, e.application, e.user, e.start, e.type, e.remote);
            monitor_output->finish(true);
        }
        return tateyama::tgctl::return_code::ok;
    }
    if (monitor_output) {
        monitor_output->finish(false);
    }
    return tateyama::tgctl::return_code::err;
}

tgctl::return_code kill(const std::vector<std::string>&& sesstion_refs) {
    std::unique_ptr<monitor::monitor> monitor_output{};
    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    tateyama::tgctl::return_code rv{tateyama::tgctl::return_code::err};

    for (auto& session_ref : sesstion_refs) {
        if (session_ref.empty()) {
            return tateyama::tgctl::return_code::err;
        }
        bool comp_id = (session_ref.at(0) == ':');
        for( auto& e : sessoin_list ) {
            if(comp_id) {
                if (e.id != session_ref.substr(1)) {
                    continue;
                }
            } else {
                if (e.label != session_ref) {
                    continue;
                }
            }
            std::cout << std::left
                      << std::setw(6) << "kill"
                      << std::setw(1) << ":"
                      << std::setw(e.id.length()) << e.id
                      << std::endl;
            rv = tateyama::tgctl::return_code::ok;
        }
    }
    if (monitor_output) {
        monitor_output->finish(rv == tateyama::tgctl::return_code::ok);
    }
    return rv;
}

tgctl::return_code swtch(std::string_view session_ref, std::string_view set_key, std::string_view set_value) {
    std::unique_ptr<monitor::monitor> monitor_output{};
    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    bool comp_id = (session_ref.at(0) == ':');
    for( auto& e : sessoin_list ) {
        if(comp_id) {
            if (e.id != session_ref.substr(1)) {
                continue;
            }
        } else {
            if (e.label != session_ref) {
                continue;
            }
        }
        std::cout << std::left
                  << std::setw(8) << "set"
                  << std::setw(1) << ":"
                  << std::setw(e.id.length()) << e.id
                  << "[" << set_key << "] to " << set_value
                  << std::endl;
        if (monitor_output) {
            monitor_output->finish(true);
        }
        return tateyama::tgctl::return_code::ok;
    }
    if (monitor_output) {
        monitor_output->finish(false);
    }
    return tateyama::tgctl::return_code::err;
}

} //  tateyama::session
