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
#include <boost/filesystem/fstream.hpp>

namespace tateyama::server {

static const boost::filesystem::path LOCK_FILE_NAME = boost::filesystem::path("tsurugi.pid");

class proc_mutex {
  public:
    enum class lock_state : std::int32_t {
        no_file = 0,
        not_locked,
        locked,
        error,
    };
    
    proc_mutex(boost::filesystem::path directory, bool create_file = true)
        : file_name_(directory / LOCK_FILE_NAME), create_file_(create_file) {}
    ~proc_mutex() {
        close(fd_);
        if (create_file_) {
            unlink(file_name_.generic_string().c_str());
        }
    }

    proc_mutex(proc_mutex const& other) = delete;
    proc_mutex& operator=(proc_mutex const& other) = delete;
    proc_mutex(proc_mutex&& other) noexcept = delete;
    proc_mutex& operator=(proc_mutex&& other) noexcept = delete;

    [[nodiscard]] bool lock() {
        if (create_file_) {
            if ((fd_ = open(file_name_.generic_string().c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {
                perror("open");
                exit(-1);
            }
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
    [[nodiscard]] inline std::string name() const {
        return file_name_.generic_string();
    }
    [[nodiscard]] bool contents(std::string& str) {
        if (check() != lock_state::locked) {
            return false;
        }
        std::size_t sz = static_cast<std::size_t>(boost::filesystem::file_size(file_name_));
        str.resize(sz, '\0');
        boost::filesystem::ifstream file(file_name_);
        file.read(&str[0], sz);
        return true;
    }
    [[nodiscard]] lock_state check() {
        boost::system::error_code error;
        const bool result = boost::filesystem::exists(file_name_, error);
        if (!result || error) {
            return lock_state::no_file;
        }
        if (!boost::filesystem::is_regular_file(file_name_)) {
            return lock_state::error;            
        }
        if (fd_ = open(file_name_.generic_string().c_str(), O_WRONLY); fd_ < 0) {
            return lock_state::error;
        }
        if (flock(fd_, LOCK_EX | LOCK_NB) == 0) {
            unlock();
            return lock_state::not_locked;
        }
        return lock_state::locked;
    }
    [[nodiscard]] inline constexpr std::string_view to_string_view(lock_state value) noexcept {
        using namespace std::string_view_literals;
        using state = lock_state;
        switch (value) {
        case state::no_file: return "no_file"sv;
        case state::not_locked: return "not_locked"sv;
        case state::locked: return "locked"sv;
        case state::error: return "error"sv;
        }
        std::abort();
    }
    
private:
    boost::filesystem::path file_name_;
    int fd_{};
    bool create_file_;
};

}  // tateyama::server;
