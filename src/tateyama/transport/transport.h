/*
 * Copyright 2022-2023 Project Tsurugi.
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

#include <sstream>
#include <optional>
#include <exception>
#include <sys/types.h>
#include <unistd.h>

#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/framework/response.pb.h>
#include <tateyama/proto/datastore/common.pb.h>
#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>
#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>

#include "tateyama/server/status_info.h"
#include "client_wire.h"

namespace tateyama::bootstrap::wire {

constexpr static std::size_t HEADER_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t HEADER_MESSAGE_VERSION_MINOR = 0;
constexpr static std::size_t DATASTORE_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t DATASTORE_MESSAGE_VERSION_MINOR = 0;
constexpr static std::size_t ENDPOINT_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t ENDPOINT_MESSAGE_VERSION_MINOR = 0;

class transport {
public:
    transport() = delete;

    transport(std::string_view name, std::string_view digest, tateyama::framework::component::id_type type) :
        wire_(tateyama::common::wire::session_wire_container(tateyama::common::wire::connection_container(name).connect())) {
        header_.set_service_message_version_major(HEADER_MESSAGE_VERSION_MAJOR);
        header_.set_service_message_version_minor(HEADER_MESSAGE_VERSION_MINOR);
        header_.set_service_id(type);
        status_info_ = std::make_unique<server::status_info_bridge>(std::string(digest));
        auto handshake_response = handshake();
        if (!handshake_response) {
            throw std::runtime_error("handshake error");
        }
        if (handshake_response.value().result_case() != tateyama::proto::endpoint::response::Handshake::ResultCase::kSuccess) {
            throw std::runtime_error("handshake error");
        }
    }

    template <typename T>
    std::optional<T> send(::tateyama::proto::datastore::request::Request& request) {
        auto& response_wire = wire_.get_response_wire();

        std::stringstream sst{};
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(header_, std::addressof(sst)); ! res) {
            return std::nullopt;
        }
        request.set_service_message_version_major(DATASTORE_MESSAGE_VERSION_MAJOR);
        request.set_service_message_version_minor(DATASTORE_MESSAGE_VERSION_MINOR);
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(request, std::addressof(sst)); ! res) {
            return std::nullopt;
        }
        wire_.write(sst.str());

        while (true) {
            try {
                response_wire.await();
                break;
            } catch (std::runtime_error &e) {
                if (status_info_->alive()) {
                    continue;
                }
                std::cerr << e.what() << std::endl;
                return std::nullopt;
            }
        }
        std::string res_message{};
        res_message.resize(response_wire.get_length());
        response_wire.read(res_message.data());
        ::tateyama::proto::framework::response::Header header{};
        google::protobuf::io::ArrayInputStream ins{res_message.data(), static_cast<int>(res_message.length())};
        if(auto res = tateyama::utils::ParseDelimitedFromZeroCopyStream(std::addressof(header), std::addressof(ins), nullptr); ! res) {
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

    // implement handshake
    template <typename T>
    std::optional<T> send(::tateyama::proto::endpoint::request::Request& request) {
        auto& response_wire = wire_.get_response_wire();

        tateyama::proto::framework::request::Header fwrq_header{};
        fwrq_header.set_service_message_version_major(HEADER_MESSAGE_VERSION_MAJOR);
        fwrq_header.set_service_message_version_minor(HEADER_MESSAGE_VERSION_MINOR);
        fwrq_header.set_service_id(tateyama::framework::service_id_endpoint_broker);

        std::stringstream sst{};
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(fwrq_header, std::addressof(sst)); ! res) {
            return std::nullopt;
        }
        request.set_service_message_version_major(ENDPOINT_MESSAGE_VERSION_MAJOR);
        request.set_service_message_version_minor(ENDPOINT_MESSAGE_VERSION_MINOR);
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(request, std::addressof(sst)); ! res) {
            return std::nullopt;
        }
        wire_.write(sst.str());

        while (true) {
            try {
                response_wire.await();
                break;
            } catch (std::runtime_error &e) {
                if (status_info_->alive()) {
                    continue;
                }
                std::cerr << e.what() << std::endl;
                return std::nullopt;
            }
        }
        std::string res_message{};
        res_message.resize(response_wire.get_length());
        response_wire.read(res_message.data());
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

    void close() {
        wire_.close();
    }

private:
    tateyama::common::wire::session_wire_container wire_;
    tateyama::proto::framework::request::Header header_{};
    std::unique_ptr<tateyama::server::status_info_bridge> status_info_{};

    std::optional<tateyama::proto::endpoint::response::Handshake> handshake() {
        tateyama::proto::endpoint::request::ClientInformation information{};
        information.set_application_name("tgctl");

        tateyama::proto::endpoint::request::WireInformation wire_information{};
        tateyama::proto::endpoint::request::WireInformation::IpcInformation ipc_information{};
        ipc_information.set_connection_information(std::to_string(getpid()));
        wire_information.set_allocated_ipc_information(&ipc_information);

        tateyama::proto::endpoint::request::Handshake handshake{};
        handshake.set_allocated_client_information(&information);
        handshake.set_allocated_wire_information(&wire_information);
        tateyama::proto::endpoint::request::Request request{};
        request.set_allocated_handshake(&handshake);
        auto response = send<tateyama::proto::endpoint::response::Handshake>(request);
        request.release_handshake();

        wire_information.release_ipc_information();
        handshake.release_wire_information();

        handshake.release_client_information();
        return response;
    }
};

} // tateyama::bootstrap::wire
