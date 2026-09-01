cmake_minimum_required(VERSION 3.21)

project(sds_cn VERSION 2.0 LANGUAGES C)

option(SDS_BUILD_TESTS "构建并注册 SDS CTest 回归测试" ON)

function(sds_apply_c_settings target)
    set_target_properties(${target} PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(MSVC)
        # 源码注释和测试输出含 UTF-8 中文；同时保持较严格的警告级别。
        target_compile_options(${target} PRIVATE /W4 /utf-8)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endfunction()

# sds.c 是库的唯一实现文件，sds.h/sdsalloc.h 提供公开 API 和分配器配置。
add_library(sds STATIC sds.c)
set_target_properties(sds PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)
target_include_directories(sds PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
if(MSVC)
    target_compile_options(sds PRIVATE /W4 /utf-8)
    target_compile_definitions(sds PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(sds PRIVATE -Wall -Wextra -pedantic)
endif()

if(SDS_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        add_executable(sds_test sds.c)
        sds_apply_c_settings(sds_test)
        target_compile_definitions(sds_test PRIVATE SDS_TEST_MAIN)
        add_test(NAME sds_test COMMAND sds_test)
    endif()
endif()
