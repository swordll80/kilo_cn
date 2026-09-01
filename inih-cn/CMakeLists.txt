cmake_minimum_required(VERSION 3.21)

project(inih_cn VERSION 62 LANGUAGES C)

option(INIH_BUILD_SHARED "构建 inih/INIReader 共享库" OFF)
option(INIH_BUILD_INIREADER "构建 C++ INIReader 封装" ON)
option(INIH_BUILD_EXAMPLES "构建示例程序" ON)
option(INIH_BUILD_TESTS "构建并注册 CTest 回归测试" ON)

if(INIH_BUILD_INIREADER)
    enable_language(CXX)
endif()

# 这些选项与 ini.h 中的公开宏一一对应。配置值会写入 CMake 生成的配置头文件，
# 供库和示例统一使用；测试变体再通过额外编译定义覆盖相应选项。
option(INI_ALLOW_MULTILINE "允许多行值" ON)
option(INI_ALLOW_BOM "允许 UTF-8 BOM" ON)
option(INI_ALLOW_INLINE_COMMENTS "允许行内注释" ON)
option(INI_ALLOW_NO_VALUE "允许没有值的 name" OFF)
option(INI_STOP_ON_FIRST_ERROR "遇到第一个错误时停止" OFF)
option(INI_HANDLER_LINENO "向回调传递行号" OFF)
option(INI_CALL_HANDLER_ON_NEW_SECTION "遇到新 section 时调用回调" OFF)
option(INI_USE_STACK "使用栈上的行缓冲区" ON)
option(INI_ALLOW_REALLOC "允许堆缓冲区扩容" OFF)
option(INI_CUSTOM_ALLOCATOR "使用自定义内存分配器" OFF)
set(INI_MAX_LINE "200" CACHE STRING "INI 行缓冲区最大字节数")
set(INI_INITIAL_ALLOC "200" CACHE STRING "堆行缓冲区初始字节数")
set(INI_INLINE_COMMENT_PREFIXES ";" CACHE STRING "行内注释起始字符")
set(INI_START_COMMENT_PREFIXES ";#" CACHE STRING "行首注释起始字符")

set(INIH_TEST_DEFINITIONS)
if(NOT INI_ALLOW_MULTILINE)
    list(APPEND INIH_TEST_DEFINITIONS INI_ALLOW_MULTILINE=0)
endif()
if(NOT INI_ALLOW_BOM)
    list(APPEND INIH_TEST_DEFINITIONS INI_ALLOW_BOM=0)
endif()
if(NOT INI_ALLOW_INLINE_COMMENTS)
    list(APPEND INIH_TEST_DEFINITIONS INI_ALLOW_INLINE_COMMENTS=0)
endif()
if(INI_ALLOW_NO_VALUE)
    list(APPEND INIH_TEST_DEFINITIONS INI_ALLOW_NO_VALUE=1)
endif()
if(INI_STOP_ON_FIRST_ERROR)
    list(APPEND INIH_TEST_DEFINITIONS INI_STOP_ON_FIRST_ERROR=1)
endif()
if(INI_HANDLER_LINENO)
    list(APPEND INIH_TEST_DEFINITIONS INI_HANDLER_LINENO=1)
endif()
if(INI_CALL_HANDLER_ON_NEW_SECTION)
    list(APPEND INIH_TEST_DEFINITIONS INI_CALL_HANDLER_ON_NEW_SECTION=1)
endif()
if(NOT INI_USE_STACK)
    list(APPEND INIH_TEST_DEFINITIONS INI_USE_STACK=0)
endif()
if(INI_ALLOW_REALLOC)
    list(APPEND INIH_TEST_DEFINITIONS INI_ALLOW_REALLOC=1)
endif()
if(INI_CUSTOM_ALLOCATOR)
    list(APPEND INIH_TEST_DEFINITIONS INI_CUSTOM_ALLOCATOR=1)
