/*
 * Copyright 2022-2022 Project Tsurugi.
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
#include <unistd.h>
#include <stdlib.h>
#include "test_root.h"
#include "tateyama/configuration/bootstrap_configuration.h"

namespace tateyama::testing {

class bootstrap_configuration_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("bootstrap_configuration_test/var/etc", 20500, true);
        helper_->set_up();
        std::cout << helper_->conf_file_path() << std::endl;
    }

    virtual void TearDown() {
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};
};

TEST_F(bootstrap_configuration_test, conf) {
    setenv("TSURUGI_CONF", "/tmp/bootstrap_configuration_test/var/etc/tsurugi.ini", 1);
    unsetenv("TSURUGI_HOME");
    auto conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration("");
    
    EXPECT_EQ(conf.conf_file().string(), "/tmp/bootstrap_configuration_test/var/etc/tsurugi.ini");
    auto tateyama_conf = conf.get_configuration();
    EXPECT_EQ(tateyama_conf->base_path(), std::nullopt);
}

TEST_F(bootstrap_configuration_test, conf_none) {
    setenv("TSURUGI_CONF", "/tmp/bootstrap_configuration_test/var/etc_not_exist/tsurugi.ini", 1);
    setenv("TSURUGI_HOME", "/tmp/bootstrap_configuration_test", 1);
    auto conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration("");
    
    EXPECT_EQ(conf.conf_file().string(), "");
}

TEST_F(bootstrap_configuration_test, home) {
    unsetenv("TSURUGI_CONF");
    setenv("TSURUGI_HOME", "/tmp/bootstrap_configuration_test", 1);
    auto conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration("");
    
    EXPECT_EQ(conf.conf_file().string(), "/tmp/bootstrap_configuration_test/var/etc/tsurugi.ini");
    auto tateyama_conf = conf.get_configuration();
    EXPECT_EQ(tateyama_conf->base_path().value(), std::filesystem::path("/tmp/bootstrap_configuration_test"));
}

TEST_F(bootstrap_configuration_test, home_none) {
    unsetenv("TSURUGI_CONF");
    setenv("TSURUGI_HOME", "/tmp/bootstrap_configuration_test_not_exist", 1);
    auto conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration("");
    
    EXPECT_EQ(conf.conf_file().string(), "");
}

TEST_F(bootstrap_configuration_test, both_none) {
    unsetenv("TSURUGI_CONF");
    unsetenv("TSURUGI_HOME");
    auto conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration("");

    EXPECT_EQ(conf.conf_file().string(), "");
}

}  // namespace tateyama::testing


namespace tateyama::configuration {

std::string_view default_property_for_bootstrap() {
    return
    "[ipc_endpoint]\n"
        "database_name=tsurugi\n"
        "threads=104\n"
        "datachannel_buffer_size=64\n"

    "[stream_endpoint]\n"
        "port=12345\n"
        "threads=104\n"

    "[datastore]\n"
        "log_location=\n"
        "logging_max_parallelism=112\n"

    "[system]\n"
        "pid_directory = /tmp\n";
}

}  // namespace tateyama::configuration
