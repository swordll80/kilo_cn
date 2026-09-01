# Kilo 中文化、jsmn-cn/inih-cn 集成与 VS2026 CMake 支持总结

## 1. 项目概况

本项目包含 Kilo 单文件终端文本编辑器、`jsmn-cn` 极简 JSON 标记解析器、
`inih-cn` INI 文件解析器和 `sds-cn` 动态字符串库。Kilo 的核心数据模型仍然是按字节保存的行数组，
终端界面仍然通过 VT100/ANSI 转义序列刷新；jsmn 仍保持单头文件、inih 仍
保持原始 C/C++ API、解析状态机和测试输入。本次工作只补充中文体验、注释、
说明文档和 Windows/VS2026 CMake 集成，不引入大型依赖。

## 2. 需求进度

| 序号 | 需求 | 详细说明 | 状态 | 问题备注 |
|---:|---|---|---|---|
| 1 | 翻译为中文 | README、TODO、运行提示、状态栏和搜索提示中文化；保留 `README.en.md` 作为英文原文 | 已实现 | 中文列宽仍按原有字节模型计算 |
| 2 | 补全注释 | 将主要模块、数据结构、终端处理、语法高亮、行编辑、文件读写、刷新和事件处理注释改为中文，并补充跨平台适配说明 | 已实现 | BSD 2-Clause 法律文本保留原文 |
| 3 | 编写总结文档 | 本文记录需求、架构、风格、验证边界和待办事项 | 已实现 | 后续验证结果应继续回填本表 |
| 4 | 支持 VS2026 CMake | 新增 CMake 入口和 VS2026 x64 Release/Debug 预设，MSVC 使用 UTF-8 编译选项和必要警告级别 | 已实现 | Kilo 与 jsmn 目标均要求在 VS2026 Developer Command Prompt 中执行 |
| 5 | Windows 控制台运行 | 适配原始输入、扩展按键、窗口尺寸、ANSI 输出和文件截断 | 部分实现 | 依赖支持 ANSI/VT100 的 Windows Terminal 或新式控制台 |
| 6 | 保持原有功能 | 保留打开、编辑、保存、搜索、语法高亮、滚动和未保存退出保护 | 已实现 | 尚未进行完整交互回归 |
| 7 | 修复调试运行乱码 | Windows 启动时将控制台输出代码页设置为 UTF-8，使中文窄字符串提示可正确显示 | 已实现 | 重定向到不支持 UTF-8 的外部查看器时仍取决于查看器编码 |
| 8 | 无文件名新建和保存 | 无参数启动空白文档；首次 `Ctrl-S` 询问文件名，留空时保存为默认 `temp.c` | 已实现 | Windows 默认位于 exe 所在目录，其他系统位于当前目录；尚未完整回归复杂路径和中文输入法 |
| 9 | 中文注释显示 | 修复 UTF-8 高位字节传入 ctype 函数时被误判为不可打印，并兼容 CP936/GBK 文件显示 | 已实现 | 中文仍按字节计算列宽，正好落在窗口边界时可能截断显示 |
| 10 | 删除安装目标 | 移除 CMake 的 `install()` 规则，避免生成 `INSTALL`/“安装”构建目标；仍保留 Release/Debug 编译 | 已实现 | 只生成并使用构建目录中的 exe，不提供安装包或安装目录 |
| 11 | jsmn 中文化 | 将 `jsmn-cn` 的 README、API 说明、示例和测试输出改为中文，保留 C API、宏和 JSON 示例字段 | 已实现 | MIT 许可证法律文本保留原文 |
| 12 | jsmn 注释补全 | 为标记模型、解析状态、字符串转义、原始值、严格模式和测试辅助函数补充中文注释 | 已实现 | 只改注释和诊断文本，未改解析算法 |
| 13 | jsmn 合并根 CMake | 根 `CMakeLists.txt` 提供 `jsmn` INTERFACE 头文件目标、两个示例和四个测试变体 | 已实现 | 通过 `KILO_BUILD_JSMN=OFF` 可只构建 Kilo |
| 14 | jsmn CTest 回归 | 接入默认、`JSMN_STRICT`、`JSMN_PARENT_LINKS` 和组合模式测试 | 已验证 | 根目录 VS2026 x64 Release 集成 CTest 中 4/4 通过 |
| 15 | 中文键盘输入 | Windows 使用 `_getwch` 读取 Unicode 字符，转换为 UTF-8 字节队列后交给现有编辑逻辑 | 已实现 | 仍需在真实 Windows Terminal 中完整回归输入法、组合键和超宽字符 |
| 16 | inih-cn 注释中文化 | 翻译 C/C++ 头文件、解析实现、示例、测试辅助代码和模糊测试注释；保留 SPDX、版权和 API 标识 | 已实现 | 解析算法、公开函数名、宏名和测试输入未改 |
| 17 | inih-cn 文档总结 | 新增中文 README，记录 API、解析规则、宏选项、目录职责、Windows 编码边界和验证命令 | 已实现 | 上游链接和 BSD-3-Clause 法律文本保留 |
| 18 | inih-cn CMake 配置 | 新增独立 CMake 入口、静态/DLL、C++ INIReader、示例和 CTest 配置；根 CMake 可选集成 | 已实现 | CMake/MSVC 实际构建结果见 inih 验证记录 |
| 19 | inih-cn Windows 支持 | MSVC `/utf-8`、`/W4`、DLL 导入导出宏、MinGW 兼容和 `_wfopen` 路径说明 | 已实现 | INI 文件内容不做自动编码转换；目标机 Unicode 路径仍需调用方使用 `_wfopen` |
| 20 | inih-cn CTest 回归 | 15 个 C 测试变体使用跨平台 CMake 脚本比较 baseline 输出 | 已验证 | MSVC x64 Release/Debug 均为 15/15 通过 |
| 21 | sds-cn 中文化 | 中文 README、源码注释、测试输出、Changelog、Make/CMake 入口；保留 SDS C API、动态头部布局和 BSD 法律文本；Windows 测试入口设置 UTF-8 输出代码页 | 已实现/已验证 | GCC 与 VS2026 MSVC 均完成 46 项回归；重定向文件的查看器编码、真实项目接入和分配器替换仍需单独验证 |

