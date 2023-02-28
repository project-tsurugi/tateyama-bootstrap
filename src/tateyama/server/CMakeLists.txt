if(NOT TARGET ${jogasaki_engine})
    message(FATAL_ERROR "jogasaki implementation ${jogasaki_engine} not found")
endif()

file(GLOB SOURCES
        "*.cpp"
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
        PRIVATE ../include
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

set_compile_options(server)

install_libexec(server ${export_name})
