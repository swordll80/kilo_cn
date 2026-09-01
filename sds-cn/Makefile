CC ?= cc
CFLAGS ?= -Wall -std=c99 -pedantic -O2

all: sds-test

sds-test: sds.c sds.h testhelp.h sdsalloc.h
	$(CC) $(CFLAGS) -o sds-test sds.c -DSDS_TEST_MAIN
	@echo ">>> 运行 ./sds-test 执行 sds.c 单元测试。"

test: sds-test
	./sds-test

clean:
	$(RM) sds-test
