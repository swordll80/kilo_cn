/* ini.h 示例：输出 INI 文件中的内容，不输出注释。 */

#include <stdio.h>
#include <string.h>
#include "../ini.h"

static int dumper(void* user, const char* section, const char* name,
                  const char* value)
{
    (void)user;
    static char prev_section[50] = "";

    if (strcmp(section, prev_section)) {
        printf("%s[%s]\n", (prev_section[0] ? "\n" : ""), section);
        strncpy(prev_section, section, sizeof(prev_section));
        prev_section[sizeof(prev_section) - 1] = '\0';
    }
    printf("%s = %s\n", name, value);
    return 1;
}

int main(int argc, char* argv[])
{
    int error;

    if (argc <= 1) {
        printf("用法：ini_dump 文件名.ini\n");
        return 1;
    }

    error = ini_parse(argv[1], dumper, NULL);
    if (error < 0) {
        printf("无法读取 '%s'！\n", argv[1]);
        return 2;
    }
    else if (error) {
        printf("配置文件错误（第一个错误位于第 %d 行）！\n", error);
        return 3;
    }
    return 0;
}
