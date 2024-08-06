/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <set>
#include <mutex>
#include <condition_variable>
#include <string>
#include <queue>
#include <thread>
#include <sstream>
#include <string_view>

#include <glog/logging.h>
#include <tateyama/logging.h>
#include <tateyama/logging_helper.h>

#include "tateyama/transport/wire.h"

namespace tateyama::test_utils {

class server_wire_container_mock
{
    static constexpr std::size_t request_buffer_size = (1<<12);   //  4K bytes NOLINT
    static constexpr std::size_t response_buffer_size = (1<<13);  //  8K bytes NOLINT
    static constexpr std::size_t writer_count = 32;
    static constexpr std::size_t data_channel_overhead = 7700;   //  by experiment NOLINT
    static constexpr std::size_t total_overhead = (1<<14);   //  16K bytes by experiment NOLINT

public:
    class request_wire_container_mock {
    public:
        request_wire_container_mock() = default;
        ~request_wire_container_mock() = default;
        request_wire_container_mock(request_wire_container_mock const&) = delete;
        request_wire_container_mock(request_wire_container_mock&&) = delete;
        request_wire_container_mock& operator = (request_wire_container_mock const&) = delete;
        request_wire_container_mock& operator = (request_wire_container_mock&&) = delete;

        void initialize(tateyama::common::wire::unidirectional_message_wire* wire, char* bip_buffer) {
            wire_ = wire;
            bip_buffer_ = bip_buffer;
        }
        tateyama::common::wire::message_header peep() {
            while (true) {
                try {
                    return wire_->peep(bip_buffer_);
                } catch (std::runtime_error &ex) {
                    if (!suppress_message_) {
                        std::cout << ex.what() << std::endl;
                    }
                    if (finish_) {
                        break;
                    }
                }
            }
            return {tateyama::common::wire::message_header::terminate_request, 0};
        }
        std::string_view payload() {
            return wire_->payload(bip_buffer_);
        }
        void read(char* to) {
            wire_->read(to, bip_buffer_);
        }
        std::size_t read_point() { return wire_->read_point(); }
        void dispose() { wire_->dispose(); }
        void notify() { wire_->notify(); }
        void finish() { finish_ = true; }
        void suppress_message() { suppress_message_ = true; }

        // for mainly client, except for terminate request from server
        void write(const char* from, const std::size_t len, tateyama::common::wire::message_header::index_type index) {
            wire_->write(bip_buffer_, from, tateyama::common::wire::message_header(index, len));
        }

    private:
        tateyama::common::wire::unidirectional_message_wire* wire_{};
        char* bip_buffer_{};
        bool finish_{};
        bool suppress_message_{};
    };

    class response_wire_container_mock {
    public:
        response_wire_container_mock() = default;
        ~response_wire_container_mock() = default;
        response_wire_container_mock(response_wire_container_mock const&) = delete;
        response_wire_container_mock(response_wire_container_mock&&) = delete;
        response_wire_container_mock& operator = (response_wire_container_mock const&) = delete;
        response_wire_container_mock& operator = (response_wire_container_mock&&) = delete;

        void initialize(tateyama::common::wire::unidirectional_response_wire* wire, char* bip_buffer) {
            wire_ = wire;
            bip_buffer_ = bip_buffer;
        }
        void write(const char* from, tateyama::common::wire::response_header header) {
            std::lock_guard<std::mutex> lock(mtx_);
            wire_->write(bip_buffer_, from, header);
        }
        void notify_shutdown() {
            wire_->notify_shutdown();
        }

        // for client
        tateyama::common::wire::response_header await() {
            while (true) {
                try {
                    return wire_->await(bip_buffer_);
                } catch (std::runtime_error &ex) {
                    if (!suppress_message_) {
                        std::cout << ex.what() << std::endl;
                    }
                    if (finish_) {
                        break;
                    }
                }
            }
        }
        [[nodiscard]] tateyama::common::wire::response_header::length_type get_length() const {
            return wire_->get_length();
        }
        [[nodiscard]] tateyama::common::wire::response_header::index_type get_idx() const {
            return wire_->get_idx();
        }
        [[nodiscard]] tateyama::common::wire::response_header::msg_type get_type() const {
            return wire_->get_type();
        }
        void read(char* to) {
            wire_->read(to, bip_buffer_);
        }
        void close() {
            wire_->close();
        }
        void finish() { finish_ = true; }
        void suppress_message() { suppress_message_ = true; }

    private:
        tateyama::common::wire::unidirectional_response_wire* wire_{};
        char* bip_buffer_{};
        std::mutex mtx_{};
        bool finish_{};
        bool suppress_message_{};
    };

