/* 这是为模糊测试略作调整的 tests/unittest.c 副本。 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ini.h"

int User;
char Prev_section[50];

int dumper(void* user, const char* section, const char* name,
           const char* value)
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

    printf("... %s%s%s;\n", name, value ? "=" : "", value ? value : "");

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

int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("用法：inihfuzz 文件名.ini\n");
        return 1;
    }
    parse(argv[1]);
    return 0;
}
