/* 使用 X-Macros 将配置文件解析到结构体。 */

#include <stdio.h>
#include <string.h>
#include "../ini.h"

#ifdef _WIN32
#define strdup _strdup
#endif

/* 定义配置结构体类型。 */
typedef struct {
    #define CFG(s, n, default) char *s##_##n;
    #include "config.def"
} config;

/* 创建配置对象并填充默认值。 */
config Config = {
    #define CFG(s, n, default) default,
    #include "config.def"
};

/* 处理 INI 文件中的一行，将有效值保存到配置结构体。 */
int handler(void *user, const char *section, const char *name,
            const char *value)
{
    config *cfg = (config *)user;

    if (0) ;
    #define CFG(s, n, default) else if (strcmp(section, #s) == 0 && \
        strcmp(name, #n) == 0) cfg->s##_##n = strdup(value);
    #include "config.def"

    return 1;
}

/* 逐行输出配置结构体中的全部变量。 */
void dump_config(config *cfg)
{
    #define CFG(s, n, default) printf("%s_%s = %s\n", #s, #n, cfg->s##_##n);
    #include "config.def"
}

int main(void)
{
    if (ini_parse("test.ini", handler, &Config) < 0)
        printf("无法加载 'test.ini'，将使用默认值。\n");
    dump_config(&Config);
    return 0;
}
