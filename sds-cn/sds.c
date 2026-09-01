/* SDSLib 2.0 —— C 动态字符串库
 *
 * Copyright (c) 2006-2015, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2015, Oran Agra
 * Copyright (c) 2015, Redis Labs, Inc
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "sds.h"
#include "sdsalloc.h"

#if defined(SDS_TEST_MAIN) && defined(_WIN32)
#include <windows.h>
#endif

#if defined(_MSC_VER) && !defined(va_copy)
/* MSVC 的 va_list 可以直接复制；为 C99 接口补充 va_copy 兼容宏。 */
#define va_copy(destination, source) ((destination) = (source))
#endif

const char *SDS_NOINIT = "SDS_NOINIT";

static inline int sdsHdrSize(char type) {
    switch(type&SDS_TYPE_MASK) {
        case SDS_TYPE_5:
            return sizeof(struct sdshdr5);
        case SDS_TYPE_8:
            return sizeof(struct sdshdr8);
        case SDS_TYPE_16:
            return sizeof(struct sdshdr16);
        case SDS_TYPE_32:
            return sizeof(struct sdshdr32);
        case SDS_TYPE_64:
            return sizeof(struct sdshdr64);
    }
    return 0;
}

static inline char sdsReqType(size_t string_size) {
    if (string_size < 1<<5)
        return SDS_TYPE_5;
    if (string_size < 1<<8)
        return SDS_TYPE_8;
    if (string_size < 1<<16)
        return SDS_TYPE_16;
#if (LONG_MAX == LLONG_MAX)
    if (string_size < 1ll<<32)
        return SDS_TYPE_32;
    return SDS_TYPE_64;
#else
    return SDS_TYPE_32;
#endif
}

/* 按 init/initlen 创建 SDS 字符串。
 * init 为 NULL 时将缓冲区清零；使用 SDS_NOINIT 时不初始化缓冲区。
 * SDS 字符串始终以空字符结尾，但长度单独保存在头部，因此允许内容中间
 * 包含空字符，仍然是二进制安全的。 */
sds sdsnewlen(const void *init, size_t initlen) {
    void *sh;
    sds s;
    char type = sdsReqType(initlen);
    /* 空字符串通常用于后续追加，因此使用类型 8；类型 5 不适合此场景。 */
    if (type == SDS_TYPE_5 && initlen == 0) type = SDS_TYPE_8;
    int hdrlen = sdsHdrSize(type);
    unsigned char *fp; /* 指向 flags 字节的指针。 */

    sh = s_malloc(hdrlen+initlen+1);
    if (sh == NULL) return NULL;
    if (init==SDS_NOINIT)
        init = NULL;
    else if (!init)
        memset(sh, 0, hdrlen+initlen+1);
    s = (char*)sh+hdrlen;
    fp = ((unsigned char*)s)-1;
    switch(type) {
        case SDS_TYPE_5: {
            *fp = type | (initlen << SDS_TYPE_BITS);
            break;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            sh->len = initlen;
            sh->alloc = initlen;
            *fp = type;
            break;
        }
    }
    if (initlen && init)
        memcpy(s, init, initlen);
    s[initlen] = '\0';
    return s;
}

/* 创建空的（长度为零）SDS 字符串；即使为空，也会保留隐含的结尾空字符。 */
sds sdsempty(void) {
    return sdsnewlen("",0);
}

