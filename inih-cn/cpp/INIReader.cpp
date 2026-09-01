// 将 INI 文件读入便于访问的名称/值键值表。

// SPDX-License-Identifier: BSD-3-Clause

// Copyright (C) 2009-2025, Ben Hoyt

// inih 和 INIReader 按 New BSD 许可证发布（见 LICENSE.txt）。
// 更多信息请访问项目主页：
//
// https://github.com/benhoyt/inih

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include "../ini.h"
#include "INIReader.h"

using std::string;

INIReader::INIReader(const string& filename)
{
    _error = ini_parse(filename.c_str(), ValueHandler, this);
}

INIReader::INIReader(const char *buffer, size_t buffer_size)
{
  _error = ini_parse_string_length(buffer, buffer_size, ValueHandler, this);
}

int INIReader::ParseError() const
{
    return _error;
}

string INIReader::ParseErrorMessage() const
{
    // _error 为正数时表示发生解析错误的行号。原因可能是行过长、
    // ValueHandler 指示的用户自定义错误、未闭合的 section 名称，或没有值
    // 的 name。
    if (_error > 0) {
        return "第 " + std::to_string(_error) + " 行解析错误；是否缺少 ']' 或 '='？";
    }

    // _error 为负数时表示系统类错误，0 表示成功。
    switch (_error) {
    case -2:
        return "内存分配失败";

    case -1:
        return "文件打开失败";

    case 0:
        return "";
    }

    // 正常情况下不会到达这里。若 C API 增加了错误码而此处未同步更新，
    // 可能会执行到这里。
    return "未知错误 " + std::to_string(_error);
}

string INIReader::Get(const string& section, const string& name, const string& default_value) const
{
    string key = MakeKey(section, name);
    // 使用 _values.find() 而不是 _values.at()，以兼容 C++11 之前的编译器。
    return _values.count(key) ? _values.find(key)->second : default_value;
}

string INIReader::GetString(const string& section, const string& name, const string& default_value) const
{
    const string str = Get(section, name, "");
    return str.empty() ? default_value : str;
}

long INIReader::GetInteger(const string& section, const string& name, long default_value) const
{
    string valstr = Get(section, name, "");
    const char* value = valstr.c_str();
    char* end;
    // 同时支持解析十进制 "1234" 和十六进制 "0x4D2"。
    long n = strtol(value, &end, 0);
    return end > value ? n : default_value;
}

INI_API int64_t INIReader::GetInteger64(const string& section, const string& name, int64_t default_value) const
{
    string valstr = Get(section, name, "");
    const char* value = valstr.c_str();
    char* end;
    // 同时支持解析十进制 "1234" 和十六进制 "0x4D2"。
    int64_t n = strtoll(value, &end, 0);
    return end > value ? n : default_value;
}

unsigned long INIReader::GetUnsigned(const string& section, const string& name, unsigned long default_value) const
{
    string valstr = Get(section, name, "");
    const char* value = valstr.c_str();
    char* end;
    // 同时支持解析十进制 "1234" 和十六进制 "0x4D2"。
    unsigned long n = strtoul(value, &end, 0);
    return end > value ? n : default_value;
}

INI_API uint64_t INIReader::GetUnsigned64(const string& section, const string& name, uint64_t default_value) const
{
    string valstr = Get(section, name, "");
    const char* value = valstr.c_str();
    char* end;
    // 同时支持解析十进制 "1234" 和十六进制 "0x4D2"。
    uint64_t n = strtoull(value, &end, 0);
    return end > value ? n : default_value;
}

double INIReader::GetReal(const string& section, const string& name, double default_value) const
{
    string valstr = Get(section, name, "");
    const char* value = valstr.c_str();
    char* end;
    double n = strtod(value, &end);
    return end > value ? n : default_value;
}

bool INIReader::GetBoolean(const string& section, const string& name, bool default_value) const
{
    string valstr = Get(section, name, "");
    // 转换为小写，使字符串比较不区分大小写。
    std::transform(valstr.begin(), valstr.end(), valstr.begin(),
        [](const unsigned char& ch) { return static_cast<unsigned char>(::tolower(ch)); });
    if (valstr == "true" || valstr == "yes" || valstr == "on" || valstr == "1")
        return true;
    else if (valstr == "false" || valstr == "no" || valstr == "off" || valstr == "0")
        return false;
    else
        return default_value;
}

std::vector<string> INIReader::Sections() const
{
    std::set<string> sectionSet;
    for (std::map<string, string>::const_iterator it = _values.begin(); it != _values.end(); ++it) {
        size_t pos = it->first.find('=');
        if (pos != string::npos) {
            sectionSet.insert(it->first.substr(0, pos));
        }
    }
    return std::vector<string>(sectionSet.begin(), sectionSet.end());
}

std::vector<string> INIReader::Keys(const string& section) const
{
    std::vector<string> keys;
    string keyPrefix = MakeKey(section, "");
    for (std::map<string, string>::const_iterator it = _values.begin(); it != _values.end(); ++it) {
        if (it->first.compare(0, keyPrefix.length(), keyPrefix) == 0) {
            keys.push_back(it->first.substr(keyPrefix.length()));
        }
    }
    return keys;
}

bool INIReader::HasSection(const string& section) const
{
    const string key = MakeKey(section, "");
    std::map<string, string>::const_iterator pos = _values.lower_bound(key);
    if (pos == _values.end())
        return false;
    // lower_bound 返回位置上的键是否以该 section 前缀开头？
    return pos->first.compare(0, key.length(), key) == 0;
}

bool INIReader::HasValue(const string& section, const string& name) const
{
    string key = MakeKey(section, name);
    return _values.count(key);
}

string INIReader::MakeKey(const string& section, const string& name)
{
    string key = section + "=" + name;
    // 转换为小写，使 section/name 查找不区分大小写。
    std::transform(key.begin(), key.end(), key.begin(),
        [](const unsigned char& ch) { return static_cast<unsigned char>(::tolower(ch)); });
    return key;
}

int INIReader::ValueHandler(void* user, const char* section, const char* name,
                            const char* value)
{
    if (!name)  // 启用 INI_CALL_HANDLER_ON_NEW_SECTION 时会出现这种情况。
        return 1;
    INIReader* reader = static_cast<INIReader*>(user);
    string key = MakeKey(section, name);
    if (reader->_values[key].size() > 0)
        reader->_values[key] += "\n";
    reader->_values[key] += value ? value : "";
    return 1;
}
