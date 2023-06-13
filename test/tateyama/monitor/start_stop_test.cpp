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

class start_stop_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("start_stop_test", 20003);
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

TEST_F(start_stop_test, success) {
    std::string command;
    
    command = "tgctl start --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/start.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl start" << std::endl;
        FAIL();
    }
    helper_->confirm_started();
    EXPECT_TRUE(validate_json(helper_->abs_path("test/start.log")));
    
    FILE *fp;
    command = "wc -l ";
    command += helper_->abs_path("test/start.log");
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }

    int l;
    auto rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 2);
    EXPECT_EQ(rv, 1);

    command = "tgctl shutdown --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/shutdown.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl shutdown" << std::endl;
        FAIL();
    }
    EXPECT_TRUE(validate_json(helper_->abs_path("test/shutdown.log")));

    command = "wc -l ";
    command += helper_->abs_path("test/shutdown.log");
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }
}

TEST_F(start_stop_test, start_twice) {  // issue_109
    std::string command;
    
    command = "tgctl start --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl start" << std::endl;
        FAIL();
    }
    helper_->confirm_started();

    command = "tgctl start --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/start_twice.log");
    std::cout << command << std::endl;
    EXPECT_EQ(system(command.c_str()), 0);
    EXPECT_TRUE(validate_json(helper_->abs_path("test/start_twice.log")));
    
    FILE *fp;
    command = "grep success ";
    command += helper_->abs_path("test/start_twice.log");
    command += " | wc -l";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot grep and wc" << std::endl;
    }

    int l;
    auto rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);

    command = "tgctl shutdown --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot tgctl shutdown" << std::endl;
        FAIL();
    }
}

}  // namespace tateyama::testing
