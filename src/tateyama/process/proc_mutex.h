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
#include <stdexcept> // std::runtime_error

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

namespace tateyama::process {

class proc_mutex {
  public:
    enum class lock_state : std::int32_t {
        no_file = 0,
        not_locked,
        locked,
        error,
    };
    
    explicit proc_mutex(boost::filesystem::path lock_file, bool create_file = true, bool throw_exception = true)
        : create_file_(create_file) {
        lock_file_ = std::move(lock_file);
        if (!create_file_ && throw_exception && !boost::filesystem::exists(lock_file_)) {
            throw std::runtime_error("the lock file does not exist");
        }
    }
    ~proc_mutex() {
        close(fd_);
        if (create_file_) {
            unlink(lock_file_.generic_string().c_str());
        }
    }

    proc_mutex(proc_mutex const& other) = delete;
    proc_mutex& operator=(proc_mutex const& other) = delete;
    proc_mutex(proc_mutex&& other) noexcept = delete;
    proc_mutex& operator=(proc_mutex&& other) noexcept = delete;

    [[nodiscard]] bool lock() {
        if (create_file_) {
            if ((fd_ = open(lock_file_.generic_string().c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {  // NOLINT
                perror("open");
                exit(-1);
            }
        }
        if (flock(fd_, LOCK_EX | LOCK_NB) == 0) {  // NOLINT
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
    void unlock() const {
        flock(fd_, LOCK_UN);
    }
    [[nodiscard]] inline std::string name() const {
        return lock_file_.generic_string();
    }
    [[nodiscard]] lock_state check() {
        boost::system::error_code error;
        const bool result = boost::filesystem::exists(lock_file_, error);
        if (!result || error) {
            return lock_state::no_file;
        }
        if (!boost::filesystem::is_regular_file(lock_file_)) {
            return lock_state::error;            
        }
        if (fd_ = open(lock_file_.generic_string().c_str(), O_WRONLY); fd_ < 0) {  // NOLINT
            return lock_state::error;
        }
        if (flock(fd_, LOCK_EX | LOCK_NB) == 0) {  // NOLINT
            unlock();
            return lock_state::not_locked;
        }
        return lock_state::locked;
    }
    [[nodiscard]] int pid(bool do_check = true) {
        std::string s;
        if (contents(s, do_check)) {
            return stoi(s);
        }
        return 0;
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
    boost::filesystem::path lock_file_;
    int fd_{};
    const bool create_file_;

    [[nodiscard]] bool contents(std::string& str, bool do_check = true) {
        if (do_check && check() != lock_state::locked) {
            return false;
        }
        auto sz = static_cast<std::size_t>(boost::filesystem::file_size(lock_file_));
        str.resize(sz, '\0');
        boost::filesystem::ifstream file(lock_file_);
        file.read(&str[0], sz);
        return true;
    }
};

}  // tateyama::process
