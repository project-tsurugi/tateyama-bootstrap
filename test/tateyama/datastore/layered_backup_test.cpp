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

#include <string>
#include <vector>
#include <iostream>

#include <boost/thread/barrier.hpp>

#include <tateyama/proto/datastore/request.pb.h>
#include <tateyama/proto/datastore/response.pb.h>
#include <tateyama/framework/component_ids.h>
#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/test_utils/server_mock.h"

namespace tateyama::datastore {

class layered_backup_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("layered_backup_test", 20501);
        helper_->set_up();
    }

    virtual void TearDown() {
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};

    std::string read_pipe(FILE* fp) {
        std::stringstream ss{};
        int c{};
        while ((c = std::fgetc(fp)) != EOF) {
            ss << static_cast<char>(c);
        }
        return ss.str();
    }
};

TEST_F(layered_backup_test, backup) {
    std::string command;
    FILE *fp;

    std::vector<std::string> files{
        "epoch",
        "pwal_0000",
        "blob/dir_01/0000000000000001.blob",
        "compaction_catalog",
        "pwal_0001",
        "limestone-manifest.json"
    };

    // setup
    {
        std::string testdata = {
            "H4sICBu8v2cAA3Rlc3RkYXRhLnRhcgDt2E1vmzAYB3AadZd8CpTjtBkbMEi70TRbK7WjmnLYDbmU\n"
            "tmy8CWh3mPbdBy1sa1iHosWmW/8/KXGITWz08BjHxNCkow3X5W3JXE5/LXsas13uuhZv3jXKGHVt\n"
            "Tefyh6ZpN1UtSl3XbtLoSmTR4+1G6v9RxAjrMpF7E2wff5PaFPFXgRhRkYfXUvtoA+w49uPxZ2wz\n"
            "/oxyTadSR9V55vHffzn1CGBKxCi+iCS4m4NlGc1/7mzmv22ayH8VZn3+783ao+7V+kz7qv2uvFUS\n"
            "EFCJGOdJfv701n/MxPpPhS7+F3EZUCbpNtg+/hZ1sf5X4mH86UOMtJV/3cfo89/cXP9zmzI8/1Vo\n"
            "A6zHWVzHItEvRC2mHhAoRYw26E/v+d/+/8f8L18X/yoTRXWd11L62H7/x+JNgflfgdl4E/iPESPM\n"
            "00KEdZxnQdhMBUl+tes+xvKfDdZ/tmMh/5VY+qdn3nJ97L8Plt7aO/HfBUcr73D1YX7qfQxWZ/7y\n"
            "KDg+1Ond4cGJf3B/9JvT3vr+ujlt6guCrfzc/2XS+hjNfz5Y/9nMQf6r8If9XzbY/2U/Ws8Hrc2+\n"
            "6kXf2txrf1bKqGFXiJHEaVTVeRa9TkUWXzafyacqz3bYx2j+U2tz/4c3zZH/Cnyd643FZV6mog5u\n"
            "o7JqFoKLN/qCEbp4dV9ZtN9WdZTVwaCdOf829SUAAAAAAAAAAAAAAAAAAAAAAAAAAAA8G98BNqYZ\n"
            "zgBQAAA=\n" };

        try {
            std::ofstream mf(helper_->abs_path("test_data"));
            mf.exceptions(std::ios_base::failbit);
            mf << testdata;
            mf.close();
        } catch (const std::exception& e) {
            std::cerr << "error in limestone-manifest.json" << std::endl;
            FAIL();
        }
        command = "base64 -d ";
        command += helper_->abs_path("test_data");
        command += " | tar xzf - -C ";
        command += helper_->abs_path("log");
        if (system(command.c_str()) != 0) {
            std::cerr << "cannot make directory" << std::endl;
            FAIL();
        }
    }
    {
        command = "tgctl  backup create ";
        command += helper_->abs_path("backup");
        command += " --conf ";
        command += helper_->conf_file_path();
        std::cout << command << std::endl;
        if((fp = popen(command.c_str(), "r")) == nullptr){
            std::cerr << "cannot tgctl backup create" << std::endl;
        }
        auto result = read_pipe(fp);

        for(auto&& f : files) {
            EXPECT_TRUE(std::filesystem::exists(helper_->abs_path("backup/" + f)));
        }
    }
}

}  // namespace tateyama::datastore
