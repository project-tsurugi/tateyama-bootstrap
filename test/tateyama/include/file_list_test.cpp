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

#include "tateyama/datastore/file_list.h"

namespace tateyama::testing {

class file_list_test : public ::testing::Test {
};

TEST_F(file_list_test, simple) {
    const char* source[] = { "example_source1", "example_source2", "example_source3" };
    const char* destination[] = { "example_destination1", "example_destination2", "example_destination3" };
    const bool detached[] = { true, false, true };

    auto parser = std::make_unique<tateyama::datastore::file_list>();
    EXPECT_TRUE(parser->read_json("../../test/tateyama/include/json/file_list.json"));

    int i = 0;
    parser->for_each([source, destination, detached, &i](const std::string src, const std::string dst, bool det) {
                         EXPECT_EQ(src, source[i]);
                         EXPECT_EQ(dst, destination[i]);
                         EXPECT_EQ(det, detached[i]);
                         i++;
                     });
}

}  // namespace tateyama::testing
