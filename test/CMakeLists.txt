set(test_target tateyama-bootstrap-test)

add_executable(${test_target}
        main.cpp
)

target_compile_options(${test_target}
        PRIVATE -DDIRECTORY=\"${CMAKE_CURRENT_SOURCE_DIR}\"
)

target_include_directories(${test_target}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/tateyama
)

target_link_libraries(${test_target}
        PUBLIC gtest
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        )

function (add_test_executable source_file)
    get_filename_component(test_name "${source_file}" NAME_WE)
    target_sources(${test_target}
            PRIVATE ${source_file}
    )
    add_test(
            NAME ${test_name}
            COMMAND ${test_target} --gtest_filter=${test_name}.* --gtest_output=xml:${test_name}_gtest_result.xml
    )
endfunction (add_test_executable)

file(GLOB SRCS
        "tateyama/monitor/*.cpp"
)

foreach(file ${SRCS})
    add_test_executable(${file})
endforeach()
  

# add_executable(server_test server_test.cpp) 

# add_test(
#         NAME restore_backup
#         COMMAND $<TARGET_FILE:server_test> "restore_backup: name = /a/b/c, keep = true" src/tateyama/server/tateyama-server --restore_backup=/a/b/c -keep_backup
# 	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
# )

# add_test(
#         NAME restore_tag
#         COMMAND $<TARGET_FILE:server_test> "restore_tag: tag = 12345678" src/tateyama/server/tateyama-server --restore_tag=12345678
# 	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
# )
