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
#pragma once

#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdexcept> // std::runtime_error
#include <filesystem>
#include <fstream>

namespace tateyama::process {

class file_mutex {
  public:
    enum class lock_state : std::int32_t {
        no_file = 0,
        not_locked,
        locked,
        error,
    };

    file_mutex(std::filesystem::path lock_file, bool create_file) : lock_file_(std::move(lock_file)), create_file_(create_file) {
        if (!create_file_ && !std::filesystem::exists(lock_file_)) {
            throw std::runtime_error("the lock file does not exist");
        }
    }
    ~file_mutex() {
        if (fd_ != not_opened) {
            close(fd_);
        }
        if (create_file_) {
            unlink(lock_file_.generic_string().c_str());
        }
    }

    file_mutex(file_mutex const& other) = delete;
    file_mutex& operator=(file_mutex const& other) = delete;
    file_mutex(file_mutex&& other) noexcept = delete;
    file_mutex& operator=(file_mutex&& other) noexcept = delete;

    void lock() {
        if (create_file_) {
            if ((fd_ = open(lock_file_.generic_string().c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {  // NOLINT
                throw std::runtime_error("open error");
            }
        }
        if (flock(fd_, LOCK_EX | LOCK_NB) == 0) {  // NOLINT
            if (ftruncate(fd_, 0) < 0) {
                throw std::runtime_error("ftruncate error");
            }
            std::string pid = std::to_string(getpid());
            if (write(fd_, pid.data(), pid.length()) < 0) {
                throw std::runtime_error("write error");
            }
            return;
        }
        throw std::runtime_error("lock error");
    }
    void unlock() const {
        flock(fd_, LOCK_UN);
    }
    [[nodiscard]] lock_state check() {
        std::error_code error;
        const bool result = std::filesystem::exists(lock_file_, error);
        if (!result || error) {
            return lock_state::no_file;
        }
        if (!std::filesystem::is_regular_file(lock_file_)) {
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

protected:
    std::filesystem::path lock_file_;  // NOLINT
    int fd_{};                         // NOLINT
    const bool create_file_;           // NOLINT

private:
    static constexpr int not_opened = -1;
};


class proc_mutex : public file_mutex {
public:
    explicit proc_mutex(std::filesystem::path lock_file, bool create_file = true) : file_mutex(std::move(lock_file), create_file) {
    }

    [[nodiscard]] inline std::string name() const {
        return lock_file_.generic_string();
    }
    [[nodiscard]] int pid(bool do_check = true) {
        std::string str;
        if (contents(str, do_check)) {
            try {
                return stoi(str);
            } catch (std::invalid_argument& e) {
                return 0;
            }
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
    [[nodiscard]] bool contents(std::string& str, bool do_check = true) {
        if (do_check && check() != lock_state::locked) {
            return false;
        }
        auto size = static_cast<std::size_t>(std::filesystem::file_size(lock_file_));
        str.resize(size, '\0');
        std::ifstream file(lock_file_);
        file.read(str.data(), static_cast<std::streamsize>(size));
        return true;
    }
};

}  // tateyama::process
