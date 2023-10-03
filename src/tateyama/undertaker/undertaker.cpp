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
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>

int main([[maybe_unused]] int argc, char* argv[]) {
    char *endptr{};
    auto pid = static_cast<pid_t>(strtol(*(argv+1), &endptr, 10));
    int status{};

    waitpid(pid, &status, 0);
    return 0;
}
