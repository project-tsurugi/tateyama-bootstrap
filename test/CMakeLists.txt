set(test_target tateyama-bootstrap-test)

add_executable(${test_target}
        main.cpp
)

target_compile_options(${test_target}
        PRIVATE -DCMAKE_CURRENT_SOURCE_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\"
)

target_include_directories(${test_target}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(${test_target}
        PUBLIC gtest
        PRIVATE lib_for_tests-impl
        PRIVATE glog::glog
        PRIVATE gflags::gflags
        PRIVATE rt
        PRIVATE Boost::thread
        PRIVATE Boost::filesystem
        PRIVATE ${tateyama_engine}
        )

function (add_test_executable source_file)
    get_filename_component(test_name "${source_file}" NAME_WE)
    target_sources(${test_target}
            PRIVATE ${source_file}
    )
    add_dependencies(${test_target}
            build_protos
    )
    add_test(
            NAME ${test_name}
            COMMAND ${test_target} --gtest_filter=${test_name}.* --gtest_output=xml:${test_name}_gtest_result.xml
    )
endfunction (add_test_executable)

file(GLOB SRCS
        "tateyama/monitor/*_test.cpp"
        "tateyama/include/*_test.cpp"
        "tateyama/tgctl/*_test.cpp"
        "tateyama/configuration/*_test.cpp"
        "tateyama/version/*_test.cpp"
        "tateyama/session/*_test.cpp"
        "tateyama/metrics/*_test.cpp"
        "tateyama/request/*_test.cpp"
        "tateyama/transport/*_test.cpp"
)
if (ENABLE_ALTIMETER)
    file(GLOB ALTIMETER_SRCS
        "tateyama/altimeter/*_test.cpp"
    )
    list(APPEND SRCS ${ALTIMETER_SRCS})
endif ()

foreach(file ${SRCS})
    add_test_executable(${file})
endforeach()