/* 从以空字符结尾的 C 字符串创建新的 SDS 字符串。 */
sds sdsnew(const char *init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/* 复制一个 SDS 字符串。 */
sds sdsdup(const sds s) {
    return sdsnewlen(s, sdslen(s));
}

/* 释放 SDS 字符串；s 为 NULL 时不执行任何操作。 */
void sdsfree(sds s) {
    if (s == NULL) return;
    s_free((char*)s-sdsHdrSize(s[-1]));
}

/* 根据 strlen() 重新设置 SDS 的逻辑长度，因此只把第一个空字符之前的
 * 内容视为字符串。适用于调用方直接修改缓冲区后同步内部长度的场景：
 *
 * s = sdsnew("foobar");
 * s[2] = '\0';
 * sdsupdatelen(s);
 * printf("%d\n", sdslen(s));
 *
 * 输出为 "2"；如果注释掉 sdsupdatelen()，虽然字符串内容已修改，逻辑长度
 * 仍会保持为 6，因此输出为 "6"。 */
void sdsupdatelen(sds s) {
    size_t reallen = strlen(s);
    sdssetlen(s, reallen);
}

/* 原地清空 SDS 字符串，但不释放已有缓冲区；已有空间会变成可用空间，
 * 后续追加可以复用它而无需立即重新分配。 */
void sdsclear(sds s) {
    sdssetlen(s, 0);
    s[0] = '\0';
}

/* 扩大字符串末尾的可用空间，使调用方可以在当前末尾后写入 addlen 个字节，
 * 另加一个结尾空字符。此函数只改变容量，不改变 sdslen() 返回的逻辑长度。 */
sds sdsMakeRoomFor(sds s, size_t addlen) {
    void *sh, *newsh;
    size_t avail = sdsavail(s);
    size_t len, newlen, reqlen;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen;

    /* 剩余空间足够时立即返回。 */
    if (avail >= addlen) return s;

    len = sdslen(s);
    sh = (char*)s-sdsHdrSize(oldtype);
    reqlen = newlen = (len+addlen);
    if (newlen < SDS_MAX_PREALLOC)
        newlen *= 2;
    else
        newlen += SDS_MAX_PREALLOC;

    type = sdsReqType(newlen);

    /* 不使用类型 5：它不能记录剩余空间，追加时会导致每次都重新调用
     * sdsMakeRoomFor()。 */
    if (type == SDS_TYPE_5) type = SDS_TYPE_8;

    hdrlen = sdsHdrSize(type);
    assert(hdrlen + newlen + 1 > reqlen); /* 捕获 size_t 溢出。 */
    if (oldtype==type) {
        newsh = s_realloc(sh, hdrlen+newlen+1);
        if (newsh == NULL) return NULL;
        s = (char*)newsh+hdrlen;
    } else {
        /* 头部大小发生变化，需要移动字符串位置，因此不能直接使用 realloc。 */
        newsh = s_malloc(hdrlen+newlen+1);
        if (newsh == NULL) return NULL;
        memcpy((char*)newsh+hdrlen, s, len+1);
        s_free(sh);
        s = (char*)newsh+hdrlen;
        s[-1] = type;
        sdssetlen(s, len);
    }
    sdssetalloc(s, newlen);
    return s;
}

/* 重新分配 SDS，使末尾不再保留空闲空间；字符串内容不变，但下一次追加
 * 可能需要重新分配。调用后旧指针失效，所有引用都必须替换为返回值。 */
sds sdsRemoveFreeSpace(sds s) {
    void *sh, *newsh;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen, oldhdrlen = sdsHdrSize(oldtype);
    size_t len = sdslen(s);
    size_t avail = sdsavail(s);
    sh = (char*)s-oldhdrlen;

    /* 没有剩余空间时立即返回。 */
    if (avail == 0) return s;

    /* 计算刚好容纳当前字符串所需的最小 SDS 头部类型。 */
    type = sdsReqType(len);
    hdrlen = sdsHdrSize(type);

    /* 类型不变，或仍需要较大的头部类型时直接 realloc，让分配器决定是否
     * 复制；如果头部类型明显变小，则手动分配并移动字符串。 */
    if (oldtype==type || type > SDS_TYPE_8) {
        newsh = s_realloc(sh, oldhdrlen+len+1);
        if (newsh == NULL) return NULL;
        s = (char*)newsh+oldhdrlen;
    } else {
        newsh = s_malloc(hdrlen+len+1);
        if (newsh == NULL) return NULL;
        memcpy((char*)newsh+hdrlen, s, len+1);
        s_free(sh);
        s = (char*)newsh+hdrlen;
        s[-1] = type;
        sdssetlen(s, len);
    }
    sdssetalloc(s, len);
    return s;
}

/* 返回 SDS 的完整分配大小，包括指针前的头部、字符串、末尾剩余空间以及
 * 隐含的结尾空字符。 */
size_t sdsAllocSize(sds s) {
    size_t alloc = sdsalloc(s);
    return sdsHdrSize(s[-1])+alloc+1;
}

/* 返回实际分配块的起始地址；通常 SDS 对外只暴露字符串缓冲区起始地址。 */
void *sdsAllocPtr(sds s) {
    return (void*) (s-sdsHdrSize(s[-1]));
}

/* 按 incr 调整 SDS 的逻辑长度和末尾剩余空间，并在新末尾写入空字符。
 * 同时根据 incr 更新末尾剩余空间，并在新的字符串末尾写入空字符。
 *
 * 典型用法是先用 sdsMakeRoomFor() 取得空间，直接写入缓冲区，再用本函数
 * 提交新长度。
 * incr 可以为负数，用于从右侧截短字符串；原有的 C API 语义不变。
 * 使用示例：
 *
 * 使用 sdsIncrLen() 和 sdsMakeRoomFor()，可以把内核读取的字节直接追加到
 * SDS 末尾，不需要经过中间缓冲区复制：
 *
 * oldlen = sdslen(s);
 * s = sdsMakeRoomFor(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * sdsIncrLen(s, nread);
 */
void sdsIncrLen(sds s, ssize_t incr) {
    unsigned char flags = s[-1];
    size_t len;
    switch(flags&SDS_TYPE_MASK) {
        case SDS_TYPE_5: {
            unsigned char *fp = ((unsigned char*)s)-1;
            unsigned char oldlen = SDS_TYPE_5_LEN(flags);
            assert((incr > 0 && oldlen+incr < 32) || (incr < 0 && oldlen >= (unsigned int)(-incr)));
            *fp = SDS_TYPE_5 | ((oldlen+incr) << SDS_TYPE_BITS);
            len = oldlen+incr;
            break;
        }
        case SDS_TYPE_8: {
            SDS_HDR_VAR(8,s);
            assert((incr >= 0 && sh->alloc-sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_16: {
            SDS_HDR_VAR(16,s);
            assert((incr >= 0 && sh->alloc-sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_32: {
            SDS_HDR_VAR(32,s);
            assert((incr >= 0 && sh->alloc-sh->len >= (unsigned int)incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
            len = (sh->len += incr);
            break;
        }
        case SDS_TYPE_64: {
            SDS_HDR_VAR(64,s);
            assert((incr >= 0 && sh->alloc-sh->len >= (uint64_t)incr) || (incr < 0 && sh->len >= (uint64_t)(-incr)));
            len = (sh->len += incr);
            break;
        }
        default: len = 0; /* 仅为避免编译器警告。 */
    }
    s[len] = '\0';
}

/* 将 SDS 扩展到指定长度；新增字节清零。如果目标长度不大于当前长度，
 * 则不执行任何操作。 */
sds sdsgrowzero(sds s, size_t len) {
    size_t curlen = sdslen(s);

    if (len <= curlen) return s;
    s = sdsMakeRoomFor(s,len-curlen);
    if (s == NULL) return NULL;

    /* 确保新增区域不含未初始化数据。 */
    memset(s+curlen,0,(len-curlen+1)); /* 同时设置末尾的 \0。 */
    sdssetlen(s, len);
    return s;
}

/* 将 t 指向的、长度为 len 的二进制安全数据追加到 s 末尾。
 * 调用后旧的 s 指针可能失效，必须使用返回的指针。 */
sds sdscatlen(sds s, const void *t, size_t len) {
    size_t curlen = sdslen(s);

    s = sdsMakeRoomFor(s,len);
    if (s == NULL) return NULL;
    memcpy(s+curlen, t, len);
    sdssetlen(s, curlen+len);
    s[curlen+len] = '\0';
    return s;
}

/* 将以空字符结尾的 C 字符串追加到 s；调用后应使用返回的指针。 */
sds sdscat(sds s, const char *t) {
    return sdscatlen(s, t, strlen(t));
}

/* 将 SDS t 追加到 SDS s；调用后旧的 s 指针可能失效。 */
sds sdscatsds(sds s, const sds t) {
    return sdscatlen(s, t, sdslen(t));
}

/* 原地修改 SDS s，使其保存 t 指向的、长度为 len 的二进制安全数据。 */
sds sdscpylen(sds s, const char *t, size_t len) {
    if (sdsalloc(s) < len) {
        s = sdsMakeRoomFor(s,len-sdslen(s));
        if (s == NULL) return NULL;
    }
    memcpy(s, t, len);
    s[len] = '\0';
    sdssetlen(s, len);
    return s;
}

/* sdscpylen() 的 C 字符串版本；t 必须以空字符结尾，长度由 strlen() 获取。 */
sds sdscpy(sds s, const char *t) {
    return sdscpylen(s, t, strlen(t));
}

/* 数字转字符串的内部辅助函数；s 至少需要 SDS_LLSTR_SIZE 字节空间。
 * 返回写入 s 的、以空字符结尾的字符串长度。 */
#define SDS_LLSTR_SIZE 21
int sdsll2str(char *s, long long value) {
    char *p, aux;
    unsigned long long v;
    size_t l;

    /* 先生成逆序的数字字符串。 */
    if (value < 0) {
        /* v 是无符号数；value 为 LLONG_MIN 时，直接计算 -LLONG_MIN 会溢出。 */
        if (value != LLONG_MIN) {
            v = -value;
        } else {
            v = ((unsigned long long)LLONG_MAX) + 1;
        }
    } else {
        v = value;
    }

    p = s;
    do {
        *p++ = '0'+(v%10);
        v /= 10;
    } while(v);
    if (value < 0) *p++ = '-';

    /* 计算长度并添加结尾空字符。 */
    l = p-s;
    *p = '\0';

    /* 将字符串反转。 */
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* 与 sdsll2str() 相同，但处理 unsigned long long。 */
int sdsull2str(char *s, unsigned long long v) {
    char *p, aux;
    size_t l;

    /* 先生成逆序的数字字符串。 */
    p = s;
    do {
        *p++ = '0'+(v%10);
        v /= 10;
    } while(v);

    /* 计算长度并添加结尾空字符。 */
    l = p-s;
    *p = '\0';

    /* 将字符串反转。 */
    p--;
    while(s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* 从 long long 创建 SDS 字符串，比下面的写法更快：
 *
 * sdscatprintf(sdsempty(),"%lld\n", value);
 */
sds sdsfromlonglong(long long value) {
    char buf[SDS_LLSTR_SIZE];
    int len = sdsll2str(buf,value);

    return sdsnewlen(buf,len);
}

/* sdscatprintf() 的 va_list 版本，不使用可变参数声明。 */
sds sdscatvprintf(sds s, const char *fmt, va_list ap) {
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    size_t buflen = strlen(fmt)*2;
    int bufstrlen;

    /* 优先使用栈上的静态缓冲区提升速度；空间不足时回退到堆分配。 */
    if (buflen > sizeof(staticbuf)) {
        buf = s_malloc(buflen);
        if (buf == NULL) return NULL;
    } else {
        buflen = sizeof(staticbuf);
    }

    /* 如果当前缓冲区放不下结果，按实际需要重新分配，并为结尾空字符
     * 预留空间。 */
    while(1) {
        va_copy(cpy,ap);
#if defined(_MSC_VER)
        /* MSVC 兼容路径先计算完整结果长度，避免旧版 vsnprintf 在缓冲区
         * 不足时返回 -1，进而误判为格式化失败。 */
        bufstrlen = _vscprintf(fmt, cpy);
        va_end(cpy);
        if (bufstrlen < 0) {
            if (buf != staticbuf) s_free(buf);
            return NULL;
        }
        if (((size_t)bufstrlen) >= buflen) {
            if (buf != staticbuf) s_free(buf);
            buflen = ((size_t)bufstrlen) + 1;
            buf = s_malloc(buflen);
            if (buf == NULL) return NULL;
            continue;
        }
        va_copy(cpy,ap);
        _vsnprintf_s(buf, buflen, _TRUNCATE, fmt, cpy);
        va_end(cpy);
#else
        bufstrlen = vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if (bufstrlen < 0) {
            if (buf != staticbuf) s_free(buf);
            return NULL;
        }
        if (((size_t)bufstrlen) >= buflen) {
            if (buf != staticbuf) s_free(buf);
            buflen = ((size_t)bufstrlen) + 1;
            buf = s_malloc(buflen);
            if (buf == NULL) return NULL;
            continue;
        }
#endif
        break;
    }

    /* 将格式化结果追加到 SDS 并返回新的字符串指针。 */
    t = sdscatlen(s, buf, bufstrlen);
    if (buf != staticbuf) s_free(buf);
    return t;
}

/* 按 printf 风格的格式说明符生成字符串并追加到 s。
 *
 * 调用后旧的 s 指针可能失效，所有引用都必须替换为返回值。
 *
 * 示例：
 *
 * s = sdsnew("Sum is: ");
 * s = sdscatprintf(s,"%d+%d = %d",a,b,a+b).
 *
 * 如果需要从头创建格式化字符串，可把 sdsempty() 作为目标字符串：
 *
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
 */
sds sdscatprintf(sds s, const char *fmt, ...) {
    va_list ap;
    char *t;
    va_start(ap, fmt);
    t = sdscatvprintf(s,fmt,ap);
    va_end(ap);
    return t;
}

/* 此函数与 sdscatprintf() 类似，但不依赖 libc 中通常较慢的 sprintf() 系列
 * 函数，因此速度更快；直接写入可追加的 SDS 空间也能减少开销。
 *
 * 仅支持下列与 printf 不完全兼容的格式说明符：
 *
 * %s - C 字符串
 * %S - SDS 字符串
 * %i - 有符号 int
 * %I - 64 位有符号整数（long long、int64_t）
 * %u - 无符号 int
 * %U - 64 位无符号整数（unsigned long long、uint64_t）
 * %% - 原样输出“%”字符。
 */
sds sdscatfmt(sds s, char const *fmt, ...) {
    size_t initlen = sdslen(s);
    const char *f = fmt;
    long i;
    va_list ap;

    /* 为避免连续重新分配，初始容量至少按格式字符串长度的两倍申请；
     * 这是经验性策略，但在实际使用中效果良好。 */
    s = sdsMakeRoomFor(s, initlen + strlen(fmt)*2);
    va_start(ap,fmt);
    f = fmt;    /* 下一个待处理的格式字节。 */
    i = initlen; /* 目标字符串下一个写入位置。 */
    while(*f) {
        char next, *str;
        size_t l;
        long long num;
        unsigned long long unum;

        /* 确保始终至少有一个字节的可写空间。 */
        if (sdsavail(s)==0) {
            s = sdsMakeRoomFor(s,1);
        }

        switch(*f) {
        case '%':
            next = *(f+1);
            if (next == '\0') break;
            f++;
            switch(next) {
            case 's':
            case 'S':
                str = va_arg(ap,char*);
                l = (next == 's') ? strlen(str) : sdslen(str);
                if (sdsavail(s) < l) {
                    s = sdsMakeRoomFor(s,l);
                }
                memcpy(s+i,str,l);
                sdsinclen(s,l);
                i += l;
                break;
            case 'i':
            case 'I':
                if (next == 'i')
                    num = va_arg(ap,int);
                else
                    num = va_arg(ap,long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsll2str(buf,num);
                    if (sdsavail(s) < l) {
                        s = sdsMakeRoomFor(s,l);
                    }
                    memcpy(s+i,buf,l);
                    sdsinclen(s,l);
                    i += l;
                }
                break;
            case 'u':
            case 'U':
                if (next == 'u')
                    unum = va_arg(ap,unsigned int);
                else
                    unum = va_arg(ap,unsigned long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsull2str(buf,unum);
                    if (sdsavail(s) < l) {
                        s = sdsMakeRoomFor(s,l);
                    }
                    memcpy(s+i,buf,l);
                    sdsinclen(s,l);
                    i += l;
                }
                break;
            default: /* 处理 %% 以及一般的未知 %<字符>。 */
                s[i++] = next;
                sdsinclen(s,1);
                break;
            }
            break;
        default:
            s[i++] = *f;
            sdsinclen(s,1);
            break;
        }
        f++;
    }
    va_end(ap);

    /* 写入结尾空字符。 */
    s[i] = '\0';
    return s;
}

/* 从字符串左右两端删除连续出现在 cset 中的字符；cset 是以空字符结尾的
 * C 字符串。
 *
 * 此函数原地修改字符串并返回同一个指针，调用方仍应使用返回值保持接口
 * 使用习惯一致。
 *
 * 示例：
 *
 * s = sdsnew("AA...AA.a.aa.aHelloWorld     :::");
 * s = sdstrim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * 输出结果为 "HelloWorld"。
 */
sds sdstrim(sds s, const char *cset) {
    char *end, *sp, *ep;
    size_t len;

    sp = s;
    ep = end = s+sdslen(s)-1;
    while(sp <= end && strchr(cset, *sp)) sp++;
    while(ep > sp && strchr(cset, *ep)) ep--;
    len = (ep-sp)+1;
    if (s != sp) memmove(s, sp, len);
    s[len] = '\0';
    sdssetlen(s,len);
    return s;
}

/* 将字符串截取为 start 到 end 指定的子串，结果长度可以小于或等于原长度。
 *
 * start/end 可以为负数：-1 表示最后一个字节，-2 表示倒数第二个字节，依此类推。
 *
 * 区间两端均包含在结果中；函数会原地修改字符串。
 *
 * 示例：
 *
 * s = sdsnew("Hello World");
 * sdsrange(s,1,-1); => "ello World"
 */
void sdsrange(sds s, ssize_t start, ssize_t end) {
    size_t newlen, len = sdslen(s);

    if (len == 0) return;
    if (start < 0) {
        start = len+start;
        if (start < 0) start = 0;
    }
    if (end < 0) {
        end = len+end;
        if (end < 0) end = 0;
    }
    newlen = (start > end) ? 0 : (end-start)+1;
    if (newlen != 0) {
        if (start >= (ssize_t)len) {
            newlen = 0;
        } else if (end >= (ssize_t)len) {
            end = len-1;
            newlen = (end-start)+1;
        }
    }
    if (start && newlen) memmove(s, s+start, newlen);
    s[newlen] = 0;
    sdssetlen(s,newlen);
}

/* 对 SDS s 的每个字节调用 tolower()。 */
void sdstolower(sds s) {
    size_t len = sdslen(s), j;

    for (j = 0; j < len; j++) s[j] = tolower(s[j]);
}

/* 对 SDS s 的每个字节调用 toupper()。 */
void sdstoupper(sds s) {
    size_t len = sdslen(s), j;

    for (j = 0; j < len; j++) s[j] = toupper(s[j]);
}

/* 使用 memcmp() 比较两个 SDS 字符串 s1 和 s2。
 *
 * 返回值：
 *
 *     s1 > s2 时为正数；s1 < s2 时为负数；二进制内容完全相同时为 0。
 *
 * 如果两个字符串具有相同前缀但长度不同，则较长者更大。 */
int sdscmp(const sds s1, const sds s2) {
    size_t l1, l2, minlen;
    int cmp;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1,s2,minlen);
    if (cmp == 0) return l1>l2? 1: (l1<l2? -1: 0);
    return cmp;
}

/* 使用 sep 分隔 s，返回 SDS 字符串数组，并通过 count 返回元素数量。
 *
 * 内存不足、输入长度为零或分隔符长度为零时返回 NULL。
 *
 * sep 可以是多字节分隔符，例如：
 * sdssplit("foo_-_bar","_-_"); 会返回 "foo" 和 "bar" 两个元素。
 *
 * 此版本是二进制安全的，但要求显式长度参数；对空字符结尾字符串使用
 * 相同语义的封装接口即可。
 */
sds *sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count) {
    int elements = 0, slots = 5;
    long start = 0, j;
    sds *tokens;

    if (seplen < 1 || len <= 0) {
        *count = 0;
        return NULL;
    }

    tokens = s_malloc(sizeof(sds)*slots);
    if (tokens == NULL) return NULL;

    for (j = 0; j < (len-(seplen-1)); j++) {
        /* 确保数组能容纳下一个元素以及最后一个元素。 */
        if (slots < elements+2) {
            sds *newtokens;

            slots *= 2;
            newtokens = s_realloc(tokens,sizeof(sds)*slots);
            if (newtokens == NULL) goto cleanup;
            tokens = newtokens;
        }
        /* 查找分隔符。 */
        if ((seplen == 1 && *(s+j) == sep[0]) || (memcmp(s+j,sep,seplen) == 0)) {
            tokens[elements] = sdsnewlen(s+start,j-start);
            if (tokens[elements] == NULL) goto cleanup;
            elements++;
            start = j+seplen;
            j = j+seplen-1; /* 跳过分隔符。 */
        }
    }
    /* 添加最后一个元素；数组中已经预留了空间。 */
    tokens[elements] = sdsnewlen(s+start,len-start);
    if (tokens[elements] == NULL) goto cleanup;
    elements++;
    *count = elements;
    return tokens;

cleanup:
    {
        int i;
        for (i = 0; i < elements; i++) sdsfree(tokens[i]);
        s_free(tokens);
        *count = 0;
        return NULL;
    }
}

/* 释放 sdssplitlen() 返回的结果；tokens 为 NULL 时不执行任何操作。 */
void sdsfreesplitres(sds *tokens, int count) {
    if (!tokens) return;
    while(count--)
        sdsfree(tokens[count]);
    s_free(tokens);
}

/* 将输入数据的转义表示追加到 SDS s。不可打印字符（由 isprint() 判断）会
 * 转换为 "\n\r\a..." 或 "\x<十六进制数>" 形式。
 *
 * 调用后旧的 s 指针可能失效，必须使用返回值。 */
sds sdscatrepr(sds s, const char *p, size_t len) {
    s = sdscatlen(s,"\"",1);
    while(len--) {
        switch(*p) {
        case '\\':
        case '"':
            s = sdscatprintf(s,"\\%c",*p);
            break;
        case '\n': s = sdscatlen(s,"\\n",2); break;
        case '\r': s = sdscatlen(s,"\\r",2); break;
        case '\t': s = sdscatlen(s,"\\t",2); break;
        case '\a': s = sdscatlen(s,"\\a",2); break;
        case '\b': s = sdscatlen(s,"\\b",2); break;
        default:
            if (isprint(*p))
                s = sdscatprintf(s,"%c",*p);
            else
                s = sdscatprintf(s,"\\x%02x",(unsigned char)*p);
            break;
        }
        p++;
    }
    return sdscatlen(s,"\"",1);
}

/* sdssplitargs() 的辅助函数：判断 c 是否为有效的十六进制数字。 */
int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* sdssplitargs() 的辅助函数：将十六进制数字转换为 0 到 15 的整数。 */
int hex_digit_to_int(char c) {
    switch(c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return 0;
    }
}

/* 将一行文本解析为参数；每个参数可以使用类似编程语言 REPL 的形式：
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * 参数数量写入 *argc，并返回 SDS 数组。
 *
 * 调用方应使用 sdsfreesplitres() 释放返回的 SDS 数组。
 *
 * sdscatrepr() 生成的带引号字符串可以由 sdssplitargs() 按相同格式解析。
 *
 * 即使输入为空，成功时也会返回已分配的数组；引号不匹配，或结束引号后
 * 紧跟非空白字符（如 "foo"bar、"foo'）时返回 NULL。
 */
sds *sdssplitargs(const char *line, int *argc) {
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while(1) {
        /* 跳过空白字符。 */
        while(*p && isspace(*p)) p++;
        if (*p) {
            /* 读取一个参数。 */
            int inq=0;  /* 当前位于双引号内时为 1。 */
            int insq=0; /* 当前位于单引号内时为 1。 */
            int done=0;

            if (current == NULL) current = sdsempty();
            while(!done) {
                if (inq) {
                    if (*p == '\\' && *(p+1) == 'x' &&
                                             is_hex_digit(*(p+2)) &&
                                             is_hex_digit(*(p+3)))
                    {
                        unsigned char byte;

                        byte = (hex_digit_to_int(*(p+2))*16)+
                                hex_digit_to_int(*(p+3));
                        current = sdscatlen(current,(char*)&byte,1);
                        p += 3;
                    } else if (*p == '\\' && *(p+1)) {
                        char c;

                        p++;
                        switch(*p) {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case 't': c = '\t'; break;
                        case 'b': c = '\b'; break;
                        case 'a': c = '\a'; break;
                        default: c = *p; break;
                        }
                        current = sdscatlen(current,&c,1);
                    } else if (*p == '"') {
                        /* 结束引号后必须是空白或字符串结尾。 */
                        if (*(p+1) && !isspace(*(p+1))) goto err;
                        done=1;
                    } else if (!*p) {
                        /* 引号未闭合。 */
                        goto err;
                    } else {
                        current = sdscatlen(current,p,1);
                    }
                } else if (insq) {
                    if (*p == '\\' && *(p+1) == '\'') {
                        p++;
                        current = sdscatlen(current,"'",1);
                    } else if (*p == '\'') {
                        /* 结束引号后必须是空白或字符串结尾。 */
                        if (*(p+1) && !isspace(*(p+1))) goto err;
                        done=1;
                    } else if (!*p) {
                        /* 引号未闭合。 */
                        goto err;
                    } else {
                        current = sdscatlen(current,p,1);
                    }
                } else {
                    switch(*p) {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done=1;
                        break;
                    case '"':
                        inq=1;
                        break;
                    case '\'':
                        insq=1;
                        break;
                    default:
                        current = sdscatlen(current,p,1);
                        break;
                    }
                }
                if (*p) p++;
            }
            /* 将参数加入数组。 */
            vector = s_realloc(vector,((*argc)+1)*sizeof(char*));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        } else {
            /* 即使输入为空，也返回非 NULL 的数组。 */
            if (vector == NULL) vector = s_malloc(sizeof(void*));
            return vector;
        }
    }

err:
    while((*argc)--)
        sdsfree(vector[*argc]);
    s_free(vector);
    if (current) sdsfree(current);
    *argc = 0;
    return NULL;
}

/* 将 from 中出现的字符替换为 to 中相同下标的字符。
 *
 * 例如 sdsmapchars(mystring, "ho", "01", 2) 会把 "hello" 变为 "0ell1"。
 *
 * 此操作不需要重新分配，因此返回的指针始终与输入指针相同。 */
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen) {
    size_t j, i, l = sdslen(s);

    for (j = 0; j < l; j++) {
        for (i = 0; i < setlen; i++) {
            if (s[j] == from[i]) {
                s[j] = to[i];
                break;
            }
        }
    }
    return s;
}

/* 使用指定的 C 字符串分隔符连接 C 字符串数组，并返回 SDS 字符串。 */
sds sdsjoin(char **argv, int argc, char *sep) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscat(join, argv[j]);
        if (j != argc-1) join = sdscat(join,sep);
    }
    return join;
}

/* sdsjoin() 的 SDS 数组版本。 */
sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen) {
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++) {
        join = sdscatsds(join, argv[j]);
        if (j != argc-1) join = sdscatlen(join,sep,seplen);
    }
    return join;
}

/* SDS 分配器的导出包装。SDS 内部直接使用 sdsalloc.h 中的宏，以避免函数调用
 * 开销；这里的函数仅供链接 SDS 的程序在需要访问内部内存时使用，即使该程序
 * 使用的是另一套分配器。 */
void *sds_malloc(size_t size) { return s_malloc(size); }
void *sds_realloc(void *ptr, size_t size) { return s_realloc(ptr,size); }
void sds_free(void *ptr) { s_free(ptr); }

#if defined(SDS_TEST_MAIN)
#include <stdio.h>
#include "testhelp.h"
#include "limits.h"

#define UNUSED(x) (void)(x)
int sdsTest(void) {
    {
        sds x = sdsnew("foo"), y;

        test_cond("创建字符串并读取长度",
            sdslen(x) == 3 && memcmp(x,"foo\0",4) == 0)

        sdsfree(x);
        x = sdsnewlen("foo",2);
        test_cond("按指定长度创建字符串",
            sdslen(x) == 2 && memcmp(x,"fo\0",3) == 0)

        x = sdscat(x,"bar");
        test_cond("字符串追加",
            sdslen(x) == 5 && memcmp(x,"fobar\0",6) == 0);

        x = sdscpy(x,"a");
        test_cond("对原本更长的字符串执行 sdscpy()",
            sdslen(x) == 1 && memcmp(x,"a\0",2) == 0)

        x = sdscpy(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
        test_cond("对原本更短的字符串执行 sdscpy()",
            sdslen(x) == 33 &&
            memcmp(x,"xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0",33) == 0)

        sdsfree(x);
        x = sdscatprintf(sdsempty(),"%d",123);
        test_cond("sdscatprintf() 基本格式化",
            sdslen(x) == 3 && memcmp(x,"123\0",4) == 0)

        sdsfree(x);
        x = sdscatprintf(sdsempty(),"a%cb",0);
        test_cond("sdscatprintf() 结果中包含 \\0",
            sdslen(x) == 3 && memcmp(x,"a\0""b\0",4) == 0)

        {
            sdsfree(x);
            /* 1 MiB 基准数据放在静态存储区，避免 Windows 默认栈空间不足。 */
            static char etalon[1024*1024];
            for (size_t i = 0; i < sizeof(etalon); i++) {
                etalon[i] = '0';
            }
            x = sdscatprintf(sdsempty(),"%0*d",(int)sizeof(etalon),0);
            test_cond("sdscatprintf() 可以生成 1MB 内容",
                sdslen(x) == sizeof(etalon) && memcmp(x,etalon,sizeof(etalon)) == 0)
        }

        sdsfree(x);
        x = sdsnew("--");
        x = sdscatfmt(x, "Hello %s World %I,%I--", "Hi!", LLONG_MIN,LLONG_MAX);
        test_cond("sdscatfmt() 基本格式化",
            sdslen(x) == 60 &&
            memcmp(x,"--Hello Hi! World -9223372036854775808,"
                     "9223372036854775807--",60) == 0)
        printf("[%s]\n",x);

        sdsfree(x);
        x = sdsnew("--");
        x = sdscatfmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
        test_cond("sdscatfmt() 支持无符号数字",
            sdslen(x) == 35 &&
            memcmp(x,"--4294967295,18446744073709551615--",35) == 0)

        sdsfree(x);
        x = sdsnew(" x ");
        sdstrim(x," x");
        test_cond("sdstrim() 删除全部匹配字符",
            sdslen(x) == 0)

        sdsfree(x);
        x = sdsnew(" x ");
        sdstrim(x," ");
        test_cond("sdstrim() 保留单个字符",
            sdslen(x) == 1 && x[0] == 'x')

        sdsfree(x);
        x = sdsnew("xxciaoyyy");
        sdstrim(x,"xy");
        test_cond("sdstrim() 正确裁剪字符",
            sdslen(x) == 4 && memcmp(x,"ciao\0",5) == 0)

        y = sdsdup(x);
        sdsrange(y,1,1);
        test_cond("sdsrange(...,1,1)",
            sdslen(y) == 1 && memcmp(y,"i\0",2) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,1,-1);
        test_cond("sdsrange(...,1,-1)",
            sdslen(y) == 3 && memcmp(y,"iao\0",4) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,-2,-1);
        test_cond("sdsrange(...,-2,-1)",
            sdslen(y) == 2 && memcmp(y,"ao\0",3) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,2,1);
        test_cond("sdsrange(...,2,1)",
            sdslen(y) == 0 && memcmp(y,"\0",1) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,1,100);
        test_cond("sdsrange(...,1,100)",
            sdslen(y) == 3 && memcmp(y,"iao\0",4) == 0)

        sdsfree(y);
        y = sdsdup(x);
        sdsrange(y,100,100);
        test_cond("sdsrange(...,100,100)",
            sdslen(y) == 0 && memcmp(y,"\0",1) == 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("foo");
        y = sdsnew("foa");
        test_cond("sdscmp(foo,foa)", sdscmp(x,y) > 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("bar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar,bar)", sdscmp(x,y) == 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnew("aar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar,bar)", sdscmp(x,y) < 0)

        sdsfree(y);
        sdsfree(x);
        x = sdsnewlen("\a\n\0foo\r",7);
        y = sdscatrepr(sdsempty(),x,sdslen(x));
        test_cond("sdscatrepr() 转义二进制数据",
            memcmp(y,"\"\\a\\n\\x00foo\\r\"",15) == 0)

        {
            char *p;
            int step = 10, j, i;

            sdsfree(x);
            sdsfree(y);
            x = sdsnew("0");
            test_cond("sdsnew() 长度和剩余空间", sdslen(x) == 1 && sdsavail(x) == 0);

            /* 重复执行若干次，以覆盖前两种 SDS 头部类型。 */
            for (i = 0; i < 10; i++) {
                int oldlen = sdslen(x);
                x = sdsMakeRoomFor(x,step);
                int type = x[-1]&SDS_TYPE_MASK;

                test_cond("sdsMakeRoomFor() 保持长度", sdslen(x) == (size_t)oldlen);
                if (type != SDS_TYPE_5) {
                    test_cond("sdsMakeRoomFor() 扩大剩余空间", sdsavail(x) >= (size_t)step);
                }
                p = x+oldlen;
                for (j = 0; j < step; j++) {
                    p[j] = 'A'+j;
                }
                sdsIncrLen(x,step);
            }
            test_cond("sdsMakeRoomFor() 内容",
                memcmp("0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ",x,101) == 0);
            test_cond("sdsMakeRoomFor() 最终长度",sdslen(x)==101);

            sdsfree(x);
        }
    }
    test_report()
    return 0;
}
#endif

#ifdef SDS_TEST_MAIN
int main(void) {
#if defined(_WIN32)
    /* 测试输出使用 UTF-8；库本身不修改宿主程序的控制台代码页。 */
    SetConsoleOutputCP(CP_UTF8);
#endif
    return sdsTest();
}
#endif
