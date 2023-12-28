/*
 * Copyright 2018-2023 Project Tsurugi.
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

#include <string_view>

namespace tateyama::configuration {

namespace details {

static constexpr std::string_view default_configuration {  // NOLINT
    "[sql]\n"
        "thread_pool_size=\n"
        "enable_index_join=false\n"
        "stealing_enabled=true\n"
        "default_partitions=5\n"
        "stealing_wait=1\n"
        "task_polling_wait=0\n"
        "lightweight_job_level=0\n"
        "enable_hybrid_scheduler=true\n"
        "busy_worker=false\n"
        "watcher_interval=1000\n"
        "worker_try_count=1000\n"
        "worker_suspend_timeout=1000000\n"
        "commit_response=STORED\n"
        "dev_update_skips_deletion=false\n"
        "dev_profile_commits=false\n"
        "dev_return_os_pages=false\n"

    "[ipc_endpoint]\n"
        "database_name=tsurugi\n"
        "threads=104\n"
        "datachannel_buffer_size=64\n"
        "max_datachannel_buffers=256\n"

    "[stream_endpoint]\n"
        "port=12345\n"
        "threads=104\n"

    "[datastore]\n"
        "log_location=\n"
        "logging_max_parallelism=112\n"
        "recover_max_parallelism=8\n"

    "[cc]\n"
        "epoch_duration=3000\n"
        "waiting_resolver_threads=2\n"

    "[system]\n"
        "pid_directory=/var/lock\n"

    "[glog]\n"
        "dummy=\n"  // just for retain glog section in default configuration
};

} // namespace details


std::string_view default_property_for_bootstrap() {
    return details::default_configuration;
}

} // tateyama::api::configuration