## 3. 代码结构与数据管理风格

- `editorConfig E` 是唯一的编辑器运行状态，包含光标、滚动、文件行、脏
  标记、文件名和语法方案。
- `erow` 保存一行的原始字符、制表符展开后的显示文本和逐字节高亮结果。
- 文件打开时逐行加载；保存时把所有行拼接为一个缓冲区，再截断并写入目标文件。
- 修改后通过 `dirty` 标记保护用户数据；未保存时连续按 `Ctrl-Q` 进行退出确认。
- 无参数启动时 `E.filename` 为 `NULL`；首次保存再绑定用户输入的路径，空输入则
  生成默认 `temp.c` 路径，避免启动阶段猜测文件名。
- 当前项目规模很小，继续维护时应优先使用局部补丁，避免拆散单文件结构或
  引入大型依赖。

## 4. UI 与架构风格约定

- UI 使用终端底部两行状态栏：第一行显示文件状态和行号，第二行显示临时提示。
- 普通提示使用中文，快捷键和 API/协议名称保留英文标识，例如 `Ctrl-S`、
  `VT100`、`MSVC`。
- Linux 分支继续使用 `termios`、`ioctl` 和 `SIGWINCH`；Windows 分支使用
  Win32 控制台 API 和 `_getwch`，通过条件编译隔离平台差异；Unicode 输入转换为
  UTF-8 后复用现有单字节编辑逻辑。
- CMake 目标为控制台应用 `kilo`，不使用 `WIN32` 子系统；MSVC 源文件编码
  明确设置为 UTF-8。

