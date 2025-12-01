/*
 * Copyright 2018-2025 Project Tsurugi.
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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <optional>
#include <map>
#include <set>

#include <gflags/gflags.h>
#include <nlohmann/json.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <tateyama/logging.h>

#include "tateyama/authentication/authenticator.h"
#include "tateyama/monitor/monitor.h"
#include "bootstrap_configuration.h"

DECLARE_string(conf);
DECLARE_bool(quiet);
DECLARE_string(monitor);
DEFINE_bool(show_dev, false, "include settings for developers");  // NOLINT

namespace tateyama::configuration {

tgctl::return_code config() {  // NOLINT(readability-function-cognitive-complexity)
    std::unique_ptr<monitor::monitor> monitor_output{};

    if (!FLAGS_monitor.empty()) {
        monitor_output = std::make_unique<monitor::monitor>(FLAGS_monitor);
        monitor_output->start();
    }

    auto bootstrap_configuration = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
    if (!bootstrap_configuration.valid()) {
        auto reason = tateyama::monitor::reason::not_found;
        std::cerr << "error: reason = " << to_string_view(reason) << ", detail = 'cannot find the configuration file'\n" << std::flush;
        if (monitor_output) {
            monitor_output->finish(reason);
        }
        return tateyama::tgctl::return_code::err;
    }
    tateyama::authentication::authenticator authenticator{};
    try {
        authenticator.authenticate(bootstrap_configuration.get_configuration()->get_section("authentication"));
    } catch (tgctl::runtime_error &ex) {
        auto reason = ex.code();
        std::cerr << "error: reason = " << to_string_view(reason) << ", detail = '" << ex.what() << "'\n" << std::flush;
        if (monitor_output) {
            monitor_output->finish(reason);
        }
        return tateyama::tgctl::return_code::err;
    }

    boost::property_tree::ptree config_tree;
    boost::property_tree::ptree default_tree;
    std::map<std::string, std::set<std::string>> attributes{};

    std::string default_configuration_string{default_configuration()};
    std::istringstream default_iss(default_configuration_string);
    boost::property_tree::read_ini(default_iss, default_tree);

    BOOST_FOREACH(const boost::property_tree::ptree::value_type &v1, default_tree) {
        auto& set = attributes[v1.first];
        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v2, v1.second) {
            auto& name = v2.first;
            if (FLAGS_show_dev || (name.substr(0, 4) != "dev_")) {
                set.emplace(name);
            }
        }
    }

    auto configuration_file = bootstrap_configuration.conf_file();
    std::ifstream stream{};
    if (std::filesystem::exists(configuration_file)) {
        auto content = std::ifstream{configuration_file.c_str()};
        boost::property_tree::read_ini(content, config_tree);

        BOOST_FOREACH(const boost::property_tree::ptree::value_type &v1, config_tree) {
            auto& set = attributes[v1.first];
            BOOST_FOREACH(const boost::property_tree::ptree::value_type &v2, v1.second) {
                set.emplace(v2.first);
            }
        }
    }

    auto conf = bootstrap_configuration.get_configuration();
    for (auto& em: attributes) {
        auto section = conf->get_section(em.first);
        if (!FLAGS_quiet) {
            std::cout << "[" << em.first << "]\n";
        }
        for (auto& es : em.second) {
            if (auto opt = section->get<std::string>(es); opt) {
                if (!FLAGS_quiet) {
                    std::cout << "    " << es << "=" << opt.value() << "\n";
                }
                if (monitor_output) {
                    monitor_output->config_item(em.first, es, opt.value());
                }
            }
        }
    }

    if (monitor_output) {
        monitor_output->finish(tateyama::monitor::reason::absent);
    }
    return tgctl::return_code::ok;
};

} // tateyama::api::configuration
