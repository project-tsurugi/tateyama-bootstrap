if(NOT TARGET ${jogasaki_engine})
    message(FATAL_ERROR "jogasaki implementation ${jogasaki_engine} not found")
endif()

file(GLOB SOURCES
        "tateyama/server/*.cpp"
        )

add_executable(server
        ${SOURCES}
        )

set_target_properties(server
        PROPERTIES
        RUNTIME_OUTPUT_NAME "tateyama-server"
        )

target_include_directories(server
        PRIVATE .
        )

target_link_libraries(server
        PRIVATE ${jogasaki_engine}
        PRIVATE ${tateyama_engine}
        PRIVATE takatori
        PRIVATE yugawara
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE pthread
        PRIVATE Boost::filesystem
        PRIVATE rt
        )

set_compile_options(server)

install_custom(server ${export_name})