## 5. 已知边界与待验证项

### 已实现

- `CMakeLists.txt` 定义 C99 控制台目标、MSVC `/W4 /utf-8` 和 GCC 警告选项。
- `CMakePresets.json` 提供 `vs2026-x64`、Release 和 Debug 构建预设；预设使用
  `NMake Makefiles`，由 VS2026 Developer Command Prompt 提供 x64 工具链。
- Windows 终端原始模式、扩展方向键、窗口尺寸查询、ANSI 输出模式和文件截断
  已有适配代码。
- Windows 启动时调用 `SetConsoleOutputCP(CP_UTF8)`，统一中文提示的控制台输出编码。
- `getline` 替换为项目内自带的动态逐行读取，避免 MSVC 缺少 POSIX 接口。
- 无文件名启动、状态栏文件名显示、保存文件名提示和默认 `temp.c` 路径已加入；
  Windows 默认路径由 `GetModuleFileNameA` 解析为 exe 所在目录。
- `isprint`、`isdigit`、`isspace` 等 ctype 调用已统一转换为 `unsigned char`，UTF-8
  中文注释不会再被当作不可打印字符替换。
- Windows 会检测 UTF-8/CP936；打开 CP936 文件时转换为 UTF-8 供编辑器使用，保存
  时转换回 CP936，保持已有中文文件的编码兼容性。
- Windows 键盘输入通过 `_getwch` 获取 Unicode 字符，并拆分为 UTF-8 字节交给原有
  编辑逻辑，避免中文输入直接产生 GBK/UTF-8 混用。

### 部分实现

- 编辑器仍按字节而非 Unicode 码点计算光标和列宽；这符合原项目设计，但中文
  全角字符可能出现光标对齐偏差。
- Windows 输入使用 `_getwch`，中文字符会转换为 UTF-8 字节队列；复杂输入法、组合键和重定向输入
  不在本次范围内。

### 待验证

- Windows Terminal 中打开、编辑、搜索、手动输入文件名保存、调整窗口大小和未保存退出保护。
- Linux GCC/Make 构建及原始终端交互回归。

## 6. jsmn-cn 子项目说明

### 目录与职责

- `jsmn-cn/jsmn.h`：库的唯一实现文件，调用方提供输入缓冲区和标记数组；
- `jsmn-cn/example/simple.c`：已知 JSON 结构下按键读取值的最小示例；
- `jsmn-cn/example/jsondump.c`：从标准输入增量读取 JSON 并递归打印标记；
- `jsmn-cn/test/tests.c`：覆盖容器、字符串、原始值、增量输入、容量不足、
  括号错误和标记计数的回归测试；
- `jsmn-cn/README.md`：中文 API、设计和构建说明。

### 数据与 API 约定

- 标记不拥有或复制 JSON 文本；`start` 包含起点，`end` 不包含终点，二者是
  输入缓冲区的字节偏移；
- `size` 表示直接子标记数量，不是递归子孙总数；
- `tokens == NULL` 用于只统计所需标记数；
- `JSMN_ERROR_NOMEM` 表示标记池不足，调用方可以扩容后从解析器保留的位置
  继续调用；`JSMN_ERROR_PART` 表示流式输入尚未完整；
- 默认模式保持上游的宽松解析行为；需要更严格的结构检查时定义
  `JSMN_STRICT`，需要父链时定义 `JSMN_PARENT_LINKS`。

### CMake 目标与统一风格

- `jsmn`：INTERFACE 头文件目标，公开 `jsmn-cn` 包含目录；
- `jsmn_simple_example`、`jsmn_jsondump_example`：两个示例程序；
- `jsmn_test_default`、`jsmn_test_strict`、`jsmn_test_parent_links`、
  `jsmn_test_strict_parent_links`：四个可执行测试目标，同时注册到 CTest；
