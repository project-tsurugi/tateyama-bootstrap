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
#pragma once

#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>

namespace tateyama::server {

static const boost::filesystem::path LOCK_FILE_NAME = boost::filesystem::path("tsurugi.pid");

class proc_mutex {
  public:
    proc_mutex(boost::filesystem::path directory) : file_name_(directory / LOCK_FILE_NAME) {}
    ~proc_mutex() {
        close(fd_);
        unlink(file_name_.generic_string().c_str());
    }

    proc_mutex(proc_mutex const& other) = delete;
    proc_mutex& operator=(proc_mutex const& other) = delete;
    proc_mutex(proc_mutex&& other) noexcept = delete;
    proc_mutex& operator=(proc_mutex&& other) noexcept = delete;

    bool lock() {
        if ((fd_ = open(file_name_.generic_string().c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
            perror("open");
            exit(-1);
        }
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
    boost::filesystem::path file_name_;
    int fd_{};
};

}  // tateyama::server;
