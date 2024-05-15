/*
 * Copyright 2022-2024 Project Tsurugi.
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
#include <sstream>
#include <map>
#include <sys/types.h>
#include <unistd.h>

#include <boost/thread/barrier.hpp>

#include <tateyama/metrics/metrics_metadata.h>
#include <tateyama/proto/metrics/request.pb.h>
#include <tateyama/proto/metrics/response.pb.h>
#include <tateyama/framework/component_ids.h>
#include "tateyama/configuration/bootstrap_configuration.h"
#include "tateyama/test_utils/server_mock.h"

namespace tateyama::test_utils {

template<>
inline void server_mock::request_message<tateyama::proto::metrics::request::List>(tateyama::proto::metrics::request::List& rq) {
    tateyama::proto::metrics::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::metrics::request::Request::CommandCase::kList);
    rq = r.list();
}

template<>
inline void server_mock::request_message<tateyama::proto::metrics::request::Show>(tateyama::proto::metrics::request::Show& rq) {
    tateyama::proto::metrics::request::Request r{};
    auto s = current_request();
    EXPECT_TRUE(r.ParseFromString(s));
    EXPECT_EQ(r.command_case(), tateyama::proto::metrics::request::Request::CommandCase::kShow);
    rq = r.show();
}

}  // tateyama::test_utils


namespace tateyama::metrics {
using namespace std::literals::string_literals;
using metrics_metadata = tateyama::metrics::metrics_metadata;

class metrics_test : public ::testing::Test {
public:
    virtual void SetUp() {
        helper_ = std::make_unique<directory_helper>("metrics_test", 20301);
        helper_->set_up();
        auto bst_conf = tateyama::configuration::bootstrap_configuration::create_bootstrap_configuration(helper_->conf_file_path());
        server_mock_ = std::make_unique<tateyama::test_utils::server_mock>("metrics_test", bst_conf.digest(), sync_);
        sync_.wait();
    }

    virtual void TearDown() {
        helper_->tear_down();
    }

protected:
    std::unique_ptr<directory_helper> helper_{};
    std::unique_ptr<tateyama::test_utils::server_mock> server_mock_{};
    boost::barrier sync_{2};

#if 0
    struct metadata_less {
         bool operator()(const tateyama::metrics::metrics_metadata& a, const tateyama::metrics::metrics_metadata& b) const noexcept {
            if (a.key() < b.key()) {
                return true;
            }
            auto& a_attrs = a.attributes();
            auto& b_attrs = b.attributes();
            if (a_attrs.size() != a_attrs.size()) {
                return a_attrs.size() < b_attrs.size();
            }
            for (std::size_t i = 0; i < a_attrs.size(); i++) {
                if (a_attrs.at(i) < b_attrs.at(i)) {
                    return true;
                }
            }
            return false;
        }
    };
#endif
    std::vector<std::pair<metrics_metadata, double>> metadata_value_{
        {
            {
                "session_count"s, "number of active sessions"s,
                std::vector<std::tuple<std::string, std::string>> {},
                std::vector<std::string> {}
            },
            100
        },
        {
            {
                "index_size"s, "estimated each index size"s,
                std::vector<std::tuple<std::string, std::string>> {
                    std::tuple<std::string, std::string> {"table_name"s, "A"s},
                    std::tuple<std::string, std::string> {"index_name"s, "IA1"}
                },
                std::vector<std::string> {"table_count"s}
            },
            65536
        },
        {
            {
                "index_size"s, "estimated each index size"s,
                std::vector<std::tuple<std::string, std::string>> {
                    std::tuple<std::string, std::string> {"table_name"s, "A"s},
                    std::tuple<std::string, std::string> {"index_name"s, "IA2"}
                },
                std::vector<std::string> {"table_count"s}
            },
            16777216
        },
        {
            {
                "index_size"s, "estimated each index size"s,
                std::vector<std::tuple<std::string, std::string>> {
                    std::tuple<std::string, std::string> {"table_name"s, "B"s},
                    std::tuple<std::string, std::string> {"index_name"s, "IB1"}
                },
                std::vector<std::string> {"table_count"s}
            },
            256
        },
        {
            {
                "storage_log_size"s, "transaction log disk usage"s,
                std::vector<std::tuple<std::string, std::string>> {},
                std::vector<std::string> {}
            },
            65535
        },
        {
            {
                "ipc_buffer_size"s, "allocated buffer size for all IPC sessions"s,
                std::vector<std::tuple<std::string, std::string>> {},
                std::vector<std::string> {}
            },
            1024
        },
        {
            {
                "sql_buffer_size"s, "allocated buffer size for SQL execution engine"s,
                std::vector<std::tuple<std::string, std::string>> {},
                std::vector<std::string> {}
            },
            268435456
        },
        {
            {
                ""s, ""s,
                std::vector<std::tuple<std::string, std::string>> {},
                std::vector<std::string> {}
            },
            0
        }
    };

    std::string read_pipe(FILE* fp) {
        std::stringstream ss{};
        int c{};
        while ((c = std::fgetc(fp)) != EOF) {
            ss << static_cast<char>(c);
        }
        return ss.str();
    }
};

TEST_F(metrics_test, metrics_list) {
    {
        tateyama::proto::metrics::response::MetricsInformation information{};

        auto size = metadata_value_.size();
        std::string prev_key{};
        for (std::size_t i = 0; i < (size - 1); i++) {
            auto&& mev = metadata_value_.at(i);
            if (prev_key != mev.first.key()) {
                auto* item = information.add_items();
                item->set_key(std::string(mev.first.key()));
                item->set_description(std::string(mev.first.description()));
                prev_key = mev.first.key();
            }
        }
        server_mock_->push_response(information.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl dbstats list --format json --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl dbstats list" << std::endl;
    }
    auto result = read_pipe(fp);
//    std::cout << result << std::flush;
    EXPECT_TRUE(validate_json_regular(result));
    EXPECT_NE(std::string::npos, result.find("session_count"));
    EXPECT_NE(std::string::npos, result.find("number of active sessions"));
    EXPECT_NE(std::string::npos, result.find("index_size"));
    EXPECT_NE(std::string::npos, result.find("estimated each index size"));
    EXPECT_NE(std::string::npos, result.find("storage_log_size"));
    EXPECT_NE(std::string::npos, result.find("transaction log disk usage"));
    EXPECT_NE(std::string::npos, result.find("ipc_buffer_size"));
    EXPECT_NE(std::string::npos, result.find("allocated buffer size for all IPC sessions"));
    EXPECT_NE(std::string::npos, result.find("sql_buffer_size"));
    EXPECT_NE(std::string::npos, result.find("allocated buffer size for SQL execution engine"));
  
    EXPECT_EQ(tateyama::framework::service_id_metrics, server_mock_->component_id());
    tateyama::proto::metrics::request::List rq{};
    server_mock_->request_message(rq);
}

TEST_F(metrics_test, metrics_show) {
    {
        tateyama::proto::metrics::response::MetricsInformation information{};

        auto size = metadata_value_.size();
        for (std::size_t i = 0; i < (size - 1); i++) {
            auto&& mev = metadata_value_.at(i);
            auto* item = information.add_items();
            std::string current_key{mev.first.key()};
            item->set_key(current_key);
            item->set_description(std::string(mev.first.description()));

            auto* mv = item->mutable_value();
            if (mev.first.attributes().empty()) {
                mv->set_value(mev.second);
            } else {
                auto* array = mv->mutable_array();
                auto* elements = array->mutable_elements();
                while (true) {
                    auto* element = elements->Add();
                    element->set_value(metadata_value_.at(i).second);
                    auto& mattrs = metadata_value_.at(i).first.attributes();
                    auto* attrmap = element->mutable_attributes();
                    for (auto&& e: mattrs) {
                        (*attrmap)[std::get<0>(e)] = std::get<1>(e);
                    }
                    if (current_key != metadata_value_.at(i+1).first.key()) {
                        break;
                    }
                    i++;
                }
            }
        }
        server_mock_->push_response(information.SerializeAsString());
    }

    std::string command;
    FILE *fp;

    command = "tgctl dbstats show --format json --conf ";
    command += helper_->conf_file_path();
    std::cout << command << std::endl;
    if((fp = popen(command.c_str(), "r")) == nullptr){
        std::cerr << "cannot tgctl dbstats show" << std::endl;
    }
    auto result = read_pipe(fp);
//    std::cout << result << std::flush;
    EXPECT_TRUE(validate_json_regular(result));
    EXPECT_NE(std::string::npos, result.find("session_count"));
    EXPECT_NE(std::string::npos, result.find("100"));

    EXPECT_NE(std::string::npos, result.find("index_size"));
    EXPECT_NE(std::string::npos, result.find("table_name"));
    EXPECT_NE(std::string::npos, result.find("index_name"));
    EXPECT_NE(std::string::npos, result.find("value"));
    EXPECT_NE(std::string::npos, result.find("A"));
    EXPECT_NE(std::string::npos, result.find("IA1"));
    EXPECT_NE(std::string::npos, result.find("65536"));
    EXPECT_NE(std::string::npos, result.find("IA2"));
    EXPECT_NE(std::string::npos, result.find("16777216"));
    EXPECT_NE(std::string::npos, result.find("B"));
    EXPECT_NE(std::string::npos, result.find("IB1"));
    EXPECT_NE(std::string::npos, result.find("256"));

    EXPECT_NE(std::string::npos, result.find("storage_log_size"));
    EXPECT_NE(std::string::npos, result.find("65535"));
    EXPECT_NE(std::string::npos, result.find("ipc_buffer_size"));
    EXPECT_NE(std::string::npos, result.find("1024"));
    EXPECT_NE(std::string::npos, result.find("sql_buffer_size"));
    EXPECT_NE(std::string::npos, result.find("268435456"));

    EXPECT_EQ(tateyama::framework::service_id_metrics, server_mock_->component_id());
    tateyama::proto::metrics::request::Show rq{};
    server_mock_->request_message(rq);
}

}  // namespace tateyama::metrics
