/*
 * Copyright 2026 Project Tsurugi.
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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <glog/logging.h>
#include <tateyama/api/configuration.h>
#include "tateyama/configuration/bootstrap_configuration.h"

namespace tateyama::testing {
namespace {
class error_log_capture final : public google::LogSink {
public:
    error_log_capture() : previous_minloglevel_(FLAGS_minloglevel) {
        FLAGS_minloglevel = 0;
        google::AddLogSink(this);
    }
    ~error_log_capture() override {
        google::RemoveLogSink(this);
        FLAGS_minloglevel = previous_minloglevel_;
    }
    void send(google::LogSeverity severity, char const*, char const*, int,
              google::LogMessageTime const&, char const* message, std::size_t length) override {
        if (severity < google::GLOG_ERROR) return;
        std::lock_guard lock{mutex_};
        errors_.append(message, length).append("\n");
    }
    [[nodiscard]] std::string errors() const {
        std::lock_guard lock{mutex_};
        return errors_;
    }
private:
    int previous_minloglevel_{};
    mutable std::mutex mutex_{};
    std::string errors_{};
};

class temporary_ini {
public:
    explicit temporary_ini(std::string const& content) {
        auto pattern = (std::filesystem::temp_directory_path() / "bootstrap-dq-config-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        auto* directory = ::mkdtemp(buffer.data());
        if (!directory) throw std::runtime_error("cannot create temporary configuration directory");
        directory_ = directory;
        std::ofstream output{path()};
        output << content;
        if (!output) throw std::runtime_error("cannot write temporary configuration");
    }
    ~temporary_ini() {
        std::error_code error{};
        std::filesystem::remove_all(directory_, error);
    }
    [[nodiscard]] std::filesystem::path path() const { return directory_ / "tsurugi.ini"; }
private:
    std::filesystem::path directory_{};
};
} // namespace

TEST(distributed_query_configuration_test, defaults_and_overrides) {
    std::stringstream empty{};
    tateyama::api::configuration::whole defaults{empty, tateyama::configuration::default_configuration()};
    for (auto const* name : {"distributed_query_coordinator", "distributed_query_remote"}) {
        auto* role = defaults.get_section(name);
        ASSERT_NE(nullptr, role);
        EXPECT_EQ(false, role->get<bool>("enabled"));
    }
    auto* client = defaults.get_section("distributed_query_client");
    ASSERT_NE(nullptr, client);
    EXPECT_EQ("", client->get<std::string>("name").value_or(""));
    EXPECT_EQ("", client->get<std::string>("uri").value_or(""));

    EXPECT_EQ(false, client->get<bool>("secure"));
    EXPECT_EQ("grpc_default", client->get<std::string>("root_cert"));

    std::stringstream ss{"[distributed_query_coordinator]\nenabled=true\n[distributed_query_client]\nname=a|b\nuri=dns:///localhost:12345|unix:///tmp/worker.sock\n"};
    tateyama::api::configuration::whole configured{ss, tateyama::configuration::default_configuration()};
    EXPECT_EQ(true, configured.get_section("distributed_query_coordinator")->get<bool>("enabled"));
    EXPECT_EQ(false, configured.get_section("distributed_query_remote")->get<bool>("enabled"));
    auto* configured_client = configured.get_section("distributed_query_client");
    auto names = configured_client->get_vector<std::string>("name", "|");
    auto uris = configured_client->get_vector<std::string>("uri", "|");
    ASSERT_TRUE(names);
    ASSERT_TRUE(uris);
    EXPECT_EQ((std::vector<std::string>{"a", "b"}), *names);
    EXPECT_EQ((std::vector<std::string>{"dns:///localhost:12345", "unix:///tmp/worker.sock"}), *uris);
}

TEST(distributed_query_configuration_test, explicit_tls_lists_are_known_configuration_properties) {
    temporary_ini input{
        "[distributed_query_coordinator]\nenabled=true\n[distributed_query_client]\n"
        "name=first|second\nuri=dns:///first:49345|dns:///second:49346\n"
        "secure=true|false\nroot_cert=/private/ca.pem|grpc_default\n"};
    error_log_capture capture{};
    // Use a real file: configuration validation does not run for the stream constructor.
    api::configuration::whole configured{input.path().string(), configuration::default_configuration()};
    auto* client = configured.get_section("distributed_query_client");
    ASSERT_NE(nullptr, client);
    EXPECT_EQ((std::vector<bool>{true, false}), client->get_vector<bool>("secure", "|"));
    EXPECT_EQ((std::vector<std::string>{"/private/ca.pem", "grpc_default"}),
        client->get_vector<std::string>("root_cert", "|"));
    EXPECT_TRUE(capture.errors().empty()) << capture.errors();
}
} // namespace tateyama::testing
