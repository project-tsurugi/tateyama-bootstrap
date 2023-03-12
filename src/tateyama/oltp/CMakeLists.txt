file(GLOB SOURCES
        "main.cpp"
        "../authentication/authentication.cpp"
        "../datastore/backup.cpp"
        "../process/control.cpp"
        )

add_executable(oltp
        ${SOURCES}
        )

set_target_properties(oltp
        PROPERTIES
        RUNTIME_OUTPUT_NAME "oltp"
        )

target_include_directories(oltp
        PRIVATE .
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        )

target_link_libraries(oltp
        PRIVATE api
        PRIVATE ${tateyama_engine}
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE pthread
        PRIVATE Boost::filesystem
        PRIVATE protobuf::libprotobuf
        PRIVATE crypto
        PRIVATE rt
        )

set_compile_options(oltp)

install_custom(oltp ${export_name})
