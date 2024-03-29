/*
 * Copyright 2019-2023 Project Tsurugi.
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

#include "gflags/gflags.h"
#include "jogasaki/api.h"

DECLARE_int32(dump_batch_size);  //NOLINT
DECLARE_int32(load_batch_size);  //NOLINT

namespace jogasaki::utils {

    extern const std::vector<std::string> tables;
    extern const std::vector<std::string> tpch_tables;

    void dump(jogasaki::api::database& tgdb, std::string& location);
    void load(jogasaki::api::database& tgdb, std::string& location);
    void dump_tpch(jogasaki::api::database& tgdb, std::string& location);
    void load_tpch(jogasaki::api::database& tgdb, std::string& location);

}  // jogasaki::utils
