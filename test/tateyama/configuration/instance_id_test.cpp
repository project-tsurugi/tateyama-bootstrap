/*
 * Copyright 2022-2026 Project Tsurugi.
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

#include <string>

#include "tateyama/configuration/bootstrap_configuration.h"

namespace tateyama::configuration {

class instance_id_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("instance_id_test", 20511);
    }
    virtual void TearDown() {
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};
};

TEST_F(instance_id_test, normal) {
    helper_->set_up("[system]\n    instance_id=instance-id-for-test\n");
    auto conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration();

    auto* section = conf->get_section("system");
    if (auto instance_id_opt = section->get<std::string>("instance_id"); instance_id_opt) {
        EXPECT_EQ("instance-id-for-test", instance_id_opt.value());
    } else {
        throw std::runtime_error("instance_id is not given in tsurugi.ini");
    }
}

TEST_F(instance_id_test, upper_case) {
    helper_->set_up("[system]\n    instance_id=INSTANCE-ID-FOR-TEST\n");
    auto conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration();

    auto* section = conf->get_section("system");
    if (auto instance_id_opt = section->get<std::string>("instance_id"); instance_id_opt) {
        EXPECT_EQ("instance-id-for-test", instance_id_opt.value());
    } else {
        throw std::runtime_error("instance_id is not given in tsurugi.ini");
    }
}

TEST_F(instance_id_test, empty) {
    helper_->set_up();
    auto conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration();

    auto* section = conf->get_section("system");
    if (auto instance_id_opt = section->get<std::string>("instance_id"); instance_id_opt) {
        static constexpr std::size_t MAX_INSTANCE_ID_LENGTH = 63;
        std::array<char, MAX_INSTANCE_ID_LENGTH> hostname{};
        if (gethostname(hostname.data(), MAX_INSTANCE_ID_LENGTH) != 0) {
            FAIL();
        }
        EXPECT_EQ(hostname.data(), instance_id_opt.value());
    } else {
        FAIL();
    }
}

TEST_F(instance_id_test, begins_with_hyphon) {
    helper_->set_up("[system]\n    instance_id=-instance-id-for-test\n");
    EXPECT_THROW(tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration(), std::runtime_error);
}

TEST_F(instance_id_test, ends_with_hyphon) {
    helper_->set_up("[system]\n    instance_id=instance-id-for-test-\n");
    EXPECT_THROW(tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration(), std::runtime_error);
}

TEST_F(instance_id_test, double_hyphon) {
    helper_->set_up("[system]\n    instance_id=instance-id--for-test-\n");
    EXPECT_THROW(tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration(), std::runtime_error);
}

TEST_F(instance_id_test, illegal_char) {
    helper_->set_up("[system]\n    instance_id=instance_id_for_test\n");
    EXPECT_THROW(tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration(), std::runtime_error);
}

TEST_F(instance_id_test, within_length_limit) {
    helper_->set_up("[system]\n    instance_id=instance-id2345678921234567893123456789412345678951234567896123\n");
    EXPECT_NO_THROW(tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration());
}

TEST_F(instance_id_test, over_length_limit) {
    helper_->set_up("[system]\n    instance_id=instance-id23456789212345678931234567894123456789512345678961234\n");
    EXPECT_THROW(tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path()).get_configuration(), std::runtime_error);
}

}  // namespace tateyama::configuration
