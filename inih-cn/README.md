# inih-cn（中文注释版）

`inih` 是一个简洁的 `.INI` 文件解析器，使用 C 编写，代码量小、无第三方
运行时依赖，适合嵌入式程序和普通 Windows/Linux 工具。本目录保留原始 C
API、宏、解析规则和测试输入，仅将注释、示例提示和 C++ 错误消息中文化，并
补充可直接用于 Windows 的 CMake/CTest 配置。

项目原作者、许可证和上游地址保留在源文件及 [LICENSE.txt](LICENSE.txt) 中：

- 上游项目：[benhoyt/inih](https://github.com/benhoyt/inih)
- 许可证：BSD-3-Clause

## 快速使用

最小 C 示例：

```c
#include "ini.h"

static int handler(void* user, const char* section,
                   const char* name, const char* value)
{
    /* section/name/value 仅在本次回调期间有效。 */
    (void)user;
    return section && name && value;
}

int result = ini_parse("settings.ini", handler, NULL);
```

`ini_parse()` 会为每个 `name=value` 键值对调用一次回调。也可以使用：

- `ini_parse_file()`：解析调用方提供的 `FILE*`，不会自动关闭文件；
- `ini_parse_string()`：解析以 `\0` 结尾的内存字符串；
- `ini_parse_string_length()`：解析指定长度的内存数据，不要求调用 `strlen()`；
- `ini_parse_stream()`：使用自定义的类 `fgets` 读取函数。

解析结果为 `0` 表示成功；正数表示第一个解析错误的行号；`-1` 表示文件打开
失败；`-2` 表示堆内存分配失败（仅使用堆缓冲区时可能发生）。

## C++ 封装

`cpp/INIReader.h` 和 `cpp/INIReader.cpp` 提供一个保留键值的简单 C++ 封装：

```cpp
#include "INIReader.h"

INIReader reader("settings.ini");
if (reader.ParseError() < 0) {
    // reader.ParseErrorMessage() 返回中文错误说明。
}
std::string name = reader.Get("user", "name", "未命名");
long count = reader.GetInteger("user", "count", 0);
```

封装支持字符串、`long`、64 位整数、无符号整数、`double`、布尔值，以及
`Sections()`、`Keys()`、`HasSection()` 和 `HasValue()`。查找键名和 section 名
不区分 ASCII 大小写；原始字符串内容不会被转换编码。

## 解析规则

- 支持 `[section]`、`name=value` 和兼容 Python `configparser` 的 `name:value`；
- 默认支持以 `;` 或 `#` 开头的行首注释；
- 默认支持值的续行，续行必须以空白字符开头；
- 默认允许 UTF-8 BOM（`EF BB BF`）；
- 默认允许以空白字符分隔的 `;` 行内注释；
- section 之前出现的键值对，其 section 为空字符串；
- 默认遇到错误后继续解析，并返回第一个错误行号；
- 回调接收的字符串只保证在回调期间有效，调用方若需保存必须自行复制。

## 编译选项

以下宏保持原始 API 名称，通常通过编译器的 `-D`/`/D` 传入：

| 宏 | 默认值 | 作用 |
|---|---:|---|
| `INI_ALLOW_MULTILINE` | `1` | 允许多行值 |
| `INI_ALLOW_BOM` | `1` | 允许 UTF-8 BOM |
| `INI_ALLOW_INLINE_COMMENTS` | `1` | 允许行内注释 |
| `INI_INLINE_COMMENT_PREFIXES` | `;` | 指定行内注释字符 |
| `INI_START_COMMENT_PREFIXES` | `;#` | 指定行首注释字符 |
| `INI_ALLOW_NO_VALUE` | `0` | 允许没有值的 name，并以 `NULL` 值回调 |
| `INI_STOP_ON_FIRST_ERROR` | `0` | 在第一个错误处停止 |
| `INI_HANDLER_LINENO` | `0` | 向 handler 增加行号参数 |
| `INI_CALL_HANDLER_ON_NEW_SECTION` | `0` | 新 section 开始时回调一次 |
| `INI_USE_STACK` | `1` | 使用栈缓冲区；设为 `0` 使用堆 |
| `INI_MAX_LINE` | `200` | 行缓冲区最大字节数 |
| `INI_INITIAL_ALLOC` | `200` | 堆缓冲区初始字节数 |
| `INI_ALLOW_REALLOC` | `0` | 允许堆缓冲区扩容 |
| `INI_CUSTOM_ALLOCATOR` | `0` | 使用自定义 `ini_malloc/free/realloc` |

`INI_MAX_LINE` 必须比最长实际行多 3 个字节，以容纳 `\r`、`\n` 和结尾的 `\0`。

## CMake、Windows 和 CTest

本目录新增的 `CMakeLists.txt` 可以单独使用，也可以由仓库根 CMake 集成。它
不依赖 Qt、vcpkg 或其他第三方库，支持 MSVC、MinGW 和常见的 GCC/Clang。

在 Visual Studio Developer Command Prompt 中使用 Windows x64 预设：

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
ctest --test-dir build/windows-x64-release --output-on-failure
```

Debug 版本：

```powershell
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug
ctest --test-dir build/windows-x64-debug --output-on-failure
```

也可以从仓库根目录执行：

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release
ctest --test-dir build/vs2026-x64 --output-on-failure
```

常用 CMake 选项：

- `-DINIH_BUILD_SHARED=ON`：构建 DLL；默认构建静态库；
- `-DINIH_BUILD_INIREADER=OFF`：不构建 C++ 封装；
- `-DINIH_BUILD_EXAMPLES=OFF`：不构建示例；
- `-DINIH_BUILD_TESTS=OFF`：不配置 CTest；
- `-DINI_MAX_LINE=1000`、`-DINI_ALLOW_NO_VALUE=ON`、
  `"-DINI_INLINE_COMMENT_PREFIXES=#"` 等：设置对应解析宏。

这些 CMake 选项会写入构建目录中的 `inih_config.h`，库和示例会自动包含它；
直接使用 `ini.c` 时则继续使用 `ini.h` 自带的默认值。

Windows DLL 构建会通过 `INI_SHARED_LIB` 和 `INI_SHARED_LIB_BUILDING` 自动设置
`__declspec(dllexport/dllimport)`。静态库不会引入 DLL 导入符号。

Windows 文件名接口仍然使用 `const char*`，解析器不会猜测或转换文件名编码。
若路径包含非 ASCII 字符，调用方可使用 `_wfopen()` 打开文件，再把得到的
`FILE*` 传给 `ini_parse_file()`：

```cpp
FILE* file = _wfopen(L"配置.ini", L"rb");
if (file) {
    int result = ini_parse_file(file, handler, user);
    fclose(file);
}
```

CMake 在 MSVC 下使用 `/utf-8`，只负责源文件、注释和示例诊断文本的编译编码；
INI 内容本身仍按字节传递，不会自动从 GBK、UTF-8 或其他编码互转。

CTest 会编译 15 个 C 回归变体，并在 Windows/Linux 上使用同一个 CMake 脚本将
程序输出与 `tests/baseline_*.txt` 比较（统一 CRLF/LF 换行差异）。原有 `unittest.bat`、
`unittest.sh` 和 Meson 构建仍保留，便于与上游流程对照。

## 目录说明

- `ini.c`、`ini.h`：C 解析器实现和公开 API；
- `cpp/`：C++ `INIReader` 封装；
- `examples/`：C、C++、X-Macros 和内容转储示例；
- `tests/`：解析规则、内存策略、错误处理和字符串输入测试；
- `fuzzing/`：用于模糊测试的辅助程序和脚本；
- `meson.build`、`meson_options.txt`：原有 Meson 构建入口；
- `CMakeLists.txt`、`CMakePresets.json`：新增跨平台 CMake/Windows 入口。

本中文化版本只调整注释、文档、示例诊断信息和构建入口；解析状态机、数据
生命周期、公开函数名、宏名和测试输入保持不变。
