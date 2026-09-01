#include "../jsmn.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* realloc_it() 是标准 realloc() 的薄包装。
 *
 * 与标准 realloc() 的区别是：重新分配失败时，它会释放旧内存。因此调用
 * realloc_it() 后绝对不能再使用原指针。如果调用方需要在内存不足时执行
 * 备用算法，应直接使用标准 realloc()。
 */
static inline void *realloc_it(void *ptrmem, size_t size) {
  void *p = realloc(ptrmem, size);
  if (!p) {
    free(ptrmem);
    fprintf(stderr, "realloc() 失败：errno=%d\n", errno);
  }
  return p;
}

/*
 * 从标准输入读取 JSON，并将解析结果打印到标准输出的示例。
 * 输出格式类似 YAML，但不承诺与 YAML 完全兼容。
 */

static int dump(const char *js, jsmntok_t *t, size_t count, int indent) {
  /* 递归遍历以当前标记开头的连续标记，并返回实际消费的数量。 */
  int i, j, k;
  jsmntok_t *key;
  if (count == 0) {
    return 0;
  }
  if (t->type == JSMN_PRIMITIVE) {
    printf("%.*s", t->end - t->start, js + t->start);
    return 1;
  } else if (t->type == JSMN_STRING) {
    printf("'%.*s'", t->end - t->start, js + t->start);
    return 1;
  } else if (t->type == JSMN_OBJECT) {
    printf("\n");
    j = 0;
    for (i = 0; i < t->size; i++) {
      for (k = 0; k < indent; k++) {
        printf("  ");
      }
      key = t + 1 + j;
      j += dump(js, key, count - j, indent + 1);
      if (key->size > 0) {
        printf(": ");
        j += dump(js, t + 1 + j, count - j, indent + 1);
      }
      printf("\n");
    }
    return j + 1;
  } else if (t->type == JSMN_ARRAY) {
    j = 0;
    printf("\n");
    for (i = 0; i < t->size; i++) {
      for (k = 0; k < indent - 1; k++) {
        printf("  ");
      }
      printf("   - ");
      j += dump(js, t + 1 + j, count - j, indent + 1);
      printf("\n");
    }
    return j + 1;
  }
  return 0;
}

int main() {
  int r;
  int eof_expected = 0;
  char *js = NULL;
  size_t jslen = 0;
  char buf[BUFSIZ];

  jsmn_parser p;
  jsmntok_t *tok;
  unsigned int tokcount = 2;

  /* 初始化解析器状态。 */
  jsmn_init(&p);

  /* 先分配一小批标记；遇到 JSMN_ERROR_NOMEM 时再逐步扩容。 */
  tok = malloc(sizeof(*tok) * tokcount);
  if (tok == NULL) {
    fprintf(stderr, "malloc() 失败：errno=%d\n", errno);
    return 3;
  }

  for (;;) {
    /* 读取下一块输入。 */
    r = (int)fread(buf, 1, sizeof(buf), stdin);
    if (ferror(stdin)) {
      fprintf(stderr, "fread() 失败：返回值=%d，errno=%d\n", r, errno);
      return 1;
    }
    if (r == 0) {
      if (eof_expected != 0) {
        return 0;
      } else {
        fprintf(stderr, "fread() 失败：意外遇到文件结束。\n");
        return 2;
      }
    }

    js = realloc_it(js, jslen + r + 1);
    if (js == NULL) {
      return 3;
    }
    strncpy(js + jslen, buf, r);
    jslen = jslen + r;

  again:
    r = jsmn_parse(&p, js, jslen, tok, tokcount);
    if (r < 0) {
      if (r == JSMN_ERROR_NOMEM) {
        tokcount = tokcount * 2;
        tok = realloc_it(tok, sizeof(*tok) * tokcount);
        if (tok == NULL) {
          return 3;
        }
        goto again;
      }
    } else {
      dump(js, tok, p.toknext, 0);
      eof_expected = 1;
    }
  }

}
