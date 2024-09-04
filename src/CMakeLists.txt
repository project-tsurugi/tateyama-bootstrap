#############
# server
#############
if(NOT TARGET ${jogasaki_engine})
        message(FATAL_ERROR "jogasaki implementation ${jogasaki_engine} not found")
endif()

file(GLOB SERVER_SOURCES
        "tateyama/server/*.cpp"
        "tateyama/configuration/*.cpp"
        )
if (ENABLE_ALTIMETER)
        set(SERVER_SOURCES ${SERVER_SOURCES} "tateyama/altimeter/altimeter_helper.cpp")
endif ()

add_executable(server
        ${SERVER_SOURCES}
        )

set_target_properties(server
        PROPERTIES
                INSTALL_RPATH "\$ORIGIN/../lib"
                RUNTIME_OUTPUT_NAME "tsurugidb"
        )

target_include_directories(server
        PRIVATE ${CMAKE_BINARY_DIR}/src
        PRIVATE .
        )

target_link_libraries(server
        PRIVATE api
        PRIVATE ${jogasaki_engine}
        PRIVATE ${tateyama_engine}
        PRIVATE limestone
        PRIVATE shirakami-shirakami
        PRIVATE takatori
        PRIVATE yugawara
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE pthread
        PRIVATE Boost::filesystem
        PRIVATE crypto
        PRIVATE rt
        )

if (OGAWAYAMA)
    target_link_libraries(server
            PRIVATE ${ogawayama_bridge}
    )
    target_compile_definitions(server PUBLIC OGAWAYAMA)
endif()

if (ENABLE_JEMALLOC)
    target_link_libraries(server
            PRIVATE jemalloc::jemalloc)
endif (ENABLE_JEMALLOC)

if (ENABLE_ALTIMETER)
    target_link_libraries(server
        PRIVATE altimeter
        PRIVATE Boost::container
        PRIVATE fmt::fmt
        PRIVATE Threads::Threads
    )
endif()

set_compile_options(server)

install_libexec(server ${export_name})


#############
# tgctl
#############
set(ProtoFiles
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/auth/request.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/auth/response.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/auth/common.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/core/request.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/core/response.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/datastore/request.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/datastore/response.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/datastore/common.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/endpoint/request.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/endpoint/response.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/metrics/request.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/metrics/response.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/framework/request.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/framework/response.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/kvs/response.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/kvs/transaction.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/kvs/data.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/diagnostics.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/session/request.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/session/response.proto
        ${CMAKE_CURRENT_SOURCE_DIR}/tateyama/proto/session/diagnostic.proto
        )

# By default, PROTOBUF_GENERATE_CPP generates file path for .pb.cc as if they are in the same directory.
# Work-around this with PROTOBUF_GENERATE_CPP_APPEND_PATH
set(PROTOBUF_GENERATE_CPP_APPEND_PATH OFF)
PROTOBUF_GENERATE_CPP(GENERATED_PROTO_SRCS GENERATED_PROTO_HDRS ${ProtoFiles})
add_custom_target(build_protos DEPENDS ${GENERATED_PROTO_SRCS})
set(GENERATED_PROTO_SRCS ${GENERATED_PROTO_SRCS} PARENT_SCOPE)

file(GLOB TGCTL_SOURCES
        "tateyama/tgctl/*.cpp"
        "tateyama/authentication/authentication.cpp"
        "tateyama/datastore/*.cpp"
        "tateyama/process/*.cpp"
        "tateyama/configuration/*.cpp"
        "tateyama/utils/*.cpp"
        "tateyama/session/*.cpp"
        "tateyama/metrics/*.cpp"
        )

set_source_files_properties(
        ${GENERATED_PROTO_SRCS}
        PROPERTIES
        GENERATED TRUE
        COMPILE_FLAGS "-Wno-unused-parameter -Wno-array-bounds"
        )

add_executable(tgctl
        ${TGCTL_SOURCES}
        ${GENERATED_PROTO_SRCS}
        )

add_dependencies(tgctl
        build_protos
        )

set_target_properties(tgctl
        PROPERTIES
                INSTALL_RPATH "\$ORIGIN/../lib"
                RUNTIME_OUTPUT_NAME "tgctl"
        )

target_include_directories(tgctl
        PRIVATE ${CMAKE_BINARY_DIR}/src
        PRIVATE .
        )

target_link_libraries(tgctl
        PRIVATE api
        PRIVATE ${tateyama_api}
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE pthread
        PRIVATE Boost::filesystem
        PRIVATE protobuf::libprotobuf
        PRIVATE crypto
        PRIVATE rt
        )

set_compile_options(tgctl)

install_custom(tgctl ${export_name})
