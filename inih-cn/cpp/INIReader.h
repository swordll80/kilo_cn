// 将 INI 文件读入便于访问的名称/值键值表。

// SPDX-License-Identifier: BSD-3-Clause

// Copyright (C) 2009-2025, Ben Hoyt

// inih 和 INIReader 按 New BSD 许可证发布（见 LICENSE.txt）。
// 更多信息请访问项目主页：
//
// https://github.com/benhoyt/inih

#ifndef INIREADER_H
#define INIREADER_H

#include <map>
#include <string>
#include <cstdint>
#include <vector>
#include <set>

// Windows DLL 所需的符号导出/导入定义。
#ifndef INI_API
#if defined _WIN32 || defined __CYGWIN__
#	ifdef INI_SHARED_LIB
#		ifdef INI_SHARED_LIB_BUILDING
#			define INI_API __declspec(dllexport)
#		else
#			define INI_API __declspec(dllimport)
#		endif
#	else
#		define INI_API
#	endif
#else
#	if defined(__GNUC__) && __GNUC__ >= 4
#		define INI_API __attribute__ ((visibility ("default")))
#	else
#		define INI_API
#	endif
#endif
#endif

// 将 INI 文件读入便于访问的名称/值键值表。（这里优先考虑实现简单，
// 而不是极限速度，但实际性能应当足够。）
class INIReader
{
public:
    // 构造 INIReader 并解析指定文件名。解析规则详见 ini.h。
    INI_API explicit INIReader(const std::string& filename);

    // 构造 INIReader 并解析指定缓冲区。解析规则详见 ini.h。
    INI_API explicit INIReader(const char *buffer, size_t buffer_size);

    // 返回 ini_parse() 的结果：成功为 0；解析错误为第一个错误的行号；
    // 文件打开失败为 -1；内存分配失败为 -2。
    INI_API int ParseError() const;

    // 返回描述错误类型的消息。没有错误时返回空字符串 ""。
    INI_API std::string ParseErrorMessage() const;

    // 获取字符串值；找不到时返回 default_value。
    INI_API std::string Get(const std::string& section, const std::string& name,
                    const std::string& default_value) const;

    // 获取字符串值；找不到、为空或只包含空白字符时返回 default_value。
    INI_API std::string GetString(const std::string& section, const std::string& name,
                    const std::string& default_value) const;

    // 获取 long 整数值；找不到或不是有效整数（十进制 "1234"、"-1234"
    // 或十六进制 "0x4d2"）时返回 default_value。
    INI_API long GetInteger(const std::string& section, const std::string& name, long default_value) const;

    // 获取 64 位整数（int64_t）值；找不到或不是有效整数（十进制 "1234"、
    // "-1234" 或十六进制 "0x4d2"）时返回 default_value。
    INI_API int64_t GetInteger64(const std::string& section, const std::string& name, int64_t default_value) const;

    // 获取无符号整数（unsigned long）值；找不到或不是有效的无符号整数
    // （十进制 "1234" 或十六进制 "0x4d2"）时返回 default_value。
    INI_API unsigned long GetUnsigned(const std::string& section, const std::string& name, unsigned long default_value) const;

    // 获取 64 位无符号整数（uint64_t）值；找不到或不是有效的无符号整数
    // （十进制 "1234" 或十六进制 "0x4d2"）时返回 default_value。
    INI_API uint64_t GetUnsigned64(const std::string& section, const std::string& name, uint64_t default_value) const;

    // 获取实数（double）值；找不到或不是符合 strtod() 规则的有效浮点数时
    // 返回 default_value。
    INI_API double GetReal(const std::string& section, const std::string& name, double default_value) const;

    // 获取布尔值；找不到或不是有效的 true/false 值时返回 default_value。
    // 有效真值为 "true"、"yes"、"on"、"1"，有效假值为 "false"、"no"、
    // "off"、"0"（不区分大小写）。
    INI_API bool GetBoolean(const std::string& section, const std::string& name, bool default_value) const;

    // 返回按字母顺序排列、包含全部 section 名称的新 vector。
    INI_API std::vector<std::string> Sections() const;

    // 返回指定 section 中按字母顺序排列的键名 vector。
    INI_API std::vector<std::string> Keys(const std::string& section) const;

    // 如果指定 section 存在则返回 true（section 至少要包含一个
    // name=value 键值对）。
    INI_API bool HasSection(const std::string& section) const;

    // 如果指定 section 和字段名对应的值存在则返回 true。
    INI_API bool HasValue(const std::string& section, const std::string& name) const;

protected:
    int _error;
    std::map<std::string, std::string> _values;
    static std::string MakeKey(const std::string& section, const std::string& name);
    static int ValueHandler(void* user, const char* section, const char* name,
                            const char* value);
};

#endif  // INIREADER_H
