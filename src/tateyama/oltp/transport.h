/*
 * Copyright 2022-2022 tsurugi project.
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

#include <tateyama/utils/protobuf_utils.h>
#include <tateyama/proto/framework/request.pb.h>
#include <tateyama/proto/framework/response.pb.h>
#include <tateyama/proto/datastore/common.pb.h>
#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>

#include "client_wire.h"

namespace tateyama::bootstrap::wire {

constexpr static std::size_t MESSAGE_VERSION = 1;

class transport {
public:
    transport() = delete;

    explicit transport(std::string_view name, tateyama::framework::component::id_type type) :
        wire_(tateyama::common::wire::session_wire_container(tateyama::common::wire::connection_container(name).connect())) {
        header_.set_message_version(MESSAGE_VERSION);
        header_.set_service_id(type);
    }

    template <typename T>
    std::optional<T> send(::tateyama::proto::datastore::request::Request& request) {
        auto box = wire_.get_response_box();
        if (box == nullptr) {
            return std::nullopt;
        }

        std::stringstream ss{};
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(header_, std::addressof(ss)); ! res) {
            return std::nullopt;
        }
        request.set_message_version(MESSAGE_VERSION);
        if(auto res = tateyama::utils::SerializeDelimitedToOstream(request, std::addressof(ss)); ! res) {
            return std::nullopt;
        }
        wire_.write(ss.str());
        wire_.flush();

        auto res = box->recv();
        ::tateyama::proto::framework::response::Header header{};
        google::protobuf::io::ArrayInputStream in{res.first, static_cast<int>(res.second)};
        if(auto res = tateyama::utils::ParseDelimitedFromZeroCopyStream(std::addressof(header), std::addressof(in), nullptr); ! res) {
            return std::nullopt;
        }
        std::string_view payload{};
        if (auto res = tateyama::utils::GetDelimitedBodyFromZeroCopyStream(std::addressof(in), nullptr, payload); ! res) {
            return std::nullopt;
        }
        T response{};
        if(auto res = response.ParseFromArray(payload.data(), payload.length()); ! res) {
            return std::nullopt;
        }
        return response;
    }

private:
    tateyama::common::wire::session_wire_container wire_;
    ::tateyama::proto::framework::request::Header header_{};
};

} // tateyama::bootstrap::wire