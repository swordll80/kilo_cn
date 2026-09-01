# jsmn 中文说明

[![Build Status](https://travis-ci.org/zserge/jsmn.svg?branch=master)](https://travis-ci.org/zserge/jsmn)

jsmn（读作 “jasmine”）是一个用 C 编写的极简 JSON 解析器。它不创建完整
的对象树，而是把输入 JSON 切分为一组标记（token），适合嵌入式系统和资源
受限的程序。

本目录保留 jsmn 的原始 API 和解析行为，仅补充中文说明、注释和 CMake 集成。
实现仍只有一个头文件 `jsmn.h`，不依赖第三方库，也不进行动态内存分配。

关于 JSON 格式的更多信息可参考 [json.org][1]。

上游源码位于 <https://github.com/zserge/jsmn>。

jsmn 项目介绍页位于 [http://zserge.com/jsmn.html][2]。

## 设计理念

许多 JSON 解析器会提供加载数据、解析数据以及按名称提取字段的一整套函数。
但对每个 JSON 数据包都做完整校验，或为解析字段分配临时对象，往往超出了
实际需要。

JSON 格式本身非常简单，因此解析器也应保持简单。

jsmn 追求 **健壮**（错误输入不会轻易导致崩溃）、**快速**（边读边解析）、
**可移植**（不依赖多余库或非标准 C 扩展）。同时，**简单**也是核心特性：
代码风格简单、算法简单、集成到其他项目简单。

## 特性

* 兼容 C89 风格的 C 代码（根项目使用 C99 编译）
* 不依赖第三方库
* 高度可移植（已在 x86/amd64、ARM、AVR 等平台使用）
* 代码体积很小
* 公开 API 只有 2 个函数
* 不进行动态内存分配
* 支持增量、单遍解析
* 自带单元测试

## 工作方式

jsmn 的基本结果是一个 **标记（token）**。考虑下面的 JSON：

	'{ "name" : "Jack", "age" : 27 }'

它包含以下标记：

* 对象：`{ "name" : "Jack", "age" : 27}`（整个对象）
* 字符串：`"name"`、`"Jack"`、`"age"`（键和部分值）
* 数字：`27`

标记本身不保存数据，只记录对应文本在 JSON 字符串中的边界。上例会生成类似
下面的标记：对象 [0..31]、字符串 [3..7]、字符串 [12..16]、字符串
[20..23]、数字 [27..29]。

每个 jsmn 标记都有类型，用来表示对应 JSON 内容的类型。支持的类型如下：

* 对象：键值对容器，例如：
	`{ "foo":"bar", "x":0.3 }`
* 数组：值的有序序列，例如：
	`[ 1, 2, 3 ]`
* 字符串：带双引号的字符序列，例如：`"foo"`
* 原始值：数字、布尔值（`true`、`false`）或 `null`

除 start/end 位置外，数组和对象等复杂类型的标记还记录直接子项数量，便于
遍历 JSON 层级。

这种方式已经足够表达任意 JSON 数据，也支持零拷贝读取。

## 使用方法

只需取得 `jsmn.h` 并包含它即可：

```
#include "jsmn.h"

...
jsmn_parser p;
jsmntok_t t[128]; /* 本示例预期最多使用 128 个 JSON 标记。 */

jsmn_init(&p);
r = jsmn_parse(&p, s, strlen(s), t, 128); /* s 指向 JSON 字符数组。 */
```

jsmn 是单头文件库。复杂项目可以定义附加宏：`JSMN_STATIC` 会将 API 符号
设为静态符号；如果多个 C 文件都包含 `jsmn.h`，可以用 `JSMN_HEADER` 让
部分文件只看到声明，从而避免重复定义。

```
/* 使用 jsmn 的其他 .c 文件只包含声明： */
#define JSMN_HEADER
#include "jsmn.h"

/* 另建一个 jsmn.c 文件提供实现： */
#include "jsmn.h"
```

## API

标记类型由 `jsmntype_t` 描述：

	typedef enum {
		JSMN_UNDEFINED = 0,
		JSMN_OBJECT = 1 << 0,
		JSMN_ARRAY = 1 << 1,
		JSMN_STRING = 1 << 2,
		JSMN_PRIMITIVE = 1 << 3
	} jsmntype_t;

**注意：** 与 JSON 的数据类型不同，原始值标记不会再细分数字、布尔值和
null，调用方可以通过首字符快速判断：

* <code>'t'、'f'</code>：布尔值
* <code>'n'</code>：null
* <code>'-'、'0'..'9'</code>：数字

标记的类型为 `jsmntok_t`：

	typedef struct {
		jsmntype_t type; // 标记类型
		int start;       // 起始偏移（包含）
		int end;         // 结束偏移（不包含）
		int size;        // 直接子标记数量
	} jsmntok_t;

**注意：** 字符串标记从起始双引号后的第一个字符开始，到结束双引号前一个
字符结束；这样可以简化从原始 JSON 中提取字符串。

所有解析状态都保存在 `jsmn_parser` 对象中。可以这样初始化并解析：

	jsmn_parser parser;
	jsmntok_t tokens[10];

	jsmn_init(&parser);

	// js：指向 JSON 字符串的指针
	// tokens：可写入的标记数组
	// 10：标记数组容量
	jsmn_parse(&parser, js, strlen(js), tokens, 10);

这会创建一个解析器，并尝试从 `js` 字符串中解析最多 10 个 JSON 标记。

`jsmn_parse` 返回非负值时，表示解析器实际使用的标记数量。
将 NULL 作为标记数组传入时不会保存解析结果，而是返回解析该字符串所需的
标记数量；当调用方无法预先确定容量时很有用。

解析失败时返回以下错误之一：

* `JSMN_ERROR_INVAL`：标记无效，JSON 字符串已损坏；
* `JSMN_ERROR_NOMEM`：标记空间不足，输入过大；
* `JSMN_ERROR_PART`：JSON 字符串尚未完整，还需要更多数据。

如果得到 `JSMN_ERROR_NOMEM`，可以扩容标记数组后再次调用 `jsmn_parse()`。
如果从流中读取 JSON，可以周期性调用 `jsmn_parse()`；在数据读完前，输入不
完整时会返回 `JSMN_ERROR_PART`。

## 其他说明

本软件使用 [MIT 许可证](http://www.opensource.org/licenses/mit-license.php)
发布，可以集成到商业产品中。完整版权和许可证文本见 [`LICENSE`](LICENSE)。

## VS2026 + CMake

根目录 `CMakeLists.txt` 已合并 jsmn 示例和测试目标。在 VS2026 Developer
PowerShell 或 Developer Command Prompt 中执行：

```powershell
cmake --preset vs2026-x64
cmake --build --preset vs2026-x64-release
ctest --test-dir build/vs2026-x64 -C Release --output-on-failure
```

默认会构建 Kilo、两个示例和四个测试变体。设置 `-DKILO_BUILD_JSMN=OFF`
可以只构建 Kilo。也可以使用 `vs2026-x64-debug` 预设构建 Debug 版本。

本目录中的示例和测试分别位于 `example/` 与 `test/`；测试覆盖默认模式、
`JSMN_STRICT`、`JSMN_PARENT_LINKS` 以及两个宏同时启用的组合。

[1]: http://www.json.org/
[2]: http://zserge.com/jsmn.html
