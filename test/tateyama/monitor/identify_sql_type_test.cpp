/*
 * Copyright 2022-2022 Project Tsurugi.
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

#include "tateyama/monitor/constants_request.h"

namespace tateyama::testing {

class identify_sql_type_test : public ::testing::Test {
protected:
    std::vector<std::pair<std::string, tateyama::monitor::type_of_sql>> test_patterns_ = {
        { "SeLeCt 1", tateyama::monitor::type_of_sql::query },
        { " iNsErT INTO table (c1, c2) VALUES (11, 22)", tateyama::monitor::type_of_sql::statement },
        { "\tUpDaTe table SET c1=111, c2=222", tateyama::monitor::type_of_sql::statement },
        { "dElEtE FROM table WHERE c1=11", tateyama::monitor::type_of_sql::statement },
        { "  BeGiN ", tateyama::monitor::type_of_sql::begin },
        { "cOmMiT ", tateyama::monitor::type_of_sql::commit },
        { "RoLlBaCk ", tateyama::monitor::type_of_sql::rollback },
        { "pRePaRe ", tateyama::monitor::type_of_sql::prepare },
        { "ExPlAiN ", tateyama::monitor::type_of_sql::explain },
        { "dUmP ", tateyama::monitor::type_of_sql::dump },
        { "LoAd ", tateyama::monitor::type_of_sql::load },
    };
};

TEST_F(identify_sql_type_test, basic) {
    for( auto&& e : test_patterns_ ) {
        auto type = tateyama::monitor::identify_sql_type(e.first);
        std::cout << e.first << " : " << tateyama::monitor::to_string_view(type) << std::endl;
        EXPECT_EQ(e.second, type);
    }
}

}  // namespace tateyama::testing
