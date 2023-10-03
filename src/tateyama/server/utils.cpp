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
#include "utils.h"

#include <vector>
#include <fstream>
#include <string>

#include "gflags/gflags.h"

DEFINE_int32(dump_batch_size, 1024, "Batch size for dump");  //NOLINT
DEFINE_int32(load_batch_size, 1024, "Batch size for load");  //NOLINT

namespace jogasaki::utils {

    const std::vector<std::string> tables = {  // NOLINT
        "WAREHOUSE",
        "DISTRICT",
        "CUSTOMER",
        "CUSTOMER_SECONDARY",
        "NEW_ORDER",
        "ORDERS",
        "ORDERS_SECONDARY",
        "ORDER_LINE",
        "ITEM",
        "STOCK"
    };

    std::filesystem::path prepare(const std::string& location) {
        std::filesystem::path dir(location);
        dir = dir / "dump";
        if (!std::filesystem::exists(dir)) {
            if (!std::filesystem::create_directories(dir)) {
                throw std::ios_base::failure("Failed to create directory.");
            }
        }
        return dir;
    }

    void
    dump([[maybe_unused]] jogasaki::api::database& tgdb, std::string &location)
    {
        std::filesystem::path dir = prepare(location);
        for (auto& table : tables) {
            std::ofstream ofs((dir / (table+".tbldmp")).c_str());
            if (ofs.fail()) {
                throw std::ios_base::failure("Failed to open file.");
            }
            tgdb.dump(ofs, table, FLAGS_dump_batch_size);
        }
    }

    void
    load([[maybe_unused]] jogasaki::api::database& tgdb, std::string &location)
    {
        std::filesystem::path dir = prepare(location);
        for (auto& table : tables) {
            std::ifstream ifs((dir / (table+".tbldmp")).c_str());
            if (ifs.fail()) {
                throw std::ios_base::failure("Failed to open file.");
            }
            tgdb.load(ifs, table,  FLAGS_load_batch_size);
        }
    }


    const std::vector<std::string> tpch_tables = {  // NOLINT
        "PART",
        "SUPPLIER",
        "PARTSUPP",
        "CUSTOMER",
        "ORDERS",
        "LINEITEM",
        "NATION",
        "REGION"
    };

    void
    dump_tpch(jogasaki::api::database& tgdb, std::string &location)
    {
        std::filesystem::path dir = prepare(location);
        for (auto& table : tpch_tables) {
            std::ofstream ofs((dir / (table+".tbldmp")).c_str());
            if (ofs.fail()) {
                throw std::ios_base::failure("Failed to open file.");
            }
            tgdb.dump(ofs, table, FLAGS_dump_batch_size);
        }
    }

    void
    load_tpch(jogasaki::api::database& tgdb, std::string &location)
    {
        std::filesystem::path dir = prepare(location);
        for (auto& table : tpch_tables) {
            std::ifstream ifs((dir / (table+".tbldmp")).c_str());
            if (ifs.fail()) {
                throw std::ios_base::failure("Failed to open file.");
            }
            tgdb.load(ifs, table, FLAGS_load_batch_size);
        }
    }

}  // jogasaki::utils
