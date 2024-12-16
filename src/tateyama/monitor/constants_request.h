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
#pragma once

#include <cstdint>
#include <strings.h>
#include <string>
#include <regex>

namespace tateyama::monitor {

enum class type_of_sql : std::int64_t {
    begin = 0,
    commit = 1,
    rollback = 2,
    prepare = 3,
    query = 4,
    statement = 5,
    explain = 6,
    dump = 7,
    load = 8,
    others = 99,

    unknown = -1,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
[[nodiscard]] constexpr inline std::string_view to_string_view(type_of_sql value) noexcept {
    using namespace std::string_view_literals;
    switch (value) {
    case type_of_sql::begin:return "begin"sv;
    case type_of_sql::commit:return "commit"sv;
    case type_of_sql::rollback:return "rollback"sv;
    case type_of_sql::prepare:return "prepare"sv;
    case type_of_sql::query:return "query"sv;
    case type_of_sql::statement:return "statement"sv;
    case type_of_sql::explain:return "explain"sv;
    case type_of_sql::dump:return "dump"sv;
    case type_of_sql::load:return "load"sv;
    case type_of_sql::others:return "others"sv;
    case type_of_sql::unknown:return "unknown"sv;
    }
    return "illegal type_of_sql"sv;
}

static inline bool begins_with(const std::string& sql, const std::string& pattern) {
    return (strncasecmp(std::regex_replace(sql, std::regex("^\\s+"), "").c_str(), pattern.c_str(), pattern.length()) == 0);
}

static inline type_of_sql identify_sql_type(const std::string& sql) {
    using namespace std::literals::string_literals;
    if (begins_with(sql, "begin"s)) {
        return type_of_sql::begin;
    }
    if (begins_with(sql, "commit"s)) {
        return type_of_sql::commit;
    }
    if (begins_with(sql, "rollback"s)) {
        return type_of_sql::rollback;
    }
    if (begins_with(sql, "prepare"s)) {
        return type_of_sql::prepare;
    }
    if (begins_with(sql, "select"s)) {
        return type_of_sql::query;
    }
    if (begins_with(sql, "insert"s) ||
        begins_with(sql, "update"s) ||
        begins_with(sql, "delete"s)) {
        return type_of_sql::statement;
    }
    if (begins_with(sql, "explain"s)) {
        return type_of_sql::explain;
    }
    if (begins_with(sql, "dump"s)) {
        return type_of_sql::dump;
    }
    if (begins_with(sql, "load"s)) {
        return type_of_sql::load;
    }
    return type_of_sql::others;
}

// request list
constexpr static std::string_view FORMAT_REQUEST_LIST = R"("format": "request_list")";
constexpr static std::string_view REQUEST_ID = R"("request_id": )";
constexpr static std::string_view SERVICE_ID = R"("service_id": )";
constexpr static std::string_view PAYLOAD_SIZE = R"("payload_size": )";
constexpr static std::string_view ELAPSED_TIME = R"("elapsed_time": )";

// request payload
constexpr static std::string_view FORMAT_REQUEST_PAYLOAD = R"("format": "request_payload")";
constexpr static std::string_view PAYLOAD = R"("payload": ")";

// request extract-sql
constexpr static std::string_view FORMAT_REQUEST_EXTRACT_SQL = R"("format": "request_extract-sql")";
constexpr static std::string_view TYPE = R"("type": ")";
constexpr static std::string_view TRANSACTION_ID = R"("transaction_id": ")";
constexpr static std::string_view SQL = R"("sql": ")";

}  // tateyama::monitor
