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
#pragma once

#include <sstream>
#include <optional>
#include <exception>
#include <memory>
#include <sys/types.h>
#include <unistd.h>

#include <gflags/gflags.h>

#include <tateyama/framework/component_ids.h>
#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/framework/response.pb.h>
#include <tateyama/proto/core/request.pb.h>
#include <tateyama/proto/core/response.pb.h>
#include <tateyama/proto/datastore/common.pb.h>
#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>
#include <tateyama/proto/endpoint/request.pb.h>
#include <tateyama/proto/endpoint/response.pb.h>
#include <tateyama/proto/session/request.pb.h>
#include <tateyama/proto/session/response.pb.h>
#include <tateyama/proto/metrics/request.pb.h>
#include <tateyama/proto/metrics/response.pb.h>

#include "tateyama/server/status_info.h"
#include "client_wire.h"
#include "timer.h"

DECLARE_string(conf);  // NOLINT

namespace tateyama::bootstrap::wire {

constexpr static std::size_t HEADER_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t HEADER_MESSAGE_VERSION_MINOR = 0;
constexpr static std::size_t CORE_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t CORE_MESSAGE_VERSION_MINOR = 0;
constexpr static std::size_t DATASTORE_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t DATASTORE_MESSAGE_VERSION_MINOR = 0;
constexpr static std::size_t ENDPOINT_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t ENDPOINT_MESSAGE_VERSION_MINOR = 0;
constexpr static std::size_t SESSION_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t SESSION_MESSAGE_VERSION_MINOR = 0;
constexpr static std::size_t METRICS_MESSAGE_VERSION_MAJOR = 0;
constexpr static std::size_t METRICS_MESSAGE_VERSION_MINOR = 0;
constexpr static std::int64_t EXPIRATION_SECONDS = 60;

class transport {
public:
    transport() = delete;

    explicit transport(tateyama::framework::component::id_type type) :
        wire_(tateyama::common::wire::session_wire_container(tateyama::common::wire::connection_container(database_name()).connect())) {
        header_.set_service_message_version_major(HEADER_MESSAGE_VERSION_MAJOR);
        header_.set_service_message_version_minor(HEADER_MESSAGE_VERSION_MINOR);
        header_.set_service_id(type);
        status_info_ = std::make_unique<server::status_info_bridge>(digest());
        auto handshake_response_opt = handshake();
        if (!handshake_response_opt) {
            throw std::runtime_error("handshake error");
        }
        auto& handshake_response = handshake_response_opt.value();
        if (handshake_response.result_case() != tateyama::proto::endpoint::response::Handshake::ResultCase::kSuccess) {
            throw std::runtime_error("handshake error");
        }
        session_id_ = handshake_response.success().session_id();
        header_.set_session_id(session_id_);

        time_ = std::make_unique<tateyama::common::wire::timer>(EXPIRATION_SECONDS, [this](){
            auto ret = update_expiration_time();
            if (ret.has_value()) {
                return ret.value().result_case() == tateyama::proto::core::response::UpdateExpirationTime::ResultCase::kSuccess;
            }
            return false;
        });
    }

    ~transport() {
        try {
            time_ = nullptr;
            if (!closed_) {
                close();
            }
        } catch (std::exception &ex) {
            std::cerr << ex.what() << std::endl;
        }
    }

    transport(transport const& other) = delete;
    transport& operator=(transport const& other) = delete;
    transport(transport&& other) noexcept = delete;
    transport& operator=(transport&& other) noexcept = delete;

    template <typename T>
    std::optional<T> send(::tateyama::proto::datastore::request::Request& request) {
        std::stringstream sst{};
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(header_, std::addressof(sst)); ! res) {
            return std::nullopt;
        }
        request.set_service_message_version_major(DATASTORE_MESSAGE_VERSION_MAJOR);
        request.set_service_message_version_minor(DATASTORE_MESSAGE_VERSION_MINOR);
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
                if (status_info_->alive()) {
                    continue;
                }
                std::cerr << e.what() << std::endl;
                return std::nullopt;
            }
        }
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

    // for session
    template <typename T>
    std::optional<T> send(::tateyama::proto::session::request::Request& request) {
        std::stringstream sst{};
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(header_, std::addressof(sst)); ! res) {
            return std::nullopt;
        }
        request.set_service_message_version_major(SESSION_MESSAGE_VERSION_MAJOR);
        request.set_service_message_version_minor(SESSION_MESSAGE_VERSION_MINOR);
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
                if (status_info_->alive()) {
                    continue;
                }
                std::cerr << e.what() << std::endl;
                return std::nullopt;
            }
        }
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
        auto slot_index = wire_.search_slot();
        wire_.send(sst.str(), slot_index);

        std::string res_message{};
        while (true) {
            try {
                wire_.receive(res_message, slot_index);
                break;
            } catch (std::runtime_error &e) {
                if (status_info_->alive()) {
                    continue;
                }
                std::cerr << e.what() << std::endl;
                return std::nullopt;
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

    // expiration
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
                if (status_info_->alive()) {
                    continue;
                }
                std::cerr << e.what() << std::endl;
                return std::nullopt;
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

    // metrics
    template <typename T>
    std::optional<T> send(::tateyama::proto::metrics::request::Request& request) {
        std::stringstream sst{};
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(header_, std::addressof(sst)); ! res) {
            return std::nullopt;
        }
        request.set_service_message_version_major(METRICS_MESSAGE_VERSION_MAJOR);
        request.set_service_message_version_minor(METRICS_MESSAGE_VERSION_MINOR);
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
                if (status_info_->alive()) {
                    continue;
                }
                std::cerr << e.what() << std::endl;
                return std::nullopt;
            }
        }
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

    void close() {
        wire_.close();
        closed_ = true;
    }

    [[nodiscard]] std::size_t session_id() const noexcept {
        return session_id_;
    }

    static std::string database_name() {
        auto conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf).get_configuration();
        auto endpoint_config = conf->get_section("ipc_endpoint");
        if (endpoint_config == nullptr) {
            throw std::runtime_error("cannot find ipc_endpoint section in the configuration");
        }
        auto database_name_opt = endpoint_config->get<std::string>("database_name");
        if (!database_name_opt) {
            throw std::runtime_error("cannot find database_name at the section in the configuration");
        }
        return database_name_opt.value();
    }

private:
    tateyama::common::wire::session_wire_container wire_;
    tateyama::proto::framework::request::Header header_{};
    std::unique_ptr<tateyama::server::status_info_bridge> status_info_{};
    std::size_t session_id_{};
    bool closed_{};
    std::unique_ptr<tateyama::common::wire::timer> time_{};

    std::string digest() {
        auto bst_conf = configuration::bootstrap_configuration::create_bootstrap_configuration(FLAGS_conf);
        if (bst_conf.valid()) {
            return bst_conf.digest();
        }
        return {};
    }

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
        (void)request.release_handshake();

        (void)wire_information.release_ipc_information();
        (void)handshake.release_wire_information();

        (void)handshake.release_client_information();
        return response;
    }

    std::optional<tateyama::proto::core::response::UpdateExpirationTime> update_expiration_time() {
        tateyama::proto::core::request::UpdateExpirationTime uet_request{};

        tateyama::proto::core::request::Request request{};
        *(request.mutable_update_expiration_time()) = uet_request;

        return send<tateyama::proto::core::response::UpdateExpirationTime>(request);
    }
};

} // tateyama::bootstrap::wire
