/* inih -- 测试程序

测试程序会将信息输出到标准输出；输出重定向到 baseline_*.txt 后纳入
Git 仓库。baseline 文件就是测试结果，若结果发生变化，应检查差异并
确认具体失败的测试。

使用 tcc 或 gcc 执行测试的方法见 unittest.bat 和 unittest.sh。

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../ini.h"

int User;
char Prev_section[50];

#if INI_HANDLER_LINENO
int dumper(void* user, const char* section, const char* name,
           const char* value, int lineno)
#else
int dumper(void* user, const char* section, const char* name,
           const char* value)
#endif
{
    User = *((int*)user);
    if (!name || strcmp(section, Prev_section)) {
        printf("... [%s]\n", section);
        strncpy(Prev_section, section, sizeof(Prev_section));
        Prev_section[sizeof(Prev_section) - 1] = '\0';
    }
    if (!name) {
        return 1;
    }

#if INI_HANDLER_LINENO
    printf("... %s%s%s;  line %d\n", name, value ? "=" : "", value ? value : "", lineno);
#else
    printf("... %s%s%s;\n", name, value ? "=" : "", value ? value : "");
#endif

    if (!value) {
        /* 当 INI_ALLOW_NO_VALUE=1 且该行没有值（没有 '=' 或 ':'）时出现。 */
        return 1;
    }

    return strcmp(name, "user") == 0 && strcmp(value, "parse_error") == 0 ? 0 : 1;
}

void parse(const char* fname) {
    static int u = 100;
    int e;

    *Prev_section = '\0';
    e = ini_parse(fname, dumper, &u);
    printf("%s: e=%d user=%d\n", fname, e, User);
    u++;
}

int main(void)
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    parse("no_file.ini");
    parse("normal.ini");
    parse("bad_section.ini");
    parse("bad_comment.ini");
    parse("user_error.ini");
    parse("multi_line.ini");
    parse("bad_multi.ini");
    parse("bom.ini");
    parse("duplicate_sections.ini");
    parse("no_value.ini");
    parse("long_section.ini");
    parse("long_line.ini");
    parse("name_only_after_error.ini");
    return 0;
}
