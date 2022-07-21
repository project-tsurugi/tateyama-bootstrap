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
        set_up();
    }

    virtual void TearDown() {
        tear_down();
    }
    const char* log_file(const char* name) {
        name_ = dir_name("log/");
        name_ += name;
        return name_.c_str();
    }

private:
    std::string name_{};
};

TEST_F(start_stop_test, success) {
    std::string command;
    
    command = "oltp start --conf ";
    command += DIRECTORY;
    command += "/tateyama/configuration/tsurugi.ini --monitor ";
    command += log_file("start.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp start" << std::endl;
        FAIL();
    }
    
    FILE *fp;
    command = "wc -l ";
    command += log_file("start.log");
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }

    int l;
    auto rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 2);
    EXPECT_EQ(rv, 1);

    command = "oltp shutdown --conf ";
    command += DIRECTORY;
    command += "/tateyama/configuration/tsurugi.ini --monitor ";
    command += log_file("shutdown.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp shutdown" << std::endl;
        FAIL();
    }

    command = "wc -l ";
    command += log_file("shutdown.log");
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }
}

TEST_F(start_stop_test, fail) {
    std::string command;
    
    command = "oltp start --conf ";
    command += DIRECTORY;
    command += "/tateyama/configuration/tsurugi.ini";
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp start" << std::endl;
        FAIL();
    }

    command = "oltp start --conf ";
    command += DIRECTORY;
    command += "/tateyama/configuration/tsurugi.ini --monitor ";
    command += log_file("start.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp start" << std::endl;
        FAIL();
    }
    
    FILE *fp;
    command = "grep fail ";
    command += log_file("start.log");
    command += " | wc -l";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot grep and wc" << std::endl;
    }

    int l;
    auto rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);

    command = "oltp shutdown --conf ";
    command += DIRECTORY;
    command += "/tateyama/configuration/tsurugi.ini";
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp shutdown" << std::endl;
        FAIL();
    }
}

}  // namespace tateyama::testing
