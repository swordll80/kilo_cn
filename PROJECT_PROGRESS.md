# Kilo 中文化、jsmn-cn 集成与 VS2026 CMake 支持总结

## 1. 项目概况

本项目包含 Kilo 单文件终端文本编辑器和 `jsmn-cn` 极简 JSON 标记解析器。
Kilo 的核心数据模型仍然是按字节保存的行数组，终端界面仍然通过 VT100/ANSI
转义序列刷新；jsmn 仍保持单头文件、无第三方依赖和原始 API。本次工作只补充
中文体验、注释、说明文档和 Windows/VS2026 CMake 集成，不引入大型依赖。

## 2. 需求进度

| 序号 | 需求 | 详细说明 | 状态 | 问题备注 |
|---:|---|---|---|---|
| 1 | 翻译为中文 | README、TODO、运行提示、状态栏和搜索提示中文化；保留 `README.en.md` 作为英文原文 | 已实现 | 中文列宽仍按原有字节模型计算 |
| 2 | 补全注释 | 将主要模块、数据结构、终端处理、语法高亮、行编辑、文件读写、刷新和事件处理注释改为中文，并补充跨平台适配说明 | 已实现 | BSD 2-Clause 法律文本保留原文 |
| 3 | 编写总结文档 | 本文记录需求、架构、风格、验证边界和待办事项 | 已实现 | 后续验证结果应继续回填本表 |
| 4 | 支持 VS2026 CMake | 新增 CMake 入口和 VS2026 x64 Release/Debug 预设，MSVC 使用 UTF-8 编译选项和必要警告级别 | 已实现 | Kilo 目标要求在 VS2026 Developer Command Prompt 中执行；工作区其他 JSMN 示例仍有 POSIX 依赖 |
| 5 | Windows 控制台运行 | 适配原始输入、扩展按键、窗口尺寸、ANSI 输出和文件截断 | 部分实现 | 依赖支持 ANSI/VT100 的 Windows Terminal 或新式控制台 |
| 6 | 保持原有功能 | 保留打开、编辑、保存、搜索、语法高亮、滚动和未保存退出保护 | 已实现 | 尚未进行完整交互回归 |
| 7 | 修复调试运行乱码 | Windows 启动时将控制台输出代码页设置为 UTF-8，使中文窄字符串提示可正确显示 | 已实现 | 重定向到不支持 UTF-8 的外部查看器时仍取决于查看器编码 |
| 8 | 无文件名新建和保存 | 无参数启动空白文档；首次 `Ctrl-S` 询问文件名，留空时保存为默认 `temp.c` | 已实现 | Windows 默认位于 exe 所在目录，其他系统位于当前目录；尚未完整回归复杂路径和输入法 |
| 9 | 中文注释显示 | 修复 UTF-8 高位字节传入 ctype 函数时被误判为不可打印，并兼容 CP936/GBK 文件显示 | 已实现 | 中文仍按字节计算列宽，正好落在窗口边界时可能截断显示 |
| 10 | jsmn 中文化 | 将 `jsmn-cn` 的 README、API 说明、示例和测试输出改为中文，保留 C API、宏和 JSON 示例字段 | 已实现 | MIT 许可证法律文本保留原文 |
| 11 | jsmn 注释补全 | 为标记模型、解析状态、字符串转义、原始值、严格模式和测试辅助函数补充中文注释 | 已实现 | 只改注释和诊断文本，未改解析算法 |
| 12 | jsmn 合并根 CMake | 根 `CMakeLists.txt` 提供 `jsmn` INTERFACE 头文件目标、两个示例和四个测试变体 | 已实现 | 通过 `KILO_BUILD_JSMN=OFF` 可只构建 Kilo |
| 13 | jsmn CTest 回归 | 接入默认、`JSMN_STRICT`、`JSMN_PARENT_LINKS` 和组合模式测试 | 待验证 | 需在 VS2026 开发者环境中完成配置、构建和 CTest |

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
  Win32 控制台 API 和 `_getch`，通过条件编译隔离平台差异。
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

### 部分实现

- 编辑器仍按字节而非 Unicode 码点计算光标和列宽；这符合原项目设计，但中文
  全角字符可能出现光标对齐偏差。
- Windows 输入使用 `_getch`，适合控制台按键；复杂输入法、组合键和重定向输入
  不在本次范围内。

### 待验证

- Windows Terminal 中打开、编辑、搜索、手动输入文件名保存、调整窗口大小和未保存退出保护。
- Linux GCC/Make 构建及原始终端交互回归。

## 8. jsmn-cn 子项目说明

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

## 9. jsmn 验证记录

### 待执行命令

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release
ctest --test-dir build/vs2026-x64 -C Release --output-on-failure
```

验证完成后应在本节分别记录 CMake 配置/编译、CTest、示例运行和真实终端环境
结果；不能将仅有静态检查或启动成功描述为完整功能回归。

## 6. 本次验证记录

### 构建/静态证据

- VS2026 Developer Command Prompt（x64）中的 MSVC 19.51.36256.0 已完成 Kilo
  目标的 CMake Release 构建，产物为 `build/vs2026-x64/kilo.exe`。
- 同一环境已完成 Kilo 目标的 Debug 构建，产物为 `build/vs2026-x64-debug/kilo.exe`。
- 直接执行 `cl /nologo /TC /W4 /WX /utf-8` 编译通过，未产生警告。
- CMake 使用 `NMake Makefiles` 时 Kilo 目标配置、生成和编译均通过；NMake 忽略
  `--parallel` 是工具限制，不是 Kilo 构建失败。工作区其他 `jsmn-cn` 示例的完整
  构建另受 `unistd.h` POSIX 依赖影响。

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
- PTY 输出同时显示中文长行在列边界处会按字节截断，这是当前字节模型的已知限制，
  不代表文件内容被修改。
- 编辑、搜索、手动输入文件名保存、窗口调整大小和未保存退出保护尚未完成完整回归。

### 尚未覆盖的终端验证

- Windows Terminal/Windows 控制台中的编辑、搜索、手动输入文件名保存、窗口调整大小和未保存
  退出保护仍待人工回归；基础打开/退出已由上项覆盖。
- 工作区 `jsmn-cn/example/jsondump.c` 仍包含 POSIX `unistd.h` 依赖；完整构建该无关
  示例时会失败，Kilo Release/Debug 目标本身已单独通过。
- Linux GCC/Make 及真实终端回归仍待执行。

## 7. 推荐验证命令

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
