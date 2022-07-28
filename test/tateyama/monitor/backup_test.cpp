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

class backup_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("backup_test", 20001);
        helper_->set_up();
        std::string command;
    
        command = "oltp start --conf ";
        command += helper_->conf_file_path();
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot oltp start" << std::endl;
            FAIL();
        }
    }

    virtual void TearDown() {
        std::string command;

        command = "oltp shutdown --conf ";
        command += helper_->conf_file_path();
        std::cout << command << std::endl;
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot oltp shutdown" << std::endl;
            FAIL();
        }

        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};
    
private:
    std::string name_{};
};

TEST_F(backup_test, begin) {
    std::string command;
    FILE *fp;
    int l;
    int rv;
    
    command = "oltp backup create ";
    command += helper_->abs_path("backup");
    command += " --conf ";
    command += helper_->conf_file_path();
    command += " --monitor ";
    command += helper_->abs_path("test/backup_create.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp backup" << std::endl;
        FAIL();
    }
    EXPECT_TRUE(validate_json(helper_->abs_path("test/backup_create.log")));
    
    command = "grep start ";
    command += helper_->abs_path("test/backup_create.log");
    command += " | wc -l ";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }

    rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);


    command = "grep finish ";
    command += helper_->abs_path("test/backup_create.log");
    command += " | wc -l ";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }

    rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);


    command = "grep progress ";
    command += helper_->abs_path("test/backup_create.log");
    command += " | wc -l ";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }

    rv = fscanf(fp, "%d", &l);
    EXPECT_TRUE(l >= 1);
    EXPECT_EQ(rv, 1);
}

}  // namespace tateyama::testing
