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

#include "configuration/configuration.h"

namespace tateyama::server {

class status_info_bridge {
public:
    status_info_bridge() = default;

    explicit status_info_bridge(const std::string& digest) {
        if (!attach(digest)) {
            throw std::runtime_error("can't find shared memory for status_info");
        }
    }
    /**
     * @brief destruct object
     */
    ~status_info_bridge() = default;

    status_info_bridge(status_info_bridge const& other) = delete;
    status_info_bridge& operator=(status_info_bridge const& other) = delete;
    status_info_bridge(status_info_bridge&& other) noexcept = delete;
    status_info_bridge& operator=(status_info_bridge&& other) noexcept = delete;

    bool attach(const std::string& digest) {
        status_file_name_ = digest;
        status_file_name_ += ".stat";
        try {
            segment_ = std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::open_only, status_file_name_.c_str());
        } catch (const boost::interprocess::interprocess_exception& ex) {
            return false;
        }
        resource_status_memory_ = std::make_unique<tateyama::status_info::resource_status_memory>(*segment_, false);
        return resource_status_memory_->valid();
    }

    [[nodiscard]] pid_t pid() const {
        return resource_status_memory_->pid();
    }
    [[nodiscard]] tateyama::status_info::state whole() const {
        return resource_status_memory_->whole();
    }
    [[nodiscard]] bool is_shutdown_requested() {
        return resource_status_memory_->shutdown();
    }
    [[nodiscard]] bool request_shutdown() {
        return resource_status_memory_->request_shutdown();
    }
    [[nodiscard]] bool alive() {
        return resource_status_memory_->alive();
    }
    void apply_shm_entry(std::function<void(std::string_view)> f) {
        resource_status_memory_->apply_shm_entry(std::move(f));
    }
    void force_delete() {
        if (!status_file_name_.empty()) {
            boost::interprocess::shared_memory_object::remove(status_file_name_.c_str());
        }
    }

private:
    std::unique_ptr<boost::interprocess::managed_shared_memory> segment_{};
    std::unique_ptr<tateyama::status_info::resource_status_memory> resource_status_memory_{};
    std::string status_file_name_{};
};

} // namespace tateyama::server
