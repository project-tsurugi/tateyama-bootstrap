/*
 * Copyright 2018-2024 Project Tsurugi.
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
#include <vector>

#include <tateyama/api/configuration.h>
#include <altimeter/configuration.h>

namespace tateyama::altimeter {

    class altimeter_helper {
        enum class log_type : std::uint32_t {
            /**
             * @brief event log.
             */
            event_log = 0,

            /**
              * @brief audit log.
              */
            audit_log = 1
        };
        
      public:
        /**
         * @brief default constructer is deleted
         */
        altimeter_helper() = delete;

        /**
         * @brief create a object reflects the configuration
         */
        explicit altimeter_helper(tateyama::api::configuration::whole* conf);

        /**
         * @brief destruct the object
         */
        ~altimeter_helper();

        altimeter_helper(altimeter_helper const& other) = delete;
        altimeter_helper& operator=(altimeter_helper const& other) = delete;
        altimeter_helper(altimeter_helper&& other) noexcept = delete;
        altimeter_helper& operator=(altimeter_helper&& other) noexcept = delete;

        /**
         * @brief start an altimeter logger
         */
        void start();

        /**
         * @brief shutdown the altimeter logger
         */
        void shutdown();

      private:
        tateyama::api::configuration::whole* conf_;
        std::vector<::altimeter::configuration> cfgs_ = {
            ::altimeter::configuration{},  // event_log_cfg
            ::altimeter::configuration{}   // audit_log_cfg
        };
        bool shutdown_{};

        void setup(::altimeter::configuration& configuration, tateyama::api::configuration::section* section, log_type type);
    };

} // tateyama::altimeter
