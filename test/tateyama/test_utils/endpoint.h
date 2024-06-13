/*
 * Copyright 2023-2024 Project Tsurugi.
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
#include <thread>
#include <stdexcept>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>

#include <boost/thread/barrier.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include <glog/logging.h>
#include <tateyama/logging.h>

#include <tateyama/proto/framework/response.pb.h>
#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>
#include "server_wires_mock.h"
#include "endpoint_proto_utils.h"

namespace tateyama::test_utils {

class endpoint_response {
public:
    endpoint_response(std::string_view response, tateyama::proto::framework::response::Header_PayloadType type) : type_(type), response_(response) {
    }
    std::string_view get_response() const {
        return response_;
    }
    tateyama::proto::framework::response:: Header_PayloadType get_type() const {
        return type_;
    }
private:
    tateyama::proto::framework::response:: Header_PayloadType type_;
    std::string response_;
};

class endpoint {
    constexpr static tateyama::common::wire::response_header::msg_type RESPONSE_BODY = 1;
    constexpr static tateyama::common::wire::response_header::msg_type RESPONSE_BODYHEAD = 2;
    constexpr static tateyama::common::wire::response_header::msg_type RESPONSE_HANDSHAKE = 98;
    constexpr static tateyama::common::wire::response_header::msg_type RESPONSE_HELLO = 99;

public:
    class worker {
    public:
        worker(std::size_t session_id, std::unique_ptr<tateyama::test_utils::server_wire_container_mock> wire, std::function<void(void)> clean_up, std::queue<endpoint_response>& responses)
            : session_id_(session_id), wire_(std::move(wire)), clean_up_(std::move(clean_up)), responses_(responses), thread_(std::thread(std::ref(*this))) {
        }
        ~worker() {
            if (thread_.joinable()) {
                thread_.join();
            }
        }
        void operator()() {
            while(true) {
                auto h = wire_->get_request_wire().peep();
                auto index = h.get_idx();
                if (h.get_length() == 0 && index == tateyama::common::wire::message_header::terminate_request) { break; }
                std::string message;
                message.resize(h.get_length());
                wire_->get_request_wire().read(message.data());

                ::tateyama::proto::framework::request::Header req_header{};
                google::protobuf::io::ArrayInputStream in{message.data(), static_cast<int>(message.length())};
                if(auto res = tateyama::utils::ParseDelimitedFromZeroCopyStream(std::addressof(req_header), std::addressof(in), nullptr); ! res) {
                    throw std::runtime_error("error parsing request message");
                }
                std::stringstream ss{};
                if (component_id_ = req_header.service_id(); component_id_ == tateyama::framework::service_id_endpoint_broker) {
                    ::tateyama::proto::framework::response::Header header{};
                    if(auto res = tateyama::utils::SerializeDelimitedToOstream(header, std::addressof(ss)); ! res) {
                        throw std::runtime_error("error formatting response message");
                    }
                    tateyama::proto::endpoint::response::Handshake rp{};
                    auto rs = rp.mutable_success();
                    rs->set_session_id(1);  // session id is dummy, as this is a test
                    auto body = rp.SerializeAsString();
                    if(auto res = tateyama::utils::PutDelimitedBodyToOstream(body, std::addressof(ss)); ! res) {
                        throw std::runtime_error("error formatting response message");
                    }
                    rp.clear_success();
                    auto reply_message = ss.str();
                    wire_->get_response_wire().write(reply_message.data(), tateyama::common::wire::response_header(index, reply_message.length(), RESPONSE_HANDSHAKE));
                    continue;
                }
                {
                    std::string_view payload{};
                    if (auto res = tateyama::utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(in), nullptr, payload); ! res) {
                        throw std::runtime_error("error reading payload");
                    }
                    current_request_ = payload;
                    if (responses_.empty()) {
                        throw std::runtime_error("response queue is empty");
                    }
                    auto reply = responses_.front();
                    responses_.pop();
                    std::stringstream ss{};
                    ::tateyama::proto::framework::response::Header header{};
                    header.set_payload_type(reply.get_type());
                    header.set_session_id(session_id_);
                    if(auto res = tateyama::utils::SerializeDelimitedToOstream(header, std::addressof(ss)); ! res) {
                        throw std::runtime_error("error formatting response message");
                    }
                    if(auto res = tateyama::utils::PutDelimitedBodyToOstream(reply.get_response(), std::addressof(ss)); ! res) {
                        throw std::runtime_error("error formatting response message");
                    }
                    auto reply_message = ss.str();
                    wire_->get_response_wire().write(reply_message.data(), tateyama::common::wire::response_header(index, reply_message.length(), RESPONSE_BODY));
                }
            }
            clean_up_();
        }
        const tateyama::framework::component::id_type component_id() const {
            return component_id_;
        }
        const std::string& current_request() const {
            return current_request_;
        }

    private:
        std::size_t session_id_;
        std::unique_ptr<tateyama::test_utils::server_wire_container_mock> wire_;
        std::function<void(void)> clean_up_;
        std::queue<endpoint_response>& responses_;
        std::thread thread_;

        std::string current_request_;
        tateyama::framework::component::id_type component_id_{};
    };

    endpoint(const std::string& name, const std::string& digest, boost::barrier& sync)
        : name_(name), digest_(digest), container_(std::make_unique<tateyama::test_utils::connection_container>(name_, 1)), sync_(sync) {
        thread_ = std::thread(std::ref(*this));
    }
    ~endpoint() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    worker* get_worker() {
        if (!worker_) {
            std::unique_lock<std::mutex> lk(mutex_);
            condition_.wait(lk, [&]{
                return worker_.get() != nullptr;
            });
        }
        return worker_.get();
    }
    void operator()() {
        auto& connection_queue = container_->get_connection_queue();

        if (!notified_) {
            sync_.wait();
            notified_ =  true;
        }
        while(true) {
            auto session_id = connection_queue.listen();
            if (connection_queue.is_terminated()) {
                connection_queue.confirm_terminated();
                break;
            }
            std::string session_name = name_;
            session_name += "-";
            session_name += std::to_string(session_id);
            auto wire = std::make_unique<tateyama::test_utils::server_wire_container_mock>(session_name, std::string("tsurugidb-") + digest_ + ".stat");
            std::size_t index = connection_queue.slot();
            connection_queue.accept(index, session_id);
            try {
                std::unique_lock<std::mutex> lk(mutex_);
                worker_ = std::make_unique<worker>(session_id, std::move(wire), [&connection_queue, index](){ connection_queue.disconnect(index); }, responses_);
                condition_.notify_all();
            } catch (std::exception& ex) {
                LOG(ERROR) << ex.what();
                break;
            }
        }
    }
    void push_response(std::string_view response, tateyama::proto::framework::response::Header_PayloadType type) {
        responses_.emplace(response, type);
    }
    const tateyama::framework::component::id_type component_id() const {
        if (worker_) {
            return worker_->component_id();
        }
        throw std::runtime_error("no active worker thread");
    }
    const std::string& current_request() const {
        if (worker_) {
            return worker_->current_request();
        }
        throw std::runtime_error("no active worker thread");
    }
    void terminate() {
        container_->get_connection_queue().request_terminate();
    }

private:
    std::string name_;
    std::string digest_;
    std::unique_ptr<tateyama::test_utils::connection_container> container_;
    std::thread thread_;
    boost::barrier& sync_;
    std::unique_ptr<worker> worker_{};
    std::mutex mutex_{};
    std::condition_variable condition_{};
    std::queue<endpoint_response> responses_{};
    bool notified_{false};
};

}  // namespace tateyama::test_utils
