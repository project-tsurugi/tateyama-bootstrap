/*
 * Copyright 2022-2023 Project Tsurugi.
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

#include "tateyama/version/version.h"

namespace tateyama::testing {

class version_test : public ::testing::Test {
};

TEST_F(version_test, read) {
    std::ifstream fs{};
    auto file = std::filesystem::path("../../test/tateyama/version/json/tsurugi-info.json");
    if (!std::filesystem::exists(file)) {
        FAIL();
    }
    fs = std::ifstream{file.c_str()};

    std::ostringstream os{};
    tateyama::version::do_show_version(fs, os);

    auto result = os.str();

    auto i1 = result.find("tsurugidb");
    auto i2 = result.find("version: snapshot-202309301233-c415a69");
    auto i3 = result.find("date: 202309301233");

    EXPECT_NE(std::string::npos, i1);
    EXPECT_NE(std::string::npos, i2);
    EXPECT_NE(std::string::npos, i3);
    EXPECT_TRUE(i1 < i2);
    EXPECT_TRUE(i2 < i3);
}

}  // namespace tateyama::testing
