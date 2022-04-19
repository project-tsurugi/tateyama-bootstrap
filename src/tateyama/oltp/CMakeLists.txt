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
        )

target_link_libraries(oltp
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE pthread
        PRIVATE Boost::filesystem
        PRIVATE rt
        )

set_compile_options(oltp)

install_custom(oltp ${export_name})
