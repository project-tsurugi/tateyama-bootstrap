/*
 * Copyright 2022-2023 Project Tsurugi.
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
#include <sstream>

#include <boost/property_tree/ptree.hpp>
#define BOOST_BIND_GLOBAL_PLACEHOLDERS  // to retain the current behavior
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem/operations.hpp>

#include "tateyama/process/process.h"
#include "tateyama/version/version.h"

#include "tateyama/tgctl/tgctl.h"

namespace tateyama::version {

static constexpr std::string_view info_file_name_string = "tsurugi-info.json";

static tgctl::return_code do_show_version(std::istream& stream, std::ostream& cout = std::cout)
{
    boost::property_tree::ptree pt;
    try {
        boost::property_tree::read_json(stream, pt);

        auto name_opt = pt.get_optional<std::string>("name");
        auto version_opt = pt.get_optional<std::string>("version");
        auto date_opt = pt.get_optional<std::string>("date");

        if (name_opt && version_opt && date_opt) {
            // name
            cout << name_opt.value() << std::endl;
            // version
            cout << "version: " << version_opt.value() << std::endl;
            // date
            cout << "date: " << date_opt.value() << std::endl;
            return tgctl::return_code::ok;
        }
        std::cerr << "json is incorrect to identify version" << std::endl;
        return tgctl::return_code::err;
    } catch (boost::property_tree::json_parser_error &e) {
        std::cerr << "parse error : " << e.what() << std::endl;
    }
    return tgctl::return_code::err;
}

static tgctl::return_code show_version(const std::string& argv0)
{
    auto base_path = tateyama::process::get_base_path(argv0);
    std::string info_file_name(info_file_name_string);
    auto info_file_path = base_path / boost::filesystem::path("lib") / boost::filesystem::path(info_file_name);

    if (boost::filesystem::exists(info_file_path) && !boost::filesystem::is_directory(info_file_path)) {
        std::ifstream stream{};
        stream = std::ifstream{info_file_path.c_str()};
        return do_show_version(stream);
    }
    std::cerr << "can't find " << info_file_path.string() << std::endl;
    return tgctl::return_code::err;
}

} // namespace tateyama::version
