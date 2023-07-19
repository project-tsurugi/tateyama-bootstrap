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

class start_error_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("start_error_test", 20100);
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

TEST_F(start_error_test, dir) {
    std::string command;
    
    command = "tgctl start --conf /tmp";
    command += " --monitor ";
    command += helper_->abs_path("test/dir.log");
    std::cout << command << std::endl;
    EXPECT_NE(system(command.c_str()), 0);
    EXPECT_TRUE(validate_json(helper_->abs_path("test/dir.log")));
    
    FILE *fp;
    command = "grep fail ";
    command += helper_->abs_path("test/dir.log");
    command += " | wc -l";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot grep and wc" << std::endl;
    }

    int l;
    auto rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);
}

TEST_F(start_error_test, dir_and_slash) {
    std::string command;
    
    command = "tgctl start --conf /tmp/";
    command += " --monitor ";
    command += helper_->abs_path("test/dir.log");
    std::cout << command << std::endl;
    EXPECT_NE(system(command.c_str()), 0);
    EXPECT_TRUE(validate_json(helper_->abs_path("test/dir.log")));
    
    FILE *fp;
    command = "grep fail ";
    command += helper_->abs_path("test/dir.log");
    command += " | wc -l";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot grep and wc" << std::endl;
    }

    int l;
    auto rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);
}

TEST_F(start_error_test, end_slash) {
    std::string command;
    
    command = "tgctl start --conf ";
    command += helper_->conf_file_path();
    command += "/ --monitor ";
    command += helper_->abs_path("test/dir.log");
    std::cout << command << std::endl;
    EXPECT_NE(system(command.c_str()), 0);
    EXPECT_TRUE(validate_json(helper_->abs_path("test/dir.log")));
    
    FILE *fp;
    command = "grep fail ";
    command += helper_->abs_path("test/dir.log");
    command += " | wc -l";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot grep and wc" << std::endl;
    }

    int l;
    auto rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);
}

}  // namespace tateyama::testing
