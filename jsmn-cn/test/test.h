#ifndef __TEST_H__
#define __TEST_H__

static int test_passed = 0;
static int test_failed = 0;

/* 当前测试失败：用宏展开后的行号标记失败位置。 */
#define fail() return __LINE__

/* 当前测试成功结束。 */
#define done() return 0

/* 检查单个条件，失败时立即结束当前测试。 */
#define check(cond)                                                            \
  do {                                                                         \
    if (!(cond))                                                               \
      fail();                                                                  \
  } while (0)

/* 运行一个测试函数并累计通过/失败数量。 */
static void test(int (*func)(void), const char *name) {
  int r = func();
  if (r == 0) {
    test_passed++;
  } else {
    test_failed++;
    printf("失败：%s（第 %d 行）\n", name, r);
  }
}

#endif /* __TEST_H__ */
