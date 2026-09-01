/* inih -- 简洁的 .INI 文件解析器

SPDX-License-Identifier: BSD-3-Clause

Copyright (C) 2009-2025, Ben Hoyt

inih is released under the New BSD license (see LICENSE.txt). Go to the project
home page for more info:

https://github.com/benhoyt/inih

*/

#ifndef INI_H
#define INI_H

/* 让该头文件可以直接用于 C++ 代码。 */
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/* 非零时，ini_handler 回调函数会接收行号参数。 */
#ifndef INI_HANDLER_LINENO
#define INI_HANDLER_LINENO 0
#endif

/* Windows DLL 所需的符号导出/导入定义。 */
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

/* 回调处理函数的类型定义。

   虽然 value 参数的类型是 "const char*"，调用方仍可以将其转换为
   "char*" 后修改内容，因为 ini_handler 返回后解析器不会再次使用 value。
   section 和 name 不同，不能修改它们指向的内容。
*/
#if INI_HANDLER_LINENO
typedef int (*ini_handler)(void* user, const char* section,
                           const char* name, const char* value,
                           int lineno);
#else
typedef int (*ini_handler)(void* user, const char* section,
                           const char* name, const char* value);
#endif

/* 类 fgets 读取函数的类型定义。 */
typedef char* (*ini_reader)(char* str, int num, void* stream);

/* 解析指定的 INI 格式文件。文件可以包含 [section]、name=value 键值对
   （会去除首尾空白）以及以 ';'（分号）开头的注释。在遇到任何 section
   之前解析到的 name=value，其 section 为空字符串。为兼容 Python 的
   configparser，也支持 name:value 形式的键值对。

   每解析到一个 name=value 键值对，就使用 user 指针以及 section、name、
   value 调用 handler（这些数据只在本次回调期间有效）。handler 返回非零
   表示成功，返回零表示出错。

   成功返回 0；解析错误返回第一个错误所在的行号（默认不会在第一个错误
   处停止）；文件打开失败返回 -1；内存分配失败返回 -2（仅在
   INI_USE_STACK 为 0 时可能发生）。
*/
INI_API int ini_parse(const char* filename, ini_handler handler, void* user);

/* 与 ini_parse() 相同，但接收 FILE* 而不是文件名。解析完成后不会关闭
   文件，调用方必须自行关闭。 */
INI_API int ini_parse_file(FILE* file, ini_handler handler, void* user);

/* 与 ini_parse() 相同，但接收 ini_reader 函数指针而不是文件名，用于实现
   自定义 I/O 或字符串输入（另请参见 ini_parse_string）。 */
INI_API int ini_parse_stream(ini_reader reader, void* stream, ini_handler handler,
                     void* user);

/* 与 ini_parse() 相同，但接收以 '\0' 结尾的 INI 数据字符串而不是文件。
   适合解析网络套接字数据或已经位于内存中的 INI 数据。 */
INI_API int ini_parse_string(const char* string, ini_handler handler, void* user);

/* 与 ini_parse_string() 相同，但同时接收字符串长度，避免调用 strlen()。
   适合解析网络套接字数据、内存中的非 '\0' 结尾数据，或对接 C++ 的
   std::string_view。 */
INI_API int ini_parse_string_length(const char* string, size_t length, ini_handler handler, void* user);

/* 非零时允许按 Python configparser 风格解析多行值。启用后，后续每一行
   都会以相同的 name 调用 handler。 */
#ifndef INI_ALLOW_MULTILINE
#define INI_ALLOW_MULTILINE 1
#endif

/* 非零时允许文件开头出现 UTF-8 BOM 序列（0xEF 0xBB 0xBF）。参见
   https://github.com/benhoyt/inih/issues/21 */
#ifndef INI_ALLOW_BOM
#define INI_ALLOW_BOM 1
#endif

/* 行首注释的起始字符。按照 Python configparser 的规则，默认允许使用
   ; 和 # 作为行首注释。 */
#ifndef INI_START_COMMENT_PREFIXES
#define INI_START_COMMENT_PREFIXES ";#"
#endif

/* 非零时允许行内注释（有效的注释字符由 INI_INLINE_COMMENT_PREFIXES
   指定）。设为 0 可关闭此功能，以匹配 Python 3.2+ configparser 的行为。 */
#ifndef INI_ALLOW_INLINE_COMMENTS
#define INI_ALLOW_INLINE_COMMENTS 1
#endif
#ifndef INI_INLINE_COMMENT_PREFIXES
#define INI_INLINE_COMMENT_PREFIXES ";"
#endif

/* 非零时使用栈上的行缓冲区，设为 0 时使用堆（malloc/free）。 */
#ifndef INI_USE_STACK
#define INI_USE_STACK 1
#endif

/* INI 文件中任意一行的最大长度（栈或堆缓冲区均适用）。注意该值必须
   比最长的实际行多 3 个字节，因为还需要容纳 '\r'、'\n' 和 '\0'。 */
#ifndef INI_MAX_LINE
#define INI_MAX_LINE 200
#endif

/* 非零时允许通过 realloc() 增长堆上的行缓冲区，设为 0 时使用固定的
   INI_MAX_LINE 字节缓冲区。仅在 INI_USE_STACK 为 0 时有效。 */
#ifndef INI_ALLOW_REALLOC
#define INI_ALLOW_REALLOC 0
#endif

/* 堆上行缓冲区的初始字节数。仅在 INI_USE_STACK 为 0 时有效。 */
#ifndef INI_INITIAL_ALLOC
#define INI_INITIAL_ALLOC 200
#endif

/* 在第一个错误处停止解析（默认会继续解析）。 */
#ifndef INI_STOP_ON_FIRST_ERROR
#define INI_STOP_ON_FIRST_ERROR 0
#endif

/* 非零时，在每个新 section 开始处调用 handler（此时 name 和 value 为
   NULL）。默认只对每个 name=value 键值对调用 handler。 */
#ifndef INI_CALL_HANDLER_ON_NEW_SECTION
#define INI_CALL_HANDLER_ON_NEW_SECTION 0
#endif

/* 非零时允许没有值的 name（该行没有 '=' 或 ':'），并在此时以 value 为
   NULL 调用 handler。默认将没有值的行视为错误。 */
#ifndef INI_ALLOW_NO_VALUE
#define INI_ALLOW_NO_VALUE 0
#endif

/* 非零时使用自定义的 ini_malloc、ini_free 和 ini_realloc 内存分配函数
   （INI_USE_STACK 也必须为 0）。这些函数必须与 malloc/free/realloc 具有
   相同的签名并遵循相近的行为。只有设置 INI_ALLOW_REALLOC 时才需要
   提供 ini_realloc。 */
#ifndef INI_CUSTOM_ALLOCATOR
#define INI_CUSTOM_ALLOCATOR 0
#endif


#ifdef __cplusplus
}
#endif

#endif /* INI_H */
