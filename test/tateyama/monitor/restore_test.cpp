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

class restore_test : public ::testing::Test {
public:
    virtual void SetUp() {
        set_up();

        std::string command;
    
        command = "oltp start --conf ";
        command += DIRECTORY;
        command += "/tateyama/configuration/tsurugi.ini";
        std::cout << command << std::endl;
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot oltp start" << std::endl;
            FAIL();
        }

        command = "oltp backup create ";
        command += dir_name("backup");
        command += " --conf ";
        command += DIRECTORY;
        command += "/tateyama/configuration/tsurugi.ini ";
        std::cout << command << std::endl;
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot oltp backup" << std::endl;
            FAIL();
        }

        command = "oltp shutdown --conf ";
        command += DIRECTORY;
        command += "/tateyama/configuration/tsurugi.ini";
        std::cout << command << std::endl;
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot oltp shutdown" << std::endl;
            FAIL();
        }
    }

    virtual void TearDown() {
        tear_down();
    }
    const char* file_name(const char* name) {
        name_ = location;
        name_ += "/";
        name_ += name;
        return name_.c_str();
    }

private:
    std::string name_{};
};

TEST_F(restore_test, begin) {
    std::string command;
    
    command = "oltp restore backup ";
    command += dir_name("backup");
    command += " --conf ";
    command += DIRECTORY;
    command += "/tateyama/configuration/tsurugi.ini --monitor ";
    command += file_name("restore.log");
    std::cout << command << std::endl;
    if (system(command.c_str()) != 0) {
        std::cerr << "cannot oltp backup" << std::endl;
        FAIL();
    }
    
    FILE *fp;
    command = "wc -l ";
    command += file_name("restore.log");
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
