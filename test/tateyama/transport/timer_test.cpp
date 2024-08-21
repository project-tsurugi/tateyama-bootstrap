/*
 * Copyright 2022-2024 Project Tsurugi.
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

#include "test_root.h"

#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <random>

#include <boost/thread/barrier.hpp>

#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/transport/transport.h"
#include "tateyama/test_utils/server_mock.h"

#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/request.pb.h>

namespace tateyama::transport {

static constexpr std::size_t threads = 16;
static constexpr std::size_t loops = 1024;

class timer_test : public ::testing::Test {
    constexpr static std::size_t EXPIRATION_SECONDS = 6;
    constexpr static std::size_t SLEEP_TIME = 8;

    class transport_mock {
        constexpr static std::size_t HEADER_MESSAGE_VERSION_MAJOR = 0;
        constexpr static std::size_t HEADER_MESSAGE_VERSION_MINOR = 0;
        constexpr static std::size_t CORE_MESSAGE_VERSION_MAJOR = 0;
        constexpr static std::size_t CORE_MESSAGE_VERSION_MINOR = 0;
        constexpr static tateyama::framework::component::id_type TYPE = 2468;

    public:
        transport_mock(tateyama::common::wire::session_wire_container& wire) : wire_(wire) {
            header_.set_service_message_version_major(HEADER_MESSAGE_VERSION_MAJOR);
            header_.set_service_message_version_minor(HEADER_MESSAGE_VERSION_MINOR);
            header_.set_service_id(TYPE);

            time_ = std::make_unique<tateyama::common::wire::timer>(EXPIRATION_SECONDS, [this](){
                auto ret = update_expiration_time();
                if (ret.has_value()) {
                    return ret.value().result_case() == tateyama::proto::core::response::UpdateExpirationTime::ResultCase::kSuccess;
                }
                return false;
            });
        }
        std::optional<std::string> send(std::string_view request) {
            std::stringstream ss{};
            if(auto res = tateyama::utils::SerializeDelimitedToOstream(header_, std::addressof(ss)); ! res) {
                return std::nullopt;
            }
            if(auto res = tateyama::utils::PutDelimitedBodyToOstream(request, std::addressof(ss)); ! res) {
                return std::nullopt;
            }
            auto index = wire_.search_slot();
            wire_.send(ss.str(), index);

            if (auto response_opt = receive(index); response_opt) {
                return response_opt.value();
            }
            return std::nullopt;
        }
    private:
        tateyama::common::wire::session_wire_container& wire_;
        tateyama::proto::framework::request::Header header_{};
        std::unique_ptr<tateyama::common::wire::timer> time_{};

        std::optional<std::string> receive(tateyama::common::wire::message_header::index_type index) {
            std::string response_message{};
            
            while (true) {
                try {
                    wire_.receive(response_message, index);
                    break;
                } catch (std::runtime_error &e) {
                    continue;
                }
            }

            ::tateyama::proto::framework::response::Header header{};
            google::protobuf::io::ArrayInputStream in{response_message.data(), static_cast<int>(response_message.length())};
            if(auto res = tateyama::utils::ParseDelimitedFromZeroCopyStream(std::addressof(header), std::addressof(in), nullptr); ! res) {
                return std::nullopt;
            }
            std::string_view response{};
            bool eof{};
            if(auto res = tateyama::utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(in), &eof, response); ! res) {
                return std::nullopt;
            }
            return std::string{response};
        }

        std::optional<tateyama::proto::core::response::UpdateExpirationTime> update_expiration_time() {
            tateyama::proto::core::request::UpdateExpirationTime uet_request{};

            tateyama::proto::core::request::Request request{};
            *(request.mutable_update_expiration_time()) = uet_request;

            return send<tateyama::proto::core::response::UpdateExpirationTime>(request);
        }
        template <typename T>
        std::optional<T> send(::tateyama::proto::core::request::Request& request) {
            tateyama::proto::framework::request::Header fwrq_header{};
            fwrq_header.set_service_message_version_major(HEADER_MESSAGE_VERSION_MAJOR);
            fwrq_header.set_service_message_version_minor(HEADER_MESSAGE_VERSION_MINOR);
            fwrq_header.set_service_id(tateyama::framework::service_id_routing);

            std::stringstream sst{};
            if(auto res = tateyama::utils::SerializeDelimitedToOstream(fwrq_header, std::addressof(sst)); ! res) {
                return std::nullopt;
            }
            request.set_service_message_version_major(CORE_MESSAGE_VERSION_MAJOR);
            request.set_service_message_version_minor(CORE_MESSAGE_VERSION_MINOR);
            if(auto res = tateyama::utils::SerializeDelimitedToOstream(request, std::addressof(sst)); ! res) {
                return std::nullopt;
            }
            auto slot_index = wire_.search_slot();
            wire_.send(sst.str(), slot_index);

            std::string res_message{};
            while (true) {
                try {
                    wire_.receive(res_message, slot_index);
                    break;
                } catch (std::runtime_error &e) {
                    std::cerr << e.what() << std::endl;
                    continue;
                }
            }
            tateyama::proto::framework::response::Header fwrs_header{};
            google::protobuf::io::ArrayInputStream ins{res_message.data(), static_cast<int>(res_message.length())};
            if(auto res = tateyama::utils::ParseDelimitedFromZeroCopyStream(std::addressof(fwrs_header), std::addressof(ins), nullptr); ! res) {
                return std::nullopt;
            }
            std::string_view payload{};
            if (auto res = tateyama::utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(ins), nullptr, payload); ! res) {
                return std::nullopt;
            }
            T response{};
            if(auto res = response.ParseFromArray(payload.data(), payload.length()); ! res) {
                return std::nullopt;
            }
            return response;
        }
    };

public:
    class worker {
    public:
        worker(tateyama::common::wire::session_wire_container& wire) : transport_mock_(std::make_unique<transport_mock>(wire)) {
        }
        void operator()() {
            std::string time = std::to_string(SLEEP_TIME);
            auto rv = transport_mock_->send(time);
            if (!rv.has_value()) {
                FAIL();
            }
        }

    private:
        std::unique_ptr<transport_mock> transport_mock_{};
    };

    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("timer_test", 20402);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("timer_test", bst_conf.digest(), sync_);
        server_mock_->suppress_message();
        sync_.wait();
    }

    virtual void TearDown() {
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};
    std::unique_ptr<tateyama::test_utils::server_mock> server_mock_{};
    boost::barrier sync_{2};
};


TEST_F(timer_test, fundamental) {
    tateyama::common::wire::session_wire_container wire(tateyama::common::wire::connection_container("timer_test").connect());
    std::unique_ptr<worker> w = std::make_unique<worker>(wire);
    std::thread thread = std::thread(std::ref(*w));

    thread.join();
    EXPECT_GT(server_mock_->update_expiration_time_count(), 0);
    wire.close();
}

}  // namespace tateyama::transport
