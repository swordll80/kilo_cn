# Kilo 中文版

Kilo 是一个极简的终端文本编辑器。原始项目约 1K 行；本版本补充了中文注释
和 Windows 终端适配，仍不依赖 curses 或其他第三方库，直接输出 VT100/ANSI
转义序列。

## 使用

```text
kilo [文件名]
```

不带文件名启动时会打开一个空白新文档。首次按 `Ctrl-S` 会在状态栏询问保存
文件名；输入文件名后按 `Enter` 保存，直接按 `Enter` 则保存为默认的 `temp.c`
（Windows 保存到 exe 所在目录，其他系统保存到当前目录），`Esc` 取消保存。

快捷键：

- `Ctrl-S`：保存文件
- `Ctrl-Q`：退出。存在未保存修改时，需要连续按 3 次确认
- `Ctrl-F`：搜索文本；`Esc` 取消，方向键切换匹配项，`Enter` 确认

编辑器以字节为单位处理文件内容，原有的轻量设计和语法高亮行为保持不变。
中文界面提示使用 UTF-8；中文文件内容可以正常读写，但光标和列宽仍按原项目
的字节模型计算，不提供全角字符对齐或多字节字符级编辑。语法高亮判断字符时
会按无符号字节处理，避免 UTF-8 中文被误判为不可打印字符。

Windows 下打开文件时会自动识别 UTF-8 或 CP936/GBK；GBK 文件在内存中转换为
UTF-8 后显示和编辑，保存时转换回 CP936，以兼容已有中文源文件。

## 构建

### VS2026 + CMake（推荐）

在 `VS2026 Developer Command Prompt (x64)` 中进入项目根目录后执行：

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release --parallel
```

Debug 构建：

```powershell
cmake --preset vs2026-x64-debug-config
cmake --build --preset vs2026-x64-debug --parallel
```

预设采用 `NMake Makefiles`，由 VS2026 Developer Command Prompt 提供 x64
MSVC 环境，避免不同安装路径下 Visual Studio 生成器的工具链探测差异。
也可以在 Visual Studio 2026 中直接打开本目录，让 IDE 使用其 CMake 配置。
生成的程序位于 `build/vs2026-x64/kilo.exe`；Debug 程序位于
`build/vs2026-x64-debug/kilo.exe`。

Windows 运行时需要支持 ANSI/VT100 输出序列的终端，推荐 Windows Terminal 或
较新的 Windows 控制台；程序启动时会自动设置 UTF-8 输出代码页。程序是控制台
应用，不要使用 `WIN32` 子系统配置。

### GCC / Make

```bash
make
./kilo [文件名]
```

## 项目来源

Kilo 由 Salvatore Sanfilippo（antirez）编写，本仓库保留原 BSD 2-Clause
许可证，英文原始说明见 [README.en.md](README.en.md)。原项目演示地址：
<https://asciinema.org/a/90r2i9bq8po03nazhqtsifksb>。
