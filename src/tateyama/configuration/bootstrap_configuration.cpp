/*
 * Copyright 2018-2025 Project Tsurugi.
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
        "enable_index_join=true\n"
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
        "dev_profile_commits=false\n"
        "dev_return_os_pages=false\n"
        "dev_omit_task_when_idle=true\n"
        "plan_recording=false\n"
        "dev_try_insert_on_upserting_secondary=true\n"
        "dev_scan_concurrent_operation_as_not_found=true\n"
        "dev_point_read_concurrent_operation_as_not_found=true\n"
        "lowercase_regular_identifiers=false\n"
        "scan_block_size=100\n"
        "scan_yield_interval=1\n"
        "dev_thousandths_ratio_check_local_first=100\n"
        "dev_direct_commit_callback=false\n"
        "scan_default_parallel=4\n"
        "dev_inplace_teardown=true\n"
        "dev_inplace_dag_schedule=true\n"
        "enable_join_scan=true\n"
        "dev_rtx_key_distribution=uniform\n"
        "dev_enable_blob_cast=true\n"
        "max_result_set_writers=64\n"
        "dev_initial_core=\n"
        "dev_core_affinity=false\n"
        "dev_assign_numa_nodes_uniformly=false\n"
        "dev_force_numa_node=\n"
        "dev_log_msg_user_data=false\n"

    "[ipc_endpoint]\n"
        "database_name=tsurugi\n"
        "threads=104\n"
        "datachannel_buffer_size=64\n"
        "max_datachannel_buffers=256\n"
        "admin_sessions=1\n"
        "allow_blob_privileged=true\n"

    "[stream_endpoint]\n"
        "enabled=false\n"
        "port=12345\n"
        "threads=104\n"
        "allow_blob_privileged=false\n"
        "dev_idle_work_interval=1000\n"

    "[session]\n"
        "enable_timeout=true\n"
        "refresh_timeout=300\n"
        "max_refresh_timeout=86400\n"
        "zone_offset=\n"
        "authentication_timeout=0\n"

    "[datastore]\n"
        "log_location=\n"
        "logging_max_parallelism=112\n"
        "recover_max_parallelism=8\n"

    "[cc]\n"
        "epoch_duration=3000\n"
        "waiting_resolver_threads=2\n"
        "max_concurrent_transactions=\n"
        "index_restore_threads=4\n"

    "[system]\n"
        "pid_directory=/var/lock\n"

    "[authentication]\n"
        "enabled=false\n"
        "url=http://localhost:8080/harinoki\n"
        "request_timeout=0\n"
        "administrators=*\n"

    "[grpc_server]\n"
        "enabled=true\n"
        "endpoint=dns:///localhost:52345\n"
        "secure=false\n"

    "[blob_relay]\n"
        "enabled=true\n"
        "session_store=var/blob/sessions\n"
        "session_quota_size=\n"
        "local_enabled=true\n"
        "local_upload_copy_file=false\n"
        "stream_chunk_size=1048576\n"

    "[glog]\n"
        "dummy=\n"  // just for retain glog section in default configuration

#ifdef ENABLE_ALTIMETER
    "[event_log]\n"
        "output=true\n"
        "directory=/var/log/altimeter/event\n"
        "level=50\n"
        "file_number=10\n"
        "sync=false\n"
        "buffer_size=52428800\n"
        "flush_interval=10000\n"
        "flush_file_size=1048576\n"
        "max_file_size=1073741824\n"
        "stmt_duration_threshold = 1000000000\n"

    "[audit_log]\n"
        "output=true\n"
        "directory=/var/log/altimeter/audit\n"
        "level=50\n"
        "file_number=10\n"
        "sync=true\n"
        "buffer_size=0\n"
        "flush_interval=0\n"
        "flush_file_size=0\n"
        "max_file_size=1073741824\n"
#endif

    "[udf]\n"
        "plugin_directory=var/plugins/\n"
        "endpoint=dns:///localhost:50051\n"
        "secure=false\n"
};

} // namespace details


std::string_view default_configuration() {
    return details::default_configuration;
}

} // tateyama::api::configuration
