/*
 * Copyright 2019-2021 tsurugi project.
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

#include <exception>
#include <stdexcept> // std::runtime_error

#include <tateyama/status/resource/core.h>

#include "configuration.h"

namespace tateyama::bootstrap::utils {

class status_info_brigde {
public:
    status_info_brigde() = default;

    explicit status_info_brigde(const std::string& digest) {
        if (!attach(digest)) {
            throw std::runtime_error("can't find shared memory for status_info");
        }
    }
    /**
     * @brief destruct object
     */
    ~status_info_brigde() = default;

    status_info_brigde(status_info_brigde const& other) = delete;
    status_info_brigde& operator=(status_info_brigde const& other) = delete;
    status_info_brigde(status_info_brigde&& other) noexcept = delete;
    status_info_brigde& operator=(status_info_brigde&& other) noexcept = delete;

    bool attach(const std::string& digest) {
        std::string status_file_name = digest;
        status_file_name += ".stat";
        try {
            segment_ = std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::open_only, status_file_name.c_str());
        } catch (const boost::interprocess::interprocess_exception& ex) {
            return false;
        }
        resource_status_memory_ = std::make_unique<tateyama::status_info::resource_status_memory>(*segment_, false);
        return resource_status_memory_.get() != nullptr;
    }

    [[nodiscard]] pid_t pid() const {
        return resource_status_memory_->pid();
    }
    [[nodiscard]] tateyama::status_info::state whole() const {
        return resource_status_memory_->whole();
    }
    [[nodiscard]] bool shutdown() {
        return resource_status_memory_->shutdown();
    }

private:
    std::unique_ptr<boost::interprocess::managed_shared_memory> segment_{};
    std::unique_ptr<tateyama::status_info::resource_status_memory> resource_status_memory_{};
};

} // namespace tateyama::bootstrap::utils
