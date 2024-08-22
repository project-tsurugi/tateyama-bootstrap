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
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <chrono>
#include <unistd.h>

#include <boost/thread/barrier.hpp>

#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/transport/transport.h"
#include "tateyama/test_utils/server_mock.h"

#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/request.pb.h>

namespace tateyama::transport {

static constexpr std::size_t threads = 16;
static constexpr std::size_t loops = 1024;

class resutset_timeout_test : public ::testing::Test {
    class transport_mock {
        constexpr static std::size_t HEADER_MESSAGE_VERSION_MAJOR = 0;
        constexpr static std::size_t HEADER_MESSAGE_VERSION_MINOR = 0;
        constexpr static std::size_t CORE_MESSAGE_VERSION_MAJOR = 0;
        constexpr static std::size_t CORE_MESSAGE_VERSION_MINOR = 0;
        constexpr static tateyama::framework::component::id_type TYPE = 3072;

    public:
        transport_mock(tateyama::common::wire::session_wire_container& wire, std::string digest) : wire_(wire), status_provider_(wire_.get_status_provider()) {
            header_.set_service_message_version_major(HEADER_MESSAGE_VERSION_MAJOR);
            header_.set_service_message_version_minor(HEADER_MESSAGE_VERSION_MINOR);
            header_.set_service_id(TYPE);
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
        std::unique_ptr<tateyama::common::wire::session_wire_container::resultset_wires_container> create_resultset_wire(std::string_view name) {
            auto rw = wire_.create_resultset_wire();
            try {
                rw->connect(name);
            } catch (std::runtime_error& e) {
                std::cerr << e.what() << std::endl;
            }
            return rw;
        }
    private:
        tateyama::common::wire::session_wire_container& wire_;
        tateyama::common::wire::status_provider &status_provider_;
        tateyama::proto::framework::request::Header header_{};
        std::unique_ptr<tateyama::common::wire::timer> time_{};

        std::optional<std::string> receive(tateyama::common::wire::message_header::index_type index) {
            std::string response_message{};
            wire_.receive(response_message, index);
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

public:
    class worker {
    public:
        worker(tateyama::common::wire::session_wire_container& wire, std::string digest) : transport_mock_(std::make_unique<transport_mock>(wire, digest)) {
        }
        void send(int t) {
            std::string time = std::to_string(t);
            auto rv = transport_mock_->send(time);
            if (!rv.has_value()) {
                FAIL();
            }
        }
        std::unique_ptr<tateyama::common::wire::session_wire_container::resultset_wires_container> create_resultset_wire(std::string_view name) {
            return transport_mock_->create_resultset_wire(name);
        }
                                                                                                                         
    private:
        std::unique_ptr<transport_mock> transport_mock_{};
    };

    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("resutset_timeout_test", 20404);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        digest_ = bst_conf.digest();
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("resutset_timeout_test", digest_, sync_, helper_->location());
        mutex_file_ = helper_->abs_path(std::string("tsurugi-") + digest_ + ".pid");
        if (system((std::string("touch ") + mutex_file_).c_str()) != 0) {
            FAIL();
        }
        flock();
        server_mock_->suppress_message();
        sync_.wait();
    }

    virtual void TearDown() {
        close(fd_);
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};
    std::unique_ptr<tateyama::test_utils::server_mock> server_mock_{};
    boost::barrier sync_{2};
    std::string digest_{};
    std::string mutex_file_{};
    int fd_{};

    void funlock() {
        ::flock(fd_, LOCK_UN);
    }

private:
    void flock() {
        int fd_ = open(mutex_file_.c_str(), O_RDONLY);  // NOLINT
        if (fd_ < 0) {
            std::stringstream ss{};
            ss << "in " << __func__ << ": ";
            ss << "cannot open the lock file (" << mutex_file_.c_str() << ")";
            FAIL();
        }
        if (::flock(fd_, LOCK_EX | LOCK_NB) == 0) {  // NOLINT
            return;
        }
        FAIL();
    }
};

TEST_F(resutset_timeout_test, response_alive_case) {
    tateyama::common::wire::session_wire_container wire(tateyama::common::wire::connection_container("resutset_timeout_test").connect());
    worker w(wire, digest_);
    w.send(6);
    auto resultset_wire = w.create_resultset_wire("resultset");
    auto reply = resultset_wire->get_chunk();
    EXPECT_FALSE(reply.empty());
    
    wire.close();
}

TEST_F(resutset_timeout_test, response_dead_case) {
    tateyama::common::wire::session_wire_container wire(tateyama::common::wire::connection_container("resutset_timeout_test").connect());
    worker w(wire, digest_);
    funlock();

    w.send(6);
    auto resultset_wire = w.create_resultset_wire("resultset");
    EXPECT_THROW(resultset_wire->get_chunk(), std::runtime_error);

    wire.close();
}

}  // namespace tateyama::transport
