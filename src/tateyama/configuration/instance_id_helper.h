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
#pragma once

#include <string>
#include <algorithm>
#include <array>
#include <cstring>
#include <unistd.h>
#include <exception>

#include <tateyama/api/configuration.h>

class instance_id_handler {
    static constexpr std::size_t MAX_INSTANCE_ID_LENGTH = 63;

public:
    static void setup(tateyama::api::configuration::whole* conf) {
        auto* section = conf->get_section("system");
        if (auto instance_id_opt = section->get<std::string>("instance_id"); instance_id_opt) {
            section->set("instance_id", instance_id(instance_id_opt.value()));
        } else {
            throw std::runtime_error("instance_id is not given in tsurugi.ini");
        }
    }

private:
    static std::string instance_id(std::string_view id) {
        using namespace std::literals::string_literals;
        
        if (id.empty()) {
            std::array<char, MAX_INSTANCE_ID_LENGTH> hostname{};
            if (gethostname(hostname.data(), MAX_INSTANCE_ID_LENGTH) == 0) {
                std::string instance_id{hostname.data(), strlen(hostname.data())};
                try {
                    tolower_and_check(instance_id);
                    return instance_id;
                } catch (std::runtime_error &ex) {
                    return "localhost"s;
                }
            }
            return "localhost"s;
        }
        std::string instance_id{id};
        tolower_and_check(instance_id);
        return instance_id;
    }

    static void tolower_and_check(std::string& s) {
        if (s.empty()) {
            throw std::runtime_error("instance_id given is empty string");
        }
        if (s.length() > MAX_INSTANCE_ID_LENGTH) {
            throw std::runtime_error("instance_id is too long");
        }
        if (s.at(0) == '-') {
            throw std::runtime_error("instance_id begins with hyphen");
        }
        if (s.at(s.length() - 1) == '-') {
            throw std::runtime_error("instance_id ends with hyphen");
        }
        std::transform(
            s.begin(), 
            s.end(), 
            s.begin(),
            [](char c) { return std::tolower(c); }
        );
        bool hyphen{false};
        for(auto c: s) {
            if (('a' <= c && c <= 'z') ||
                ('0' <= c && c <= '9')) {
                hyphen = false;
                continue;
            }
            if ('-' == c) {
                if (hyphen) {
                    throw std::runtime_error("instance_id has consecutive hyphens");
                }
                hyphen = true;
                continue;
            }
            throw std::runtime_error("instance_id has illegal character");
        }
    }
};