- Windows/MSVC 使用 `/W4 /utf-8` 和 `_CRT_SECURE_NO_WARNINGS`；其他编译器
  使用 `-Wall -Wextra -pedantic`；所有目标按 C99、关闭编译器扩展。

## 7. jsmn 验证记录

### VS2026 构建与 CTest 证据

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release
ctest --test-dir build/vs2026-x64 -C Release --output-on-failure
```

- VS2026 Developer Command Prompt（x64）中的 CMake 4.3.1-msvc1 完成
  `vs2026-x64` Release 配置和构建；Kilo、两个 jsmn 示例及四个测试目标均生成。
- 同一环境完成 `vs2026-x64-debug` Debug 配置和构建，八个目标构建流程无错误。
- Release CTest：`jsmn_test_default`、`jsmn_test_strict`、
  `jsmn_test_parent_links`、`jsmn_test_strict_parent_links` 共 4/4 通过。
- Debug CTest：上述四项共 4/4 通过。
- Release 示例运行通过：`jsmn_simple_example` 输出用户、管理员、用户编号和
  用户组；`jsmn_jsondump_example` 成功解析包含中文字符串的 JSON 并递归输出。

## 8. Kilo 本次验证记录

### 构建/静态证据

- VS2026 Developer Command Prompt（x64）中的 MSVC 19.51.36256.0 已完成 Kilo
  目标的 CMake Release 构建，产物为 `build/vs2026-x64/kilo.exe`。
- 同一环境已完成 Kilo 目标的 Debug 构建，产物为 `build/vs2026-x64-debug/kilo.exe`。
- 直接执行 `cl /nologo /TC /W4 /WX /utf-8` 编译通过，未产生警告。
- CMake 使用 `NMake Makefiles` 时 Kilo 与 jsmn 目标配置、生成和编译均通过；NMake
  忽略 `--parallel` 是工具限制，不是构建失败。

### 终端运行证据

- 已在 Windows PTY 中打开现有 `README.md`，观察到 ANSI 全屏刷新，并发送
  `Ctrl-Q` 正常退出（进程返回 0，文件未修改）。
- 已在 Windows PTY 中直接启动 Release 程序，确认 `用法：kilo [文件名]` 正常显示，
  不再出现调试控制台截图中的乱码。
- 已补充无参数启动和首次保存流程：空白文档启动时状态栏显示“未命名”，首次
  `Ctrl-S` 进入保存名提示，直接回车使用 exe 所在目录的 `temp.c`。
- 已重新打开包含中文注释的 `kilo.c`，确认中文可以正常输出；窗口边界处的半个
  UTF-8 字符截断仍属于按字节列宽计算的已知限制。
- 已使用现有 CP936/GBK 编码的 `build/vs2026-x64/temp.c` 复现并验证，屏幕显示从
  `��` 修复为正确的“整数”“注释”。
- 中文键盘输入路径已改为 `_getwch` + UTF-8 队列；本次已完成编译验证，真实输入法
  行为仍需人工回归。
- PTY 输出同时显示中文长行在列边界处会按字节截断，这是当前字节模型的已知限制，
  不代表文件内容被修改。
- 编辑、搜索、手动输入文件名保存、窗口调整大小和未保存退出保护尚未完成完整回归。

### 尚未覆盖的终端验证

- Windows Terminal/Windows 控制台中的编辑、搜索、手动输入文件名保存、窗口调整大小和未保存
  退出保护仍待人工回归；基础打开/退出已由上项覆盖。
- jsmn 示例已完成本机 Windows 编译和基础运行验证；尚未在 Linux GCC/Make 下执行
  同等回归。
- Linux GCC/Make 及真实终端回归仍待执行。

## 9. inih-cn 子项目说明

### 目录与职责

- `inih-cn/ini.c`、`inih-cn/ini.h`：无第三方依赖的 C INI 解析器和公开 API；
- `inih-cn/cpp/`：保存键值并提供类型转换的 `INIReader` C++ 封装；
- `inih-cn/examples/`：C、C++、X-Macros 和 INI 转储示例；
- `inih-cn/tests/`：解析规则、长行、错误、字符串输入和堆分配测试；
- `inih-cn/fuzzing/`：模糊测试辅助程序和脚本；
- `inih-cn/CMakeLists.txt`：静态库/DLL、INIReader、示例和 CTest 入口；
- `inih-cn/inih_config.h.in`：CMake 配置头模板，避免 Windows 编译器命令行
  无法安全传递包含 `#` 的注释前缀；
