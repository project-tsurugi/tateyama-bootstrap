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
#include <unistd.h>
#include "test_root.h"

namespace tateyama::testing {

class session_mock_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("session_mock_test", 20006);
        helper_->set_up();
    }

    virtual void TearDown() {
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};

private:
    std::string name_{};
};

TEST_F(session_mock_test, list) {
    std::string command;
    
    command = "tgctl session list --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/list.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl session list" << std::endl;
        FAIL();
    }
    EXPECT_TRUE(validate_json(helper_->abs_path("test/list.log")));
}

TEST_F(session_mock_test, show_by_id) {
    std::string command;
    
    command = "tgctl session show :cafebabe --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/show_by_id.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl session show" << std::endl;
        FAIL();
    }
    EXPECT_TRUE(validate_json(helper_->abs_path("test/show_by_id.log")));
}

TEST_F(session_mock_test, show_by_label) {
    std::string command;
    
    command = "tgctl session show example-1 --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/show_by_label.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl session show" << std::endl;
        FAIL();
    }
    EXPECT_TRUE(validate_json(helper_->abs_path("test/show_by_label.log")));
}

TEST_F(session_mock_test, kill) {
    std::string command;
    
    command = "tgctl session kill example-1 --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/kill.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl session kill" << std::endl;
        FAIL();
    }
    EXPECT_TRUE(validate_json(helper_->abs_path("test/kill.log")));
}

TEST_F(session_mock_test, swtch) {
    std::string command;
    
    command = "tgctl session set example-1 key label --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/set.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl session set" << std::endl;
        FAIL();
    }
    EXPECT_TRUE(validate_json(helper_->abs_path("test/set.log")));
}

}  // namespace tateyama::testing
