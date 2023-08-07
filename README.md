# tateyama-bootstrap - bootstrap executable for application infrastructure

## Requirements

* CMake `>= 3.16`
* C++ Compiler `>= C++17`
* access to installed dependent modules: 
  * tateyama
  * jogasaki
  * ogawayama (when -DOGAWAYAMA=ON is specified in cmake)
  * sharksfin
  * takatori
  * yugawara
  * mizugaki
  * shakujo (until dependency is removed)
  * mpdecimal (see [jogasaki](https://github.com/project-tsurugi/jogasaki#install-steps-for-mpdecimal) for installation instruction.
* and see *Dockerfile* section

```sh
# retrieve third party modules
git submodule update --init --recursive
```

### Dockerfile

```dockerfile
FROM ubuntu:22.04

RUN apt update -y && apt install -y git build-essential cmake ninja-build libboost-filesystem-dev libboost-system-dev libboost-container-dev libboost-thread-dev libboost-stacktrace-dev libgoogle-glog-dev libgflags-dev doxygen libtbb-dev libnuma-dev protobuf-compiler protobuf-c-compiler libprotobuf-dev libmsgpack-dev uuid-dev libicu-dev pkg-config flex bison libssl-dev
```

optional packages:

* `clang-tidy-14`

## How to build

```sh
mkdir -p build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

available options:
* `-DCMAKE_INSTALL_PREFIX=<installation directory>` - change install location
* `-DCMAKE_PREFIX_PATH=<installation directory>` - indicate prerequisite installation directory
* `-DCMAKE_IGNORE_PATH="/usr/local/include;/usr/local/lib/"` - specify the libraries search paths to ignore. This is convenient if the environment has conflicting version installed on system default search paths. (e.g. gflags in /usr/local)
* `-DFORCE_INSTALL_RPATH=ON` - automatically configure `INSTALL_RPATH` for non-default library paths
* `-DSHARKSFIN_IMPLEMENTATION=<implementation name>` - switch sharksfin implementation. Available options are `memory` and `shirakami` (default: `memory`)
* `-DOGAWAYAMA=ON` - enable ogawayama bridge
* `-DENABLE_JEMALLOC` - use jemalloc instead of default `malloc`
* for debugging only
  * `-DENABLE_SANITIZER=OFF` - disable sanitizers (requires `-DCMAKE_BUILD_TYPE=Debug`)
  * `-DENABLE_UB_SANITIZER=ON` - enable undefined behavior sanitizer (requires `-DENABLE_SANITIZER=ON`)
  * `-DENABLE_COVERAGE=ON` - enable code coverage analysis (requires `-DCMAKE_BUILD_TYPE=Debug`)
    
### install 

```sh
cmake --build . --target install
```

### Customize logging setting 
You can customize logging in the same way as sharksfin. See sharksfin [README.md](https://github.com/project-tsurugi/sharksfin/blob/master/README.md#customize-logging-setting) for more details.

```sh
GLOG_minloglevel=0 ./tateyama-server 
```

## License

[Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)

