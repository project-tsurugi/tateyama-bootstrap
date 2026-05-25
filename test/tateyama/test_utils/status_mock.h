/*
 * Copyright 2019-2024 Project Tsurugi.
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

#include <string>
#include <sstream>
#include <exception>

#include <boost/interprocess/managed_shared_memory.hpp>

#include <tateyama/status/resource/core.h>

#include "tateyama/tgctl/runtime_error.h"

namespace tateyama::test_utils {

class resource_status_adapter {
public:
    resource_status_adapter(const std::string& file_name, std::size_t size)
        : mem_(std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, file_name.c_str()\
, size)),
          file_name_(file_name),
          resource_status_memory_(mem_->construct<tateyama::status_info::resource_status_memory::resource_status_memory::resource_status>(std::string(tateyama::status_info::resource_status_memory::area_name).c_str())(mem_->get_segment_manager())) {
    }
    ~resource_status_adapter() {
        try {
            mem_->destroy<tateyama::status_info::resource_status_memory::resource_status_memory::resource_status>(std::string(tateyama::status_info::resource_status_memory::resource_status_memory::area_name).c_str());
            boost::interprocess::shared_memory_object::remove(file_name_.c_str());
        } catch (std::exception& e) {
            LOG(WARNING) << e.what();
        }
    }

    resource_status_adapter(resource_status_adapter const& other) = delete;
    resource_status_adapter& operator=(resource_status_adapter const& other) = delete;
    resource_status_adapter(resource_status_adapter&& other) noexcept = delete;
    resource_status_adapter& operator=(resource_status_adapter&& other) noexcept = delete;

    tateyama::status_info::resource_status_memory* resource_status_memory_address() {
        return &resource_status_memory_;
    }

private:
    std::unique_ptr<boost::interprocess::managed_shared_memory> mem_;
    std::string file_name_;
    tateyama::status_info::resource_status_memory resource_status_memory_;
};

class status_mock {
    static constexpr std::string_view file_prefix = "tsurugidb-";
    static constexpr std::string_view mutex_file_prefix = "tsurugi-";
    static constexpr std::size_t shm_size = 4096;
public:
    status_mock(const std::string& name, const std::string& digest) : status_mock(name, digest, "/tmp/") {
    }
    status_mock(const std::string& name, const std::string& digest, std::string directory) {
        status_file_name_ = file_prefix;
        status_file_name_ += digest;
        status_file_name_ += ".stat";
        try {
            resource_status_adapter_ = std::make_unique<resource_status_adapter>(status_file_name_, shm_size);
            resource_status_memory_=resource_status_adapter_->resource_status_memory_address();
            resource_status_memory_->set_pid();
            resource_status_memory_->set_database_name(name);
            std::string mutex_file_name{directory};
            mutex_file_name += mutex_file_prefix;
            mutex_file_name += digest;
            mutex_file_name += ".pid";
            resource_status_memory_->mutex_file(mutex_file_name);
        } catch(const boost::interprocess::interprocess_exception& ex) {
            std::stringstream ss{};
            ss << "could not create shared memory to inform tsurugidb status (cause; '"
               << ex.what()
               << "'), review the shared memory settings.";
            throw tgctl::runtime_error(monitor::reason::connection_failure, ss.str());
        }
    }
    ~status_mock() {
        boost::interprocess::shared_memory_object::remove(status_file_name_.c_str());
    }

private:
    std::string status_file_name_{};
    std::unique_ptr<resource_status_adapter> resource_status_adapter_{};
    tateyama::status_info::resource_status_memory* resource_status_memory_{};
};

}  // namespace tateyama::testing
