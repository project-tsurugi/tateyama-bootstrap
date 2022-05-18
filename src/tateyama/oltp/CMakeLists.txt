file(GLOB SOURCES
        "*.cpp"
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
        PRIVATE ../include
        )

target_link_libraries(oltp
        PRIVATE api
        PRIVATE ${tateyama_engine}
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE pthread
        PRIVATE Boost::filesystem
        PRIVATE protobuf::libprotobuf
        PRIVATE rt
        )

set_compile_options(oltp)

install_custom(oltp ${export_name})
