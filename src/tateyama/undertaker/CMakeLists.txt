file(GLOB SOURCES
        "*.cpp"
        )

add_executable(undertaker
        ${SOURCES}
        )

set_target_properties(undertaker
        PROPERTIES
        RUNTIME_OUTPUT_NAME "undertaker"
        )

target_include_directories(undertaker
        PRIVATE .
        )

target_link_libraries(undertaker
        PRIVATE api
        )

set_compile_options(undertaker)

install_libexec(undertaker ${export_name})
