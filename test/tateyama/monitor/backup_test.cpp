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
        set_up();

        std::string command;
    
        command = "oltp start --conf ";
        command += DIRECTORY;
        command += "/tateyama/configuration/tsurugi.ini";
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot oltp start" << std::endl;
            FAIL();
        }
    }

    virtual void TearDown() {
        tear_down();

        std::string command;

        command = "oltp shutdown --conf ";
        command += DIRECTORY;
        command += "/tateyama/configuration/tsurugi.ini";
        std::cout << command << std::endl;
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot oltp shutdown" << std::endl;
            FAIL();
        }
    }
    const char* tmp_file(const char* name) {
        name_ = location;
        name_ += "/";
        name_ += name;
        return name_.c_str();
    }

private:
    std::string name_{};
};

TEST_F(backup_test, begin) {
    std::string command;
    
    command = "oltp backup create ";
    command += location;
    command += " --conf ";
    command += DIRECTORY;
    command += "/tateyama/configuration/tsurugi.ini --monitor ";
    command += tmp_file("backup_create.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp backup" << std::endl;
        FAIL();
    }
    
    FILE *fp;
    command = "wc -l ";
    command += tmp_file("backup_create.log");
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }

    int l;
    auto rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 2);
    EXPECT_EQ(rv, 1);
}

}  // namespace tateyama::testing
