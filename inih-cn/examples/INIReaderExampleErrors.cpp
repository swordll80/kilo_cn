// 示例：展示 ParseErrorMessage() 方法。

#include <iostream>
#include "../cpp/INIReader.h"

int main()
{
    INIReader reader_file_not_found("/file_that_does_not_exist.ini");
    INIReader reader_parse_error("../tests/name_only_after_error.ini");
    INIReader reader_success("../tests/normal.ini");

    std::cout
        << "文件未找到错误消息：\"" << reader_file_not_found.ParseErrorMessage() << "\"\n"
        << "解析错误消息：\"" << reader_parse_error.ParseErrorMessage() << "\"\n"
        << "成功结果消息：\"" << reader_success.ParseErrorMessage() << "\"\n";

    return 0;
}
