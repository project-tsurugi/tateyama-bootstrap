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
#include <functional>

namespace tateyama::process {

class file_mutex {
  public:
    enum class lock_state : std::int32_t {
        no_file = 0,
        not_locked,
        locked,
        error,
    };

    file_mutex(std::filesystem::path lock_file, bool create_file, bool throw_exception) : lock_file_(std::move(lock_file)) {
        if (create_file) {
            if ((fd_ = open(lock_file_.generic_string().c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) < 0) {  // NOLINT
                if (throw_exception) {
                    throw std::runtime_error("the lock file already exist");
                }
            }
        } else {
            if ((fd_ = open(lock_file_.generic_string().c_str(), O_RDWR)) < 0) {  // NOLINT
                if (throw_exception) {
                    throw std::runtime_error("the lock file does not exist");
                }
            }
        }
    }
    ~file_mutex() {
        if (fd_ >= 0) {
            close(fd_);
        }
        if (owner_) {
            unlink(lock_file_.generic_string().c_str());
        }
    }

    file_mutex(file_mutex const& other) = delete;
    file_mutex& operator=(file_mutex const& other) = delete;
    file_mutex(file_mutex&& other) noexcept = delete;
    file_mutex& operator=(file_mutex&& other) noexcept = delete;

    [[nodiscard]] inline std::string name() const {
        return lock_file_.generic_string();
    }
    void lock(const std::function<void(void)>& fill_contents = []{}) {
        if (flock(fd_, LOCK_EX | LOCK_NB) == 0) {  // NOLINT
            fill_contents();
            owner_ = true;
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
        if (fd_ == not_opened) {
            if (fd_ = open(lock_file_.generic_string().c_str(), O_RDWR); fd_ < 0) {  // NOLINT
                return lock_state::error;
            }
        }
        if (flock(fd_, LOCK_EX | LOCK_NB) == 0) {  // NOLINT
            unlock();
            return lock_state::not_locked;
        }
        return lock_state::locked;
    }
    [[nodiscard]] inline constexpr std::string_view to_string_view(lock_state value) noexcept {
        using namespace std::string_view_literals;
        switch (value) {
        case lock_state::no_file: return "no_file"sv;
        case lock_state::not_locked: return "not_locked"sv;
        case lock_state::locked: return "locked"sv;
        case lock_state::error: return "error"sv;
        }
        return "illegal lock state"sv;
    }

protected:
    std::filesystem::path lock_file_;  // NOLINT
    int fd_{not_opened};               // NOLINT

private:
    bool owner_{};
    static constexpr int not_opened = -1;
};


class proc_mutex : public file_mutex {
public:
    explicit proc_mutex(std::filesystem::path lock_file, bool create_file = true, bool throw_exception = true)
        : file_mutex(std::move(lock_file), create_file, throw_exception) {
    }

    void lock() {
        file_mutex::lock([this]{
            if (ftruncate(fd_, 0) < 0) {
                throw std::runtime_error("ftruncate error");
            }
            std::string pid = std::to_string(getpid());
            if (write(fd_, pid.data(), pid.length()) < 0) {
                throw std::runtime_error("write error");
            }
        });
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

class shm_mutex {
public:
    explicit shm_mutex(std::filesystem::path lock_file) {
        try {
            shm_mutex_ = std::make_unique<file_mutex>(std::move(lock_file), true, true);
            shm_mutex_->lock();
            return;
        } catch (std::runtime_error &ex) {
            shm_mutex_ = std::make_unique<file_mutex>(std::move(lock_file), false, true);
            shm_mutex_->lock();
            return;
        }
    }
    static std::filesystem::path lock_file_name(std::string_view dbname) {
        std::string str = "tsurugi-";
        str += dbname;
        str += ".lock";
        return {str};
    }

private:
    std::unique_ptr<file_mutex> shm_mutex_{};
};

}  // tateyama::process
