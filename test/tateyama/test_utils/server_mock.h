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

#include <stdexcept>
#include <functional>
#include <unordered_map>

#include <boost/thread/barrier.hpp>
#include <glog/logging.h>
#include <tateyama/logging.h>

#include "endpoint.h"
#include "status_mock.h"

namespace tateyama::test_utils {

class server_mock {
public:
    server_mock(const std::string& name, const std::string& digest, boost::barrier& sync) :
        server_mock(name, digest, sync, "/tmp/") {
    }
    server_mock(const std::string& name, const std::string& digest, boost::barrier& sync, const std::string& directory) :
        name_(name), endpoint_(name_, digest, sync, directory), status_(std::make_unique<status_mock>(name, digest, directory)) {
    }
    ~server_mock() {
        endpoint_.terminate();
        remove_shm();
    }

    void push_response(std::string_view response, tateyama::proto::framework::response::Header_PayloadType type = tateyama::proto::framework::response::Header_PayloadType_SERVICE_RESULT) {
        endpoint_.push_response(response, type);
    }

    template<typename T>
    void request_message(T& reply) = delete;
    template<typename T>
    void request_message(T& reply, std::size_t) = delete;

    const tateyama::framework::component::id_type component_id() const {
        return endpoint_.component_id();
    }
    const std::string& current_request() const {
        return endpoint_.current_request();
    }
    const std::size_t update_expiration_time_count() {
        return endpoint_.update_expiration_time_count();
    }
    void suppress_message() {
        endpoint_.suppress_message();
    }

private:
    std::string name_;
    endpoint endpoint_;
    std::unique_ptr<status_mock> status_;

    void remove_shm() {
        std::string cmd = "if [ -f /dev/shm/" + name_ + " ]; then rm -f /dev/shm/" + name_ + "*; fi";
        if (system(cmd.c_str())) {
            throw std::runtime_error("error in clearing shared memory file");
        }
    }
};

}  // namespace tateyama::testing
