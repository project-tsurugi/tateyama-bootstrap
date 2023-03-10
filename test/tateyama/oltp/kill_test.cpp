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

#include "client_wire.h"
#include <boost/filesystem.hpp>

namespace tateyama::testing {

class kill_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("kill_test", 20101);
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

TEST_F(kill_test, ipc_file) {
    {
        auto info = boost::filesystem::space("/tmp/");
        std::cout << "/tmp/: free " << info.free << "B, " << info.free / 1024 / 1024 << "MiB" << std::endl;
    }
    std::string command;
    
    command = "cat ";
    command += helper_->conf_file_path();
    std::cout << "==== check tsurugi.conf ===" << std::endl;
    std::cout << command << std::endl;
    EXPECT_EQ(system(command.c_str()), 0);
    std::cout << "==== check end ===" << std::endl;
    
    command = "ls -Rl ";
    command += helper_->abs_path("");
    std::cout << "==== check /tmp/kill_test files ===" << std::endl;
    std::cout << command << std::endl;
    EXPECT_EQ(system(command.c_str()), 0);
    std::cout << "==== check end ===" << std::endl;
           
    command = "oltp start --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    EXPECT_EQ(system(command.c_str()), 0);
    
    auto wire = tateyama::common::wire::session_wire_container(tateyama::common::wire::connection_container("kill_test").connect());

    command = "ls /dev/shm/kill_test";
    std::cout << command << std::endl;
    EXPECT_EQ(system(command.c_str()), 0);

    command = "ls /dev/shm/kill_test-1";
    std::cout << command << std::endl;
    EXPECT_EQ(system(command.c_str()), 0);

    command = "oltp kill --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    EXPECT_EQ(system(command.c_str()), 0);

    command = "ls /dev/shm/kill_test";
    std::cout << command << std::endl;
    EXPECT_NE(system(command.c_str()), 0);

    command = "ls /dev/shm/kill_test-1";
    std::cout << command << std::endl;
    EXPECT_NE(system(command.c_str()), 0);
}

}  // namespace tateyama::testing
