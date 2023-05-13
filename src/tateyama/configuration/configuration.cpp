/*
 * Copyright 2018-2020 tsurugi project.
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
#include <tateyama/api/configuration.h>

#include <boost/filesystem.hpp>

namespace tateyama::api::configuration {

void whole::initialize(std::istream& content) {
    if (property_file_exist_) {
        try {
            boost::property_tree::read_ini(content, property_tree_);
        } catch (boost::property_tree::ini_parser_error &e) {
            VLOG(log_info) << "error reading input, thus we use default property only. msg:" << e.what();
        }
        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v, property_tree_) {
            try {
                auto& pt = property_tree_.get_child(v.first);
                map_.emplace(v.first, std::make_unique<section>(pt));
            } catch (boost::property_tree::ptree_error &e) {
                VLOG(log_info) << "cannot find " << v.first << " section in the input.";
            }
        }
    }
}

whole::whole(std::string_view file_name) {
    file_ = boost::filesystem::path(std::string(file_name));
    std::ifstream stream{};
    if (boost::filesystem::exists(file_)) {
        property_file_exist_ = true;
        stream = std::ifstream{file_.c_str()};
    } else {
        VLOG(log_info) << "cannot find " << file_name << ", thus we use default property only.";
    }
    initialize(stream);
}

} // tateyama::api::configuration