    server_wire_container_mock(std::string_view name, std::string_view mutex_file) : name_(name) {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
        try {
            boost::interprocess::permissions unrestricted_permissions;
            unrestricted_permissions.set_unrestricted();

            std::size_t shm_size = request_buffer_size + response_buffer_size + total_overhead;
            managed_shared_memory_ =
                std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, name_.c_str(), shm_size, nullptr, unrestricted_permissions);
            auto req_wire = managed_shared_memory_->construct<tateyama::common::wire::unidirectional_message_wire>(tateyama::common::wire::request_wire_name)(managed_shared_memory_.get(), request_buffer_size);
            auto res_wire = managed_shared_memory_->construct<tateyama::common::wire::unidirectional_response_wire>(tateyama::common::wire::response_wire_name)(managed_shared_memory_.get(), response_buffer_size);
            status_provider_ = managed_shared_memory_->construct<tateyama::common::wire::status_provider>(tateyama::common::wire::status_provider_name)(managed_shared_memory_.get(), mutex_file);

            request_wire_.initialize(req_wire, req_wire->get_bip_address(managed_shared_memory_.get()));
            response_wire_.initialize(res_wire, res_wire->get_bip_address(managed_shared_memory_.get()));
        } catch(const boost::interprocess::interprocess_exception& ex) {
            LOG_LP(ERROR) << ex.what() << " on server_wire_container_mock::server_wire_container_mock()";
            throw std::runtime_error(ex.what());
        } catch (std::runtime_error &ex) {
            LOG_LP(ERROR) << "running out of boost managed shared memory";
            throw ex;
        }
    }

    /**
     * @brief Copy and move constructers are deleted.
     */
    server_wire_container_mock(server_wire_container_mock const&) = delete;
    server_wire_container_mock(server_wire_container_mock&&) = delete;
    server_wire_container_mock& operator = (server_wire_container_mock const&) = delete;
    server_wire_container_mock& operator = (server_wire_container_mock&&) = delete;

    ~server_wire_container_mock() {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
    }

    request_wire_container_mock& get_request_wire() { return request_wire_; }
    response_wire_container_mock& get_response_wire() { return response_wire_; }

    void notify_shutdown() {
        request_wire_.notify();
        response_wire_.notify_shutdown();
    }
    void finish() {
        request_wire_.finish();
        response_wire_.finish();
    }
    void suppress_message() {
        request_wire_.suppress_message();
        response_wire_.suppress_message();
    }

private:
    std::string name_;
    std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shared_memory_{};
    request_wire_container_mock request_wire_{};
    response_wire_container_mock response_wire_{};
    tateyama::common::wire::status_provider* status_provider_{};
    std::mutex mtx_shm_{};

    std::size_t datachannel_buffer_size_;
};

class connection_container
{
public:
    explicit connection_container(std::string_view name, std::size_t n) : name_(name) {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
        try {
            boost::interprocess::permissions  unrestricted_permissions;
            unrestricted_permissions.set_unrestricted();

            managed_shared_memory_ =
                std::make_unique<boost::interprocess::managed_shared_memory>(boost::interprocess::create_only, name_.c_str(), request_queue_size(n), nullptr, unrestricted_permissions);
            managed_shared_memory_->destroy<tateyama::common::wire::connection_queue>(tateyama::common::wire::connection_queue::name);
            connection_queue_ = managed_shared_memory_->construct<tateyama::common::wire::connection_queue>(tateyama::common::wire::connection_queue::name)(n, managed_shared_memory_->get_segment_manager(), 1);
        }
        catch(const boost::interprocess::interprocess_exception& ex) {
            using namespace std::literals::string_view_literals;

            std::stringstream ss{};
            ss << "cannot create a database connection outlet named "sv << name << ", probably another server is running using the same database name"sv;
            throw std::runtime_error(ss.str());
        }
    }

    /**
     * @brief Copy and move constructers are deleted.
     */
    connection_container(connection_container const&) = delete;
    connection_container(connection_container&&) = delete;
    connection_container& operator = (connection_container const&) = delete;
    connection_container& operator = (connection_container&&) = delete;

    ~connection_container() {
        boost::interprocess::shared_memory_object::remove(name_.c_str());
    }
    tateyama::common::wire::connection_queue& get_connection_queue() {
        return *connection_queue_;
    }

private:
    std::string name_;
    std::unique_ptr<boost::interprocess::managed_shared_memory> managed_shared_memory_{};
    tateyama::common::wire::connection_queue* connection_queue_;

    static constexpr std::size_t initial_size = 720;      // obtained by experiment
    static constexpr std::size_t per_size = 112;          // obtained by experiment
    std::size_t request_queue_size(std::size_t n) {
        std::size_t size = initial_size + (n * per_size); // exact size
        size += initial_size / 2;                         // a little bit of leeway
        return ((size / 4096) + 1) * 4096;                // round up to the page size
    }
};

};  // namespace tateyama::common::wire
