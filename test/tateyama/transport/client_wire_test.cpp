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

namespace tateyama::session {

static constexpr std::size_t threads = 16;
static constexpr std::size_t loops = 1024;

class client_wire_test : public ::testing::Test {
public:
    class worker {
        constexpr static std::size_t HEADER_MESSAGE_VERSION_MAJOR = 0;
        constexpr static std::size_t HEADER_MESSAGE_VERSION_MINOR = 0;
        constexpr static tateyama::framework::component::id_type TYPE = 1234;

    public:
        worker(tateyama::common::wire::session_wire_container& wire, boost::barrier& sync) : wire_(wire), sync_(sync) {
            header_.set_service_message_version_major(HEADER_MESSAGE_VERSION_MAJOR);
            header_.set_service_message_version_minor(HEADER_MESSAGE_VERSION_MINOR);
            header_.set_service_id(TYPE);
        }

        void operator()() {
            std::random_device seed_gen;
            std::default_random_engine engine(seed_gen());
            std::normal_distribution<> dist(1000, 1000);

            sync_.wait();
            for (std::size_t i = 0; i < loops; i++) {
                std::string pattern{std::to_string(pthread_self())};
                pattern += std::to_string(i);
                auto rv = send(pattern, static_cast<std::int32_t>(dist(engine)));
                if (!rv.has_value()) {
                    FAIL();
                }
                EXPECT_EQ(pattern, rv.value());
            }
        }

        std::optional<std::string> send(std::string_view request, std::int32_t t) {
            std::stringstream ss{};
            if(auto res = tateyama::utils::SerializeDelimitedToOstream(header_, std::addressof(ss)); ! res) {
                return std::nullopt;
            }
            if(auto res = tateyama::utils::PutDelimitedBodyToOstream(request, std::addressof(ss)); ! res) {
                return std::nullopt;
            }
            auto index = wire_.search_slot();
            wire_.send(ss.str(), index);

            if (t > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(t));
            }

            if (auto response_opt = receive(index); response_opt) {
                return response_opt.value();
            }
            return std::nullopt;
        }

    private:
        tateyama::common::wire::session_wire_container& wire_;
        boost::barrier& sync_;
        tateyama::proto::framework::request::Header header_{};

        std::optional<std::string> receive(tateyama::common::wire::message_header::index_type index) {
            std::string response_message{};
            
            while (true) {
                try {
                    wire_.receive(response_message, index);
                    break;
                } catch (std::runtime_error &e) {
                    std::cerr << e.what() << std::endl;
                    return std::nullopt;
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
    };

    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("client_wire_test", 20401);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("client_wire_test", bst_conf.digest(), sync_);
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


TEST_F(client_wire_test, echo) {
    tateyama::common::wire::session_wire_container wire(tateyama::common::wire::connection_container("client_wire_test").connect());
    std::vector<std::unique_ptr<worker>> workers{};
    boost::barrier thread_sync{threads};

    for (std::size_t i = 0; i < threads; i++) {
        workers.emplace_back(std::make_unique<worker>(wire, thread_sync));
    }

    std::vector<std::thread> threads{};
    for (std::size_t i = 0; i < workers.size(); i++) {
        threads.emplace_back(std::thread(std::ref(*workers.at(i))));
    }
    for (auto&& e: threads) {
        e.join();
    }
    wire.close();
}

}  // namespace tateyama::session
