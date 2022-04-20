/*
 * Copyright 2019-2022 tsurugi project.
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
#pragma once

#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/filesystem/path.hpp>

#include <tateyama/api/environment.h>
#include <tateyama/api/configuration.h>

namespace tateyama::server {

class proc_mutex {
  public:
    const boost::filesystem::path LOCK_FILE_NAME{"tsurugi.pid"};
    
    proc_mutex(std::shared_ptr<tateyama::api::environment> env) : env_(env) {
        if ((fd_ = open((env_->configuration()->get_directory() / LOCK_FILE_NAME).generic_string().c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
            perror("open");
            exit(-1);
        }
    }

    bool lock() {
        if (flock(fd_, LOCK_EX | LOCK_NB) == 0) {
            if (ftruncate(fd_, 0) < 0) {
                perror("ftruncate");
                exit(-1);
            }
            std::string pid = std::to_string(getpid());
            if (write(fd_, pid.data(), pid.length()) < 0) {
                perror("write");
                exit(-1);
            }
            return true;
        }
        return false;
    }
    void unlock() {
        flock(fd_, LOCK_UN);
    }
  private:
    std::shared_ptr<tateyama::api::environment> env_;
    int fd_{};
};

}  // tateyama::server;