endif()
if(NOT INI_MAX_LINE STREQUAL "200")
    list(APPEND INIH_TEST_DEFINITIONS INI_MAX_LINE=${INI_MAX_LINE})
endif()
if(NOT INI_INITIAL_ALLOC STREQUAL "200")
    list(APPEND INIH_TEST_DEFINITIONS INI_INITIAL_ALLOC=${INI_INITIAL_ALLOC})
endif()

foreach(_option IN ITEMS
        INI_ALLOW_MULTILINE
        INI_ALLOW_BOM
        INI_ALLOW_INLINE_COMMENTS
        INI_ALLOW_NO_VALUE
        INI_STOP_ON_FIRST_ERROR
        INI_HANDLER_LINENO
        INI_CALL_HANDLER_ON_NEW_SECTION
        INI_USE_STACK
        INI_ALLOW_REALLOC
        INI_CUSTOM_ALLOCATOR)
    if(${_option})
        set(INIH_CONFIG_${_option} 1)
    else()
        set(INIH_CONFIG_${_option} 0)
    endif()
endforeach()
set(INIH_CONFIG_MAX_LINE ${INI_MAX_LINE})
set(INIH_CONFIG_INITIAL_ALLOC ${INI_INITIAL_ALLOC})
set(INIH_CONFIG_INLINE_COMMENT_PREFIXES "${INI_INLINE_COMMENT_PREFIXES}")
set(INIH_CONFIG_START_COMMENT_PREFIXES "${INI_START_COMMENT_PREFIXES}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/inih_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/inih_config.h
    @ONLY
)

