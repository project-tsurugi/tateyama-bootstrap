/*
 * Copyright 2022-2025 Project Tsurugi.
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

#include <iostream>
#include <string_view>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <boost/process/system.hpp>
#include <boost/process/io.hpp>
#include <boost/process/pipe.hpp>
#include <boost/iostreams/copy.hpp>

namespace tateyama::configuration {

class config_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("config_test", 20501);
        helper_->set_up("[system]\n    pid_directory=/pid_directory-for-test\n");
    }
    virtual void TearDown() {
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};

    void do_test(const std::string& command, std::stringstream& ss) {
        std::cout << command << std::endl;

        boost::process::opstream enc;
        boost::process::ipstream dec;
        boost::process::child c(command, boost::process::std_in < enc, boost::process::std_out > dec);

        boost::iostreams::copy(dec, ss);
        c.wait();
    }
};

TEST_F(config_test, normal) {
    std::string command;
    command = "tgctl config --conf ";
    command += helper_->conf_file_path();

    std::stringstream ss;
    do_test(command, ss);

    EXPECT_TRUE(ss.str().find("pid_directory=/pid_directory-for-test") != std::string::npos);
}

TEST_F(config_test, monitor) {
    std::string command;
    command = "tgctl config --conf ";
    command += helper_->conf_file_path();
    command += " --quiet --monitor ";
    command += helper_->abs_path("test/config.log");

    std::stringstream ss;
    do_test(command, ss);

    EXPECT_TRUE(validate_json(helper_->abs_path("test/confit.log")));

    FILE *fp;
    command = "grep pid_directory-for-test ";
    command += helper_->abs_path("test/config.log");
    command += " | wc -l";
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot wc" << std::endl;
    }
    int l{};
    int rv = fscanf(fp, "%d", &l);
    EXPECT_EQ(l, 1);
    EXPECT_EQ(rv, 1);
}

}  // namespace tateyama::configuration
