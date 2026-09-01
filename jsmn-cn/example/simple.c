#include "../jsmn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * 一个简单的 jsmn 使用示例：JSON 结构已知，所需标记数量也可以预估。
 */

static const char *JSON_STRING =
    "{\"user\": \"johndoe\", \"admin\": false, \"uid\": 1000,\n  "
    "\"groups\": [\"users\", \"wheel\", \"audio\", \"video\"]}";

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  /* 比较字符串标记的内容；返回 0 表示相等。 */
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

int main() {
  int i;
  int r;
  jsmn_parser p;
  jsmntok_t t[128]; /* 本示例预期最多使用 128 个标记。 */

  jsmn_init(&p);
  r = jsmn_parse(&p, JSON_STRING, strlen(JSON_STRING), t,
                 sizeof(t) / sizeof(t[0]));
  if (r < 0) {
    printf("JSON 解析失败：%d\n", r);
    return 1;
  }

  /* 本示例约定顶层元素必须是对象。 */
  if (r < 1 || t[0].type != JSMN_OBJECT) {
    printf("需要 JSON 对象。\n");
    return 1;
  }

  /* 遍历根对象中的键和值；对象键和值各占一个连续标记。 */
  for (i = 1; i < r; i++) {
    if (jsoneq(JSON_STRING, &t[i], "user") == 0) {
      /* 需要复制字符串时，可以根据标记边界自行实现 strndup()。 */
      printf("- 用户：%.*s\n", t[i + 1].end - t[i + 1].start,
             JSON_STRING + t[i + 1].start);
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "admin") == 0) {
      /* 还可以继续检查值是否严格等于 true 或 false。 */
      printf("- 管理员：%.*s\n", t[i + 1].end - t[i + 1].start,
             JSON_STRING + t[i + 1].start);
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "uid") == 0) {
      /* 需要数值时，可以在这里调用 strtol() 转换文本。 */
      printf("- 用户编号：%.*s\n", t[i + 1].end - t[i + 1].start,
             JSON_STRING + t[i + 1].start);
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "groups") == 0) {
      int j;
      printf("- 用户组：\n");
      if (t[i + 1].type != JSMN_ARRAY) {
        continue; /* 本示例预期 groups 是字符串数组。 */
      }
      for (j = 0; j < t[i + 1].size; j++) {
        jsmntok_t *g = &t[i + j + 2];
        printf("  * %.*s\n", g->end - g->start, JSON_STRING + g->start);
      }
      i += t[i + 1].size + 1;
    } else {
      printf("未处理的键：%.*s\n", t[i].end - t[i].start,
             JSON_STRING + t[i].start);
    }
  }
  return EXIT_SUCCESS;
}
