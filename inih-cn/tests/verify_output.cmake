if(NOT DEFINED TEST_EXE OR NOT DEFINED EXPECTED_FILE OR NOT DEFINED WORKING_DIRECTORY)
    message(FATAL_ERROR "必须提供 TEST_EXE、EXPECTED_FILE 和 WORKING_DIRECTORY")
endif()

execute_process(
    COMMAND "${TEST_EXE}"
    WORKING_DIRECTORY "${WORKING_DIRECTORY}"
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE actual_output
    ERROR_VARIABLE error_output
)
if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "测试程序退出码为 ${test_result}\n${error_output}")
endif()

file(READ "${EXPECTED_FILE}" expected_output)
string(REPLACE "\r\n" "\n" actual_normalized "${actual_output}")
string(REPLACE "\r\n" "\n" expected_normalized "${expected_output}")
if(NOT actual_normalized STREQUAL expected_normalized)
    message(FATAL_ERROR
        "测试输出与基线不一致：${EXPECTED_FILE}\n"
        "实际输出：\n${actual_normalized}\n"
        "期望输出：\n${expected_normalized}")
endif()
