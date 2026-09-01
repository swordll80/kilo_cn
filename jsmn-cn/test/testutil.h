#ifndef __TEST_UTIL_H__
#define __TEST_UTIL_H__

#include "../jsmn.h"

/* 按可变参数描述逐项核对标记的类型、边界、子项数量和文本内容。 */
static int vtokeq(const char *s, jsmntok_t *t, unsigned long numtok,
                  va_list ap) {
  if (numtok > 0) {
    unsigned long i;
    int start, end, size;
    jsmntype_t type;
    char *value;

    size = -1;
    value = NULL;
    for (i = 0; i < numtok; i++) {
      type = va_arg(ap, jsmntype_t);
      if (type == JSMN_STRING) {
        value = va_arg(ap, char *);
        size = va_arg(ap, int);
        start = end = -1;
      } else if (type == JSMN_PRIMITIVE) {
        value = va_arg(ap, char *);
        start = end = size = -1;
      } else {
        start = va_arg(ap, int);
        end = va_arg(ap, int);
        size = va_arg(ap, int);
        value = NULL;
      }
      if (t[i].type != type) {
        printf("标记 %lu 的类型为 %d，应为 %d。\n", i, t[i].type, type);
        return 0;
      }
      if (start != -1 && end != -1) {
        if (t[i].start != start) {
          printf("标记 %lu 的起始位置为 %d，应为 %d。\n", i, t[i].start,
                 start);
          return 0;
        }
        if (t[i].end != end) {
          printf("标记 %lu 的结束位置为 %d，应为 %d。\n", i, t[i].end, end);
          return 0;
        }
      }
      if (size != -1 && t[i].size != size) {
        printf("标记 %lu 的子项数量为 %d，应为 %d。\n", i, t[i].size, size);
        return 0;
      }

      if (s != NULL && value != NULL) {
        const char *p = s + t[i].start;
        if (strlen(value) != (unsigned long)(t[i].end - t[i].start) ||
            strncmp(p, value, t[i].end - t[i].start) != 0) {
          printf("标记 %lu 的值为 %.*s，应为 %s。\n", i,
                 t[i].end - t[i].start, s + t[i].start, value);
          return 0;
        }
      }
    }
  }
  return 1;
}

/* 可变参数版本的标记比较入口。 */
static int tokeq(const char *s, jsmntok_t *tokens, unsigned long numtok, ...) {
  int ok;
  va_list args;
  va_start(args, numtok);
  ok = vtokeq(s, tokens, numtok, args);
  va_end(args);
  return ok;
}

/* 解析字符串并核对返回状态及标记内容。 */
static int parse(const char *s, int status, unsigned long numtok, ...) {
  int r;
  int ok = 1;
  va_list args;
  jsmn_parser p;
  jsmntok_t *t = malloc(numtok * sizeof(jsmntok_t));

  jsmn_init(&p);
  r = jsmn_parse(&p, s, strlen(s), t, numtok);
  if (r != status) {
    printf("解析状态为 %d，应为 %d。\n", r, status);
    return 0;
  }

  if (status >= 0) {
    va_start(args, numtok);
    ok = vtokeq(s, t, numtok, args);
    va_end(args);
  }
  free(t);
  return ok;
}

#endif /* __TEST_UTIL_H__ */
