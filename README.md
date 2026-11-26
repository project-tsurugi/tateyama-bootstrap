# tateyama-bootstrap - bootstrap executable for application infrastructure

## Requirements

* CMake `>= 3.16`
* C++ Compiler `>= C++17`
* and see *Dockerfile* section
* and see *Dockerfile* section

```sh
# retrieve third party modules
git submodule update --init --recursive
```

### Dockerfile

```dockerfile
FROM ubuntu:22.04

RUN apt update -y && apt install -y git build-essential cmake ninja-build libboost-filesystem-dev libboost-system-dev libboost-container-dev libboost-thread-dev libboost-stacktrace-dev libboost-regex-dev libgoogle-glog-dev libgflags-dev doxygen libtbb-dev libnuma-dev protobuf-compiler protobuf-c-compiler libprotobuf-dev libmsgpack-dev uuid-dev libicu-dev pkg-config flex bison libssl-dev libjwt-dev
```

optional packages:

* `clang-tidy-14`

### Install modules

#### tsurugidb modules

This requires below [tsurugidb](https://github.com/project-tsurugi/tsurugidb) modules to be installed.

* [tateyama](https://github.com/project-tsurugi/tateyama)
* [jogasaki](https://github.com/project-tsurugi/jogasaki)
* [limestone](https://github.com/project-tsurugi/limestone)
* [takatori](https://github.com/project-tsurugi/takatori)
* [yugawara](https://github.com/project-tsurugi/yugawara)
* [sharksfin](https://github.com/project-tsurugi/sharksfin)

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
* `-DSHARKSFIN_IMPLEMENTATION=<implementation name>` - switch sharksfin implementation. Available options are `memory` and `shirakami` (default: `shirakami`)
* `-DENABLE_JEMALLOC` - use jemalloc instead of default `malloc`
* `-DBUILD_STRICT=OFF` - don't treat compile warnings as build errors
* for debugging only
  * `-DENABLE_SANITIZER=OFF` - disable sanitizers (requires `-DCMAKE_BUILD_TYPE=Debug`)
  * `-DENABLE_UB_SANITIZER=ON` - enable undefined behavior sanitizer (requires `-DENABLE_SANITIZER=ON`)
  * `-DENABLE_COVERAGE=ON` - enable code coverage analysis (requires `-DCMAKE_BUILD_TYPE=Debug`)

### install

```sh
cmake --build . --target install
```

### run tests

Execute the test as below:
```sh
ctest -V
```

### Customize logging setting
You can customize logging in the same way as sharksfin. See sharksfin [README.md](https://github.com/project-tsurugi/sharksfin/blob/master/README.md#customize-logging-setting) for more details.

```sh
GLOG_minloglevel=0 tgctl start
```

## Distributed query configuration

Coordinator and worker roles are independent and disabled by default. Bootstrap
provides their default settings; Jogasaki configures outgoing connections and
Tateyama accepts the worker RPCs.

Enable the coordinator and specify its worker connections:

```ini
[distributed_query_coordinator]
enabled=true

[distributed_query_client]
name=worker_a
uri=dns:///worker-a.example.com:52345
```

`name` and `uri` are paired by position in `|`-separated lists. When `secure`
is omitted, every connection uses plaintext. When `root_cert` is omitted, each
TLS connection uses `grpc_default` roots. If either list is explicitly supplied,
it must have the same number of elements as `name` and `uri`.

For TLS, configure the coordinator to trust the worker certificate:

```ini
[distributed_query_client]
name=worker_a
uri=dns:///worker-a.example.com:52345
secure=true
root_cert=/path/to/ca.crt
```

Enable the worker and its gRPC listener:

```ini
[distributed_query_remote]
enabled=true

[grpc_server]
enabled=true
listen_address=0.0.0.0:52345
secure=true
fullchain_crt=/path/to/worker.crt
server_key=/path/to/worker.key
```

A plaintext worker uses `secure=false` in `[grpc_server]`. Both roles may be
enabled on the same node. A coordinator needs at least one worker connection;
a disabled coordinator ignores the client settings. The worker role requires
the gRPC server. Apply role changes by restarting the server.


## License

[Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)

