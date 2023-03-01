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

class status_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("status_test", 20004);
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

TEST_F(status_test, success) {
    std::string command;
    
    // check stop
    command = "oltp status --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/stop.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp status" << std::endl;
        FAIL();
    }
    EXPECT_TRUE(validate_json(helper_->abs_path("test/stop.log")));

    FILE *fp;
    command = "grep stop ";
    command += helper_->abs_path("test/stop.log");
    command += " | wc -l";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }
    int l, rv;
    rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);


    // check running
    command = "oltp start --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp start" << std::endl;
        FAIL();
    }

    bool go_ahead = false;
    for (std::size_t i = 0; i < 10; i++) {
        command = "oltp status --conf ";
        command += helper_->conf_file_path();
        command += " --monitor ";
        command += helper_->abs_path("test/running.log");
        std::cout << command << std::endl;
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot oltp status" << std::endl;
            FAIL();
        }
        EXPECT_TRUE(validate_json(helper_->abs_path("test/running.log")));

        command = "grep starting ";
        command += helper_->abs_path("test/running.log");
        command += " | wc -l";
        if((fp = popen(command.c_str(), "r")) == nullptr){
            std::cerr << "cannot wc" << std::endl;
        }
        rv = fscanf(fp, "%d", &l);
        if (l == 0 && rv == 1) {
            go_ahead = true;
            break;
        }
        usleep(5 * 1000);
    }
    if (!go_ahead) {
        FAIL();
    }

    command = "grep running ";
    command += helper_->abs_path("test/running.log");
    command += " | wc -l";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }
    rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);

    command = "oltp shutdown --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp shutdown" << std::endl;
        FAIL();
    }
}

}  // namespace tateyama::testing
