cmake_minimum_required(VERSION 3.21)

project(kilo VERSION 0.0.1 LANGUAGES C)

add_executable(kilo kilo.c)

set_target_properties(kilo PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

if(MSVC)
    # 源文件包含中文注释和界面提示，明确要求 MSVC 使用 UTF-8 读写源文件。
    target_compile_options(kilo PRIVATE /W4 /utf-8)
    target_compile_definitions(kilo PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(kilo PRIVATE -Wall -Wextra -pedantic)
endif()

option(KILO_BUILD_JSMN "构建 jsmn 示例和单元测试" ON)

if(KILO_BUILD_JSMN)
    # jsmn 是单头文件、无运行时依赖的 JSON 标记解析器。
    # 通过 INTERFACE 目标公开头文件目录，示例和测试均复用同一配置。
    add_library(jsmn INTERFACE)
    target_include_directories(jsmn INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/jsmn-cn
    )

    function(configure_jsmn_target target)
        set_target_properties(${target} PROPERTIES
            C_STANDARD 99
            C_STANDARD_REQUIRED ON
            C_EXTENSIONS OFF
        )
        target_link_libraries(${target} PRIVATE jsmn)
        if(MSVC)
            # jsmn-cn 的注释和测试输出使用 UTF-8，避免 MSVC 按本地代码页解析。
            target_compile_options(${target} PRIVATE /W4 /utf-8)
            target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
        endif()
    endfunction()

    add_executable(jsmn_simple_example jsmn-cn/example/simple.c)
    configure_jsmn_target(jsmn_simple_example)

    add_executable(jsmn_jsondump_example jsmn-cn/example/jsondump.c)
    configure_jsmn_target(jsmn_jsondump_example)

    # 与 jsmn 原 Makefile 保持一致：分别覆盖默认、严格、父链接和组合模式。
    include(CTest)
    if(BUILD_TESTING)
        set(JSMN_TEST_VARIANTS
            default
            strict
            parent_links
            strict_parent_links
        )
        foreach(variant IN LISTS JSMN_TEST_VARIANTS)
            set(target "jsmn_test_${variant}")
            add_executable(${target} jsmn-cn/test/tests.c)
            configure_jsmn_target(${target})

            if(variant STREQUAL "strict" OR variant STREQUAL "strict_parent_links")
                target_compile_definitions(${target} PRIVATE JSMN_STRICT=1)
            endif()
            if(variant STREQUAL "parent_links" OR variant STREQUAL "strict_parent_links")
                target_compile_definitions(${target} PRIVATE JSMN_PARENT_LINKS=1)
            endif()

            add_test(NAME ${target} COMMAND ${target})
        endforeach()
    endif()
endif()

include(GNUInstallDirs)
install(TARGETS kilo RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(KILO_BUILD_JSMN)
    install(FILES jsmn-cn/jsmn.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