- `inih-cn/CMakePresets.json`：Windows x64 Release/Debug 的 NMake 预设；
- `inih-cn/README.md`：中文使用说明、配置选项和验证边界。

### 数据与 API 约定

- C API 将输入缓冲区中的 section/name/value 以 `const char*` 传给回调，数据只
  在回调期间有效；需要持久化时由调用方复制；
- `ini_parse()` 返回 0 表示成功，正数表示首个错误行号，-1 表示文件打开失败，
  -2 表示堆分配失败；
- `INIReader` 将键按 `section=name` 组合并转为小写后存入 `std::map`，重复键的
  多行值按换行拼接；
- Windows 文件名接口不做编码猜测。需要 Unicode 路径时，由调用方 `_wfopen()`
  后调用 `ini_parse_file()`；INI 内容本身仍按原始字节解释。

### CMake 目标与构建风格

- `inih`：C 静态库，`INIH_BUILD_SHARED=ON` 时构建 DLL；
- `INIReader`：可选 C++11 库，依赖 `inih`；
- `inih_example`、`inih_ini_dump`、`inih_ini_xmacros`：C 示例；
- `inih_inireader_example`、`inih_inireader_errors`：C++ 示例；
- `inih_test_*`：15 个 C 回归测试，CTest 使用 `tests/verify_output.cmake` 比较
  `baseline_*.txt`（统一 CRLF/LF 换行差异），不依赖 Bash、`diff` 或 `tcc`；
- `inih_config.h`：由 CMake 根据 `INI_*` 选项生成，供库和示例统一使用；直接
  编译 `ini.c` 时不生成该文件，仍使用 `ini.h` 的默认值；
- MSVC 目标统一使用 `/W4 /utf-8` 和 `_CRT_SECURE_NO_WARNINGS`，其他编译器
  使用 `-Wall -Wextra -pedantic`；不引入 Qt、vcpkg 或大型第三方库。

## 10. inih-cn 验证记录

### 当前状态

- 已完成中文注释、README、示例提示和 C++ `ParseErrorMessage()` 的局部修改；
- 已完成独立 `CMakeLists.txt`、Windows Release/Debug 预设和跨平台基线校验脚本；
- 已在 Visual Studio 2026 x64 Developer Command Prompt、MSVC 19.51 下完成
  Release/Debug 静态库构建、C++ 封装构建和 15/15 CTest；Release DLL 构建和
  C/C++ 示例运行也已完成；
- 已使用 Qt MinGW-w64 GCC 15.2 和 `mingw32-make` 完成 Release 静态库、C++
  封装、示例构建和 15/15 CTest；
- 已在根目录集成构建中完成 Kilo、jsmn 和 inih 目标构建，根 CTest 19/19 通过；
- 旧有 Meson、`unittest.bat` 和 `unittest.sh` 入口未删除，便于与上游对照。

### 推荐验证命令

在 Visual Studio Developer Command Prompt（x64）中：

```powershell
cd C:\codex\kilo_cn\inih-cn
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --parallel
ctest --test-dir build/windows-x64-release --output-on-failure
```

根目录集成验证：

```powershell
cd C:\codex\kilo_cn
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release --parallel
ctest --test-dir build/vs2026-x64 -C Release --output-on-failure
```