function(inih_apply_c_settings target)
    set_target_properties(${target} PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    if(MSVC)
        # 源文件和示例输出含中文；/utf-8 同时指定源文件和执行字符集。
        target_compile_options(${target} PRIVATE /W4 /utf-8)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endfunction()

function(inih_apply_cpp_settings target)
    set_target_properties(${target} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endfunction()

if(INIH_BUILD_SHARED)
    add_library(inih SHARED ini.c)
else()
    add_library(inih STATIC ini.c)
endif()
add_library(inih::inih ALIAS inih)
target_include_directories(inih PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_compile_definitions(inih PUBLIC INIH_USE_CMAKE_CONFIG)
if(INIH_BUILD_SHARED)
    target_compile_definitions(inih PUBLIC INI_SHARED_LIB)
    target_compile_definitions(inih PRIVATE INI_SHARED_LIB_BUILDING)
endif()
inih_apply_c_settings(inih)

if(INIH_BUILD_INIREADER)
    if(INIH_BUILD_SHARED)
        add_library(INIReader SHARED cpp/INIReader.cpp)
    else()
        add_library(INIReader STATIC cpp/INIReader.cpp)
    endif()
    add_library(inih::INIReader ALIAS INIReader)
    target_include_directories(INIReader PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cpp>
    )
    target_link_libraries(INIReader PUBLIC inih)
    if(INIH_BUILD_SHARED)
        target_compile_definitions(INIReader PRIVATE INI_SHARED_LIB_BUILDING)
    endif()
    inih_apply_cpp_settings(INIReader)
endif()

if(INIH_BUILD_EXAMPLES)
    add_executable(inih_example examples/ini_example.c)
    target_link_libraries(inih_example PRIVATE inih)
    inih_apply_c_settings(inih_example)

    add_executable(inih_ini_dump examples/ini_dump.c)
    target_link_libraries(inih_ini_dump PRIVATE inih)
    inih_apply_c_settings(inih_ini_dump)

    add_executable(inih_ini_xmacros examples/ini_xmacros.c)
    target_link_libraries(inih_ini_xmacros PRIVATE inih)
    inih_apply_c_settings(inih_ini_xmacros)

    if(INIH_BUILD_INIREADER)
        add_executable(inih_inireader_example examples/INIReaderExample.cpp)
        target_link_libraries(inih_inireader_example PRIVATE INIReader)
        inih_apply_cpp_settings(inih_inireader_example)

        add_executable(inih_inireader_errors examples/INIReaderExampleErrors.cpp)
        target_link_libraries(inih_inireader_errors PRIVATE INIReader)
        inih_apply_cpp_settings(inih_inireader_errors)
    endif()
endif()

if(INIH_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        function(inih_add_regression_test test_name source_file expected_file)
            add_executable(inih_test_${test_name} ini.c tests/${source_file})
            inih_apply_c_settings(inih_test_${test_name})
            target_include_directories(inih_test_${test_name} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}
            )
            target_compile_definitions(inih_test_${test_name} PRIVATE
                ${INIH_TEST_DEFINITIONS} ${ARGN}
            )
            add_test(NAME inih_test_${test_name}
                COMMAND ${CMAKE_COMMAND}
                    -DTEST_EXE=$<TARGET_FILE:inih_test_${test_name}>
                    -DEXPECTED_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/${expected_file}
                    -DWORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/tests
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_output.cmake
            )
        endfunction()

        inih_add_regression_test(multi unittest.c baseline_multi.txt)
        inih_add_regression_test(multi_max_line unittest.c baseline_multi_max_line.txt
            INI_MAX_LINE=20)
        inih_add_regression_test(single unittest.c baseline_single.txt
            INI_ALLOW_MULTILINE=0)
        inih_add_regression_test(disallow_inline_comments unittest.c baseline_disallow_inline_comments.txt
            INI_ALLOW_INLINE_COMMENTS=0)
        inih_add_regression_test(stop_on_first_error unittest.c baseline_stop_on_first_error.txt
            INI_STOP_ON_FIRST_ERROR=1)
        inih_add_regression_test(handler_lineno unittest.c baseline_handler_lineno.txt
            INI_HANDLER_LINENO=1)
        inih_add_regression_test(string unittest_string.c baseline_string.txt
            INI_MAX_LINE=20)
        inih_add_regression_test(heap unittest.c baseline_heap.txt
            INI_USE_STACK=0)
        inih_add_regression_test(heap_max_line unittest.c baseline_heap_max_line.txt
            INI_USE_STACK=0 INI_MAX_LINE=20 INI_INITIAL_ALLOC=20)
        inih_add_regression_test(heap_realloc unittest.c baseline_heap_realloc.txt
            INI_USE_STACK=0 INI_ALLOW_REALLOC=1 INI_INITIAL_ALLOC=5)
        inih_add_regression_test(heap_realloc_max_line unittest.c baseline_heap_realloc_max_line.txt
            INI_USE_STACK=0 INI_MAX_LINE=20 INI_ALLOW_REALLOC=1 INI_INITIAL_ALLOC=5)
        inih_add_regression_test(heap_string unittest_string.c baseline_heap_string.txt
            INI_USE_STACK=0 INI_MAX_LINE=20 INI_INITIAL_ALLOC=20)
        inih_add_regression_test(call_handler_on_new_section unittest.c baseline_call_handler_on_new_section.txt
            INI_CALL_HANDLER_ON_NEW_SECTION=1)
        inih_add_regression_test(allow_no_value unittest.c baseline_allow_no_value.txt
            INI_ALLOW_NO_VALUE=1)

        add_executable(inih_test_alloc ini.c tests/unittest_alloc.c)
        inih_apply_c_settings(inih_test_alloc)
        target_include_directories(inih_test_alloc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_compile_definitions(inih_test_alloc PRIVATE
            INI_CUSTOM_ALLOCATOR=1 INI_USE_STACK=0 INI_ALLOW_REALLOC=1
            INI_INITIAL_ALLOC=12
        )
        add_test(NAME inih_test_alloc
            COMMAND ${CMAKE_COMMAND}
                -DTEST_EXE=$<TARGET_FILE:inih_test_alloc>
                -DEXPECTED_FILE=${CMAKE_CURRENT_SOURCE_DIR}/tests/baseline_alloc.txt
                -DWORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/tests
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_output.cmake
        )

    endif()
endif()
