/*
 * Copyright 2023-2023 Project Tsurugi.
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

#include <string>
#include <functional>
#include <exception>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>

namespace tateyama::datastore {

class file_list {
public:
    file_list() = default;
    ~file_list() = default;

    file_list(file_list const& other) = default;
    file_list& operator=(file_list const& other) = default;
    file_list(file_list&& other) noexcept = default;
    file_list& operator=(file_list&& other) noexcept = default;

    bool read_json(const std::string& file_name) {
        try {
            boost::property_tree::read_json(file_name, pt_);
            return true;
        } catch (std::exception const& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
    }

    void for_each(const std::function<void(const std::string&, const std::string&, bool)>& func) {
        BOOST_FOREACH (const boost::property_tree::ptree::value_type& child, pt_.get_child("entries")) {
            const boost::property_tree::ptree& info = child.second;

            boost::optional<std::string> source = info.get_optional<std::string>("source_path");
            boost::optional<std::string> destination = info.get_optional<std::string>("destination_path");
            boost::optional<bool> detached = info.get_optional<bool>("detached");

            if (source && destination && detached) {
                func(source.get(), destination.get(), detached.get());
            }
        }
    }

private:
    boost::property_tree::ptree pt_;
};

} // namespace tateyama::bootstrap::utils
