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

#include <tateyama/framework/server.h>
#include <tateyama/datastore/resource/bridge.h>
#include <tateyama/datastore/service/bridge.h>

namespace tateyama::bootstrap {

void restore_backup([[maybe_unused]] framework::server& sv, std::string_view name, bool keep) {
    VLOG(log_trace) << "name = " << name << ", keep = " << (keep ? "true" : "false");

    auto svc = sv.find_service<tateyama::datastore::service::bridge>();
    if(! svc) {
        LOG(ERROR) << "datastore service not found";
        return;
    }

    auto ds = sv.find_resource<tateyama::datastore::resource::bridge>();
    if(! ds) {
        LOG(ERROR) << "datastore resource not found";
        return;
    }
    ds->restore_backup(name, keep);
}

void restore_tag([[maybe_unused]] framework::server& sv, std::string_view tag) {
    std::cout << __func__ << ": tag = " << tag << std::endl;  // for test

    auto svc = sv.find_service<tateyama::datastore::service::bridge>();
    if(! svc) {
        LOG(ERROR) << "datastore service not found";
        return;
    }

    auto ds = sv.find_resource<tateyama::datastore::resource::bridge>();
    if(! ds) {
        LOG(ERROR) << "datastore resource not found";
        return;
    }
    // FIXME use bridge to process restore_tag
}

}  // tateyama::bootstrap {