验证时继续分别记录：CMake/编译/CTest 静态证据、示例进程运行证据，以及真实
Windows 目标机上的 Unicode 路径和 DLL 部署证据。当前尚未覆盖真实目标机的
Unicode 路径、DLL 外部部署和 Linux GCC/Make 构建。

## 11. sds-cn 子项目说明

### 目录与职责

- `sds-cn/sds.c`：SDS 2.0 的唯一实现文件，包含动态头部、扩容、追加、裁剪、
  拆分、连接和分配器包装；`SDS_TEST_MAIN` 打开时包含内置回归测试入口。
- `sds-cn/sds.h`：公开 `sds` 类型、长度/容量内联函数和全部 C API；Windows
  的紧凑头部布局与 `ssize_t` 兼容定义集中在此处。
- `sds-cn/sdsalloc.h`：`s_malloc`、`s_realloc`、`s_free` 三个分配器宏。
- `sds-cn/testhelp.h`：内置测试输出和汇总辅助宏，普通测试状态已中文化。
- `sds-cn/Makefile`：保持上游单命令测试入口，并补充 `make test`。
- `sds-cn/CMakeLists.txt`、`sds-cn/CMakePresets.json`：独立 CMake 构建和 CTest
  入口；根 CMake 通过 `KILO_BUILD_SDS` 选择是否集成。
- `sds-cn/README.md`：中文 API、内存布局、分配器、编码边界和验证说明；文档
  后半部分保留上游英文参考。

### 数据与 API 约定

- SDS 对外是 `char *`，头部位于返回指针之前；`sdslen()`、`sdsavail()` 和
  `sdsalloc()` 读取的是字节长度和容量，不是 Unicode 字符数。
- 可能扩容的函数返回新指针，调用方必须接收返回值；内存不足返回 `NULL`。
  `sdsfree(NULL)` 安全，`sdsclear()` 只清空内容并保留已有容量。
- `sdsnewlen()`、`sdscatlen()`、`sdscpylen()` 和 `sdssplitlen()` 按显式字节
  长度处理，可保存中间包含空字符的二进制数据。
- SDS 2.0 根据长度选择 5/8/16/32/64 位头部；类型 5 不记录剩余空间，追加
  时会升级到可以记录容量的头部类型。
- `sdsalloc.h` 默认使用 libc 分配器；替换三个宏即可接入项目分配器，不改变
  公开 API。

### sds-cn 验证记录

- MinGW-w64 GCC 15.2 使用 `-Wall -Wextra -std=c99 -pedantic -O2` 完成直接构建，
  `sds-test` 运行结果为 46/46 通过；独立 CMake 的 `sds_test` 为 1/1 通过。
- 根 CMake 新增 `sds` 静态库和 `sds_test` CTest；MinGW 根工程 Release 目标和
  CTest 共 20/20 通过，VS2026 Developer Command Prompt 的 MSVC Release 根工程
  目标和 CTest 也共 20/20 通过。
- MSVC 构建保留上游窄类型转换和变量遮蔽警告，但无编译错误；测试中的 1 MiB
  基准数组已移到静态存储区，避免 Windows 默认栈空间导致运行时异常。
- Windows `sds_test` 测试入口设置 `CP_UTF8` 后，控制台中文状态文字按 UTF-8 输出；
  SDS 库本身不修改宿主程序的控制台代码页。输出重定向文件的查看器编码尚未验证。
- 尚未覆盖真实项目接入、跨模块分配器替换、Windows/MSVC 目标机运行和中文
  文本按字节处理边界；这些不应标记为已验证。

## 12. 推荐验证命令

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release --parallel
cmake --preset vs2026-x64-debug-config
cmake --build --preset vs2026-x64-debug --parallel
```

```bash
make clean
make
./kilo README.md
```

验证时应分别记录：构建/静态检查结果、终端 GUI 交互结果，以及真实目标环境
结果；未执行的类别不能标记为已验证。
