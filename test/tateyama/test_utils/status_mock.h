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

namespace tateyama::test_utils {

class status_mock {
    static constexpr std::string_view file_prefix = "tsurugidb-";
    static constexpr std::size_t shm_size = 4096;
public:
    status_mock(const std::string& name, const std::string& digest) {
        status_file_name_ = file_prefix;
        status_file_name_ += digest;
        status_file_name_ += ".stat";
        try {
            segment_ = std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, status_file_name_.c_str(), shm_size);
            resource_status_memory_ = std::make_unique<tateyama::status_info::resource_status_memory>(*segment_);
            resource_status_memory_->set_pid();
            resource_status_memory_->set_database_name(name);
        } catch(const boost::interprocess::interprocess_exception& ex) {
            std::stringstream ss{};
            ss << "could not create shared memory to inform tsurugidb status (cause; '"
               << ex.what()
               << "'), review the shared memory settings.";
            throw std::runtime_error(ss.str());
        }
    }
    ~status_mock() {
        boost::interprocess::shared_memory_object::remove(status_file_name_.c_str());
    }

private:
    std::string status_file_name_{};
    std::unique_ptr<boost::interprocess::managed_shared_memory> segment_{};
    std::unique_ptr<tateyama::status_info::resource_status_memory> resource_status_memory_{};
};

}  // namespace tateyama::testing
