/*
 * Copyright 2019-2019 tsurugi project.
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
#include <iostream>
#include <unistd.h>
#include <string.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_string(config, "", "the directory where the configuration file is");  // NOLINT


namespace tateyama::oltp {

int oltp_start([[maybe_unused]] int argc, char* argv[]) {
    if (auto pid = fork(); pid == 0) {
        auto cmd = "tateyama-server";
        *argv = const_cast<char *>(cmd);
        execvp(cmd, argv);
    } else {
        std::cout << "pid = " << pid << std::endl;
    }
    return 0;
}

int oltp_main([[maybe_unused]] int argc, char* argv[]) {
    if (strcmp(*(argv + 1), "start") == 0) {
        return oltp_start(argc - 1, argv + 1);
    }
    return -1;
}

}  // tateyama::oltp


int main(int argc, char* argv[]) {
    if (argc > 1) {
        return tateyama::oltp::oltp_main(argc, argv);
    }
    return -1;
}
