/* 示例：解析一个简单的配置文件。 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../ini.h"

#ifdef _WIN32
#define strdup _strdup
#endif

typedef struct
{
    int version;
    const char* name;
    const char* email;
} configuration;

static int handler(void* user, const char* section, const char* name,
                   const char* value)
{
    configuration* pconfig = (configuration*)user;

    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    if (MATCH("protocol", "version")) {
        pconfig->version = atoi(value);
    } else if (MATCH("user", "name")) {
        pconfig->name = strdup(value);
    } else if (MATCH("user", "email")) {
        pconfig->email = strdup(value);
    } else {
        return 0;  /* 未知的 section/name，报告错误。 */
    }
    return 1;
}

int main(void)
{
    configuration config;
    config.version = 0;  /* 设置默认值。 */
    config.name = NULL;
    config.email = NULL;

    if (ini_parse("test.ini", handler, &config) < 0) {
        printf("无法加载 'test.ini'\n");
        return 1;
    }
    printf("已从 'test.ini' 加载配置：version=%d，name=%s，email=%s\n",
        config.version, config.name, config.email);

    if (config.name)
        free((void*)config.name);
    if (config.email)
        free((void*)config.email);

    return 0;
}
