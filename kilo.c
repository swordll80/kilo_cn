/* Kilo -- 极简的终端文本编辑器。原始项目少于 1K 行；本版本增加了中文注释
 *         和 Windows 终端适配，但仍不依赖 libcurses，直接输出 VT100 序列。
 *
 * -----------------------------------------------------------------------
 *
 * Copyright (C) 2016 Salvatore Sanfilippo <antirez at gmail dot com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define KILO_VERSION "0.0.1"

#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>
#else
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#ifdef _WIN32
#ifndef STDIN_FILENO
#define STDIN_FILENO  _fileno(stdin)
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO _fileno(stdout)
#endif
#define write         _write
#define close         _close
#define isatty        _isatty
#endif

/* 语法高亮类型。数值同时作为高亮数组中的标记。 */
#define HL_NORMAL 0
#define HL_NONPRINT 1
#define HL_COMMENT 2   /* 单行注释。 */
#define HL_MLCOMMENT 3 /* 多行注释。 */
#define HL_KEYWORD1 4
#define HL_KEYWORD2 5
#define HL_STRING 6
#define HL_NUMBER 7
#define HL_MATCH 8      /* 搜索命中内容。 */

#define HL_HIGHLIGHT_STRINGS (1<<0)
#define HL_HIGHLIGHT_NUMBERS (1<<1)

struct editorSyntax {
    char **filematch;
    char **keywords;
    char singleline_comment_start[3];
    char multiline_comment_start[3];
    char multiline_comment_end[3];
    int flags;
};

/* 表示正在编辑的文件中的一行。 */
typedef struct erow {
    int idx;            /* 行在文件中的零基索引。 */
    int size;           /* 行内容长度，不包含结尾的空字符。 */
    int rsize;          /* 展开制表符后用于显示的行长度。 */
    char *chars;        /* 原始行内容。 */
    char *render;       /* 用于屏幕显示的行内容（制表符已展开）。 */
    unsigned char *hl;  /* render 中每个字符对应的语法高亮类型。 */
    int hl_oc;          /* 最近一次高亮时，行尾是否仍处于多行注释中。 */
} erow;

typedef struct hlcolor {
    int r,g,b;
} hlcolor;

struct editorConfig {
    int cx,cy;          /* 屏幕上的光标位置，以字符为单位。 */
    int rowoff;         /* 当前显示区域对应的文件行偏移。 */
    int coloff;         /* 当前显示区域对应的文件列偏移。 */
    int screenrows;     /* 可用于显示文本的行数。 */
    int screencols;     /* 可用于显示文本的列数。 */
    int numrows;        /* 文件行数。 */
    int rawmode;        /* 是否已启用终端原始模式。 */
    erow *row;          /* 文件行数组。 */
    int dirty;          /* 文件是否有尚未保存的修改。 */
    char *filename;     /* 当前打开的文件名。 */
    char statusmsg[80]; /* 底部状态栏临时消息。 */
    time_t statusmsg_time;
    struct editorSyntax *syntax; /* 当前语法高亮方案，没有则为 NULL。 */
#ifdef _WIN32
    UINT file_code_page; /* 当前文件的编码：UTF-8 或 Windows CP936。 */
#endif
};

static struct editorConfig E;

enum KEY_ACTION{
        KEY_NULL = 0,       /* 空键。 */
        CTRL_C = 3,         /* Ctrl-C。 */
        CTRL_D = 4,         /* Ctrl-D。 */
        CTRL_F = 6,         /* Ctrl-F。 */
        CTRL_H = 8,         /* Ctrl-H。 */
        TAB = 9,            /* Tab。 */
        CTRL_L = 12,        /* Ctrl-L。 */
        ENTER = 13,         /* Enter。 */
        CTRL_Q = 17,        /* Ctrl-Q。 */
        CTRL_S = 19,        /* Ctrl-S。 */
        CTRL_U = 21,        /* Ctrl-U。 */
        ESC = 27,           /* Escape。 */
        BACKSPACE =  127,   /* Backspace。 */
        /* 以下是编辑器内部使用的软编码，并非终端直接上报的字符。 */
        ARROW_LEFT = 1000,
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
        DEL_KEY,
        HOME_KEY,
        END_KEY,
        PAGE_UP,
        PAGE_DOWN
};

void editorSetStatusMessage(const char *fmt, ...);

/* =========================== 语法高亮数据库 ================================
 *
 * 添加新的语法时，需要定义文件名匹配列表和关键字列表。文件名匹配规则为：
 * 以点号开头的模式匹配文件名后缀，例如 ".c"；其他模式只要出现在文件名中
 * 即可匹配，例如 "Makefile"。
 *
 * 关键字列表就是待高亮的单词列表。若单词末尾带有 '|'，则使用另一种颜色，
 * 这样可以把关键字分为两组。最后在全局 HLDB 数组中加入一项，填入两个数组、
 * 注释分隔符以及用于启用注释和数字高亮的标志。
 *
 * 单行和多行注释分隔符目前都必须正好是两个字符（参见 C 语言配置）。
 * 当前实现不支持正则表达式等模式高亮。 */

/* C / C++。 */
char *C_HL_extensions[] = {".c",".h",".cpp",".hpp",".cc",NULL};
char *C_HL_keywords[] = {
	/* C 关键字。 */
	"auto","break","case","continue","default","do","else","enum",
	"extern","for","goto","if","register","return","sizeof","static",
	"struct","switch","typedef","union","volatile","while","NULL",

	/* C++ 关键字。 */
	"alignas","alignof","and","and_eq","asm","bitand","bitor","class",
	"compl","constexpr","const_cast","deltype","delete","dynamic_cast",
	"explicit","export","false","friend","inline","mutable","namespace",
	"new","noexcept","not","not_eq","nullptr","operator","or","or_eq",
	"private","protected","public","reinterpret_cast","static_assert",
	"static_cast","template","this","thread_local","throw","true","try",
	"typeid","typename","virtual","xor","xor_eq",

	/* C 类型。 */
        "int|","long|","double|","float|","char|","unsigned|","signed|",
        "void|","short|","auto|","const|","bool|",NULL
};

/* 根据扩展名、关键字、注释分隔符和标志定义语法高亮方案数组。 */
struct editorSyntax HLDB[] = {
    {
        /* C / C++。 */
        C_HL_extensions,
        C_HL_keywords,
        "//","/*","*/",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS
    }
};

#define HLDB_ENTRIES (sizeof(HLDB)/sizeof(HLDB[0]))

/* ======================= 底层终端处理 ====================================== */

#ifdef _WIN32
static DWORD orig_console_input_mode;
static DWORD orig_console_output_mode;

/* 中文提示以 UTF-8 窄字符串保存；启动时让 Windows 控制台按 UTF-8 解码输出。 */
static void kiloConfigureConsoleEncoding(void) {
    /* 重定向到文件或调试器管道时该调用可能失败，此时不影响程序继续运行。 */
    SetConsoleOutputCP(CP_UTF8);
}
#else
static struct termios orig_termios; /* 退出时用于恢复终端设置。 */
#endif

void disableRawMode(int fd) {
#ifdef _WIN32
    (void)fd;
    /* Windows 控制台使用控制台模式保存/恢复原始输入状态。 */
    if (E.rawmode) {
        SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), orig_console_input_mode);
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), orig_console_output_mode);
        E.rawmode = 0;
    }
#else
    /* 程序退出阶段已经无法可靠处理恢复失败。 */
    (void)fd;
    if (E.rawmode) {
        tcsetattr(fd,TCSAFLUSH,&orig_termios);
        E.rawmode = 0;
    }
#endif
}

/* 程序退出时调用，避免终端停留在原始模式。 */
void editorAtExit(void) {
    disableRawMode(STDIN_FILENO);
}

/* 启用原始模式：关闭行缓冲和回显，使编辑器可以逐键读取输入。 */
int enableRawMode(int fd) {
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD input_mode, output_mode;

    (void)fd;
    if (E.rawmode) return 0;
    if (!GetConsoleMode(input,&orig_console_input_mode) ||
        !GetConsoleMode(output,&orig_console_output_mode)) goto fatal;

    input_mode = orig_console_input_mode;
    input_mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
                    ENABLE_PROCESSED_INPUT);
    input_mode |= ENABLE_EXTENDED_FLAGS;
    if (!SetConsoleMode(input,input_mode)) goto fatal;

    /* 使用 Windows 10+ 控制台支持的 ANSI/VT100 输出序列。 */
    output_mode = orig_console_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(output,output_mode)) {
        SetConsoleMode(input,orig_console_input_mode);
        goto fatal;
    }

    atexit(editorAtExit);
    E.rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
#else
    struct termios raw;

    if (E.rawmode) return 0; /* 已经启用原始模式。 */
    if (!isatty(STDIN_FILENO)) goto fatal;
    atexit(editorAtExit);
    if (tcgetattr(fd,&orig_termios) == -1) goto fatal;

    raw = orig_termios;  /* 从原始终端设置复制一份进行修改。 */
    /* 输入模式：关闭 BREAK、CR 转 NL、奇偶校验、字符剥离以及流控。 */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* 输出模式：关闭输出后处理。 */
    raw.c_oflag &= ~(OPOST);
    /* 控制模式：使用 8 位字符。 */
    raw.c_cflag |= (CS8);
    /* 本地模式：关闭回显、规范模式、扩展功能和信号字符（^Z、^C）。 */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* 控制字符：设置最小返回字节数和读取超时时间。 */
    raw.c_cc[VMIN] = 0; /* 有字符就返回，没有字符则等待超时。 */
    raw.c_cc[VTIME] = 1; /* 超时 100 毫秒，单位为十分之一秒。 */

    /* 刷新输入输出后，将终端切换为原始模式。 */
    if (tcsetattr(fd,TCSAFLUSH,&raw) < 0) goto fatal;
    E.rawmode = 1;
    return 0;

fatal:
    errno = ENOTTY;
    return -1;
#endif
}

/* 从原始终端读取一个按键，并解析方向键等转义序列。 */
int editorReadKey(int fd) {
#ifdef _WIN32
    int c;

    (void)fd;
    c = _getch();
    if (c == 0 || c == 224) {
        /* Windows 控制台用第二个字节表示扩展键。 */
        switch (_getch()) {
        case 72: return ARROW_UP;
        case 80: return ARROW_DOWN;
        case 75: return ARROW_LEFT;
        case 77: return ARROW_RIGHT;
        case 71: return HOME_KEY;
        case 79: return END_KEY;
        case 73: return PAGE_UP;
        case 81: return PAGE_DOWN;
        case 83: return DEL_KEY;
        default: return KEY_NULL;
        }
    }
    return c;
#else
    int nread;
    char c, seq[3];
    while ((nread = read(fd,&c,1)) == 0);
    if (nread == -1) exit(1);

    while(1) {
        switch(c) {
        case ESC:    /* 转义序列。 */
            /* 如果只是单独按下 ESC，这里会在超时后返回。 */
            if (read(fd,seq,1) == 0) return ESC;
            if (read(fd,seq+1,1) == 0) return ESC;

            /* ESC [ 序列。 */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* 扩展转义序列，再读取一个字节。 */
                    if (read(fd,seq+2,1) == 0) return ESC;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
            }

            /* ESC O 序列。 */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
            break;
        default:
            return c;
        }
    }
#endif
}

#ifndef _WIN32
/* 使用 ESC [6n 查询光标位置。失败返回 -1，成功时把行列写入 rows/cols，
 * 并返回 0。 */
int getCursorPosition(int ifd, int ofd, int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    /* 请求终端上报光标位置。 */
    if (write(ofd, "\x1b[6n", 4) != 4) return -1;

    /* 读取响应：ESC [ 行号 ; 列号 R。 */
    while (i < sizeof(buf)-1) {
        if (read(ifd,buf+i,1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    /* 解析终端响应。 */
    if (buf[0] != ESC || buf[1] != '[') return -1;
    if (sscanf(buf+2,"%d;%d",rows,cols) != 2) return -1;
    return 0;
}
#endif

/* 获取当前终端尺寸。成功返回 0，失败返回 -1。 */
int getWindowSize(int ifd, int ofd, int *rows, int *cols) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;

    (void)ifd;
    (void)ofd;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&info))
        return -1;
    *cols = info.srWindow.Right - info.srWindow.Left + 1;
    *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    return (*rows > 0 && *cols > 0) ? 0 : -1;
#else
    struct winsize ws;

    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        /* ioctl() 失败，改为直接查询终端。 */
        int orig_row, orig_col, retval;

        /* 保存初始光标位置，稍后恢复。 */
        retval = getCursorPosition(ifd,ofd,&orig_row,&orig_col);
        if (retval == -1) goto failed;

        /* 移到右下边界，再读取终端返回的位置。 */
        if (write(ofd,"\x1b[999C\x1b[999B",12) != 12) goto failed;
        retval = getCursorPosition(ifd,ofd,rows,cols);
        if (retval == -1) goto failed;

        /* 恢复原光标位置。 */
        char seq[32];
        snprintf(seq,32,"\x1b[%d;%dH",orig_row,orig_col);
        if (write(ofd,seq,strlen(seq)) == -1) {
            /* 无法恢复时继续返回，调用方会处理失败。 */
        }
        return 0;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }

failed:
    return -1;
#endif
}

/* ====================== 语法高亮颜色方案 ================================== */

/* 判断字符是否可以作为单词分隔符。 */
int is_separator(int c) {
    return c == '\0' || isspace((unsigned char)c) ||
        strchr(",.()+-/*=~%[];",c) != NULL;
}

/* ctype 函数在 C 语言环境下通常只把 ASCII 判定为可打印；UTF-8/GBK 的
 * 高位字节仍属于文本内容，不能因此被编辑器替换为问号。 */
static int kiloIsPrintableByte(int c) {
    unsigned char byte = (unsigned char)c;
    return byte >= 0x80 || isprint(byte);
}

/* 判断指定行的最后一个字符是否处于多行注释中。该注释可能从本行或上一行开始，
 * 但尚未在本行末尾结束，因此会延续到下一行。 */
int editorRowHasOpenComment(erow *row) {
    if (row->hl && row->rsize && row->hl[row->rsize-1] == HL_MLCOMMENT &&
        (row->rsize < 2 || (row->render[row->rsize-2] != '*' ||
                            row->render[row->rsize-1] != '/'))) return 1;
    return 0;
}

/* 为 row->hl 中与行字符对应的每个字节设置正确的 HL_* 语法高亮类型。 */
void editorUpdateSyntax(erow *row) {
    row->hl = realloc(row->hl,row->rsize);
    memset(row->hl,HL_NORMAL,row->rsize);

    if (E.syntax == NULL) return; /* 没有语法方案，所有字符保持 HL_NORMAL。 */

    int i, prev_sep, in_string, in_comment;
    char *p;
    char **keywords = E.syntax->keywords;
    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    /* 跳过行首空白，定位到第一个非空白字符。 */
    p = row->render;
    i = 0; /* 当前字符偏移。 */
    while(*p && isspace((unsigned char)*p)) {
        p++;
        i++;
    }
    prev_sep = 1; /* 告诉解析器 i 是否指向单词开头。 */
    in_string = 0; /* 是否处于双引号或单引号字符串中。 */
    in_comment = 0; /* 是否处于多行注释中。 */

    /* 如果上一行的多行注释未结束，本行从注释状态开始。 */
    if (row->idx > 0 && editorRowHasOpenComment(&E.row[row->idx-1]))
        in_comment = 1;

    while(*p) {
        /* 处理 // 单行注释。 */
        if (prev_sep && *p == scs[0] && *(p+1) == scs[1]) {
            /* 从当前位置直到行尾都属于注释。 */
            memset(row->hl+i,HL_COMMENT,row->size-i);
            return;
        }

        /* 处理多行注释。 */
        if (in_comment) {
            row->hl[i] = HL_MLCOMMENT;
            if (*p == mce[0] && *(p+1) == mce[1]) {
                row->hl[i+1] = HL_MLCOMMENT;
                p += 2; i += 2;
                in_comment = 0;
                prev_sep = 1;
                continue;
            } else {
                prev_sep = 0;
                p++; i++;
                continue;
            }
        } else if (*p == mcs[0] && *(p+1) == mcs[1]) {
            row->hl[i] = HL_MLCOMMENT;
            row->hl[i+1] = HL_MLCOMMENT;
            p += 2; i += 2;
            in_comment = 1;
            prev_sep = 0;
            continue;
        }

        /* 处理双引号字符串和单引号字符常量。 */
        if (in_string) {
            row->hl[i] = HL_STRING;
            if (*p == '\\') {
                row->hl[i+1] = HL_STRING;
                p += 2; i += 2;
                prev_sep = 0;
                continue;
            }
            if (*p == in_string) in_string = 0;
            p++; i++;
            continue;
        } else {
            if (*p == '"' || *p == '\'') {
                in_string = *p;
                row->hl[i] = HL_STRING;
                p++; i++;
                prev_sep = 0;
                continue;
            }
        }

        /* 处理不可打印字符。 */
        if (!kiloIsPrintableByte((unsigned char)*p)) {
            row->hl[i] = HL_NONPRINT;
            p++; i++;
            prev_sep = 0;
            continue;
        }

        /* 处理数字。 */
        if ((isdigit((unsigned char)*p) &&
             (prev_sep || row->hl[i-1] == HL_NUMBER)) ||
            (*p == '.' && i >0 && row->hl[i-1] == HL_NUMBER)) {
            row->hl[i] = HL_NUMBER;
            p++; i++;
            prev_sep = 0;
            continue;
        }

        /* 处理关键字和库调用。 */
        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = (int)strlen(keywords[j]);
                int kw2 = keywords[j][klen-1] == '|';
                if (kw2) klen--;

                if (!memcmp(p,keywords[j],klen) &&
                    is_separator(*(p+klen)))
                {
                    /* 找到关键字。 */
                    memset(row->hl+i,kw2 ? HL_KEYWORD2 : HL_KEYWORD1,klen);
                    p += klen;
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue; /* 已匹配到关键字。 */
            }
        }

        /* 普通字符。 */
        prev_sep = is_separator(*p);
        p++; i++;
    }

    /* 如果本行的多行注释状态发生变化，则递归更新下一行以及后续受影响的行。 */
    int oc = editorRowHasOpenComment(row);
    if (row->hl_oc != oc && row->idx+1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx+1]);
    row->hl_oc = oc;
}

/* 将语法高亮标记映射为终端颜色编号。 */
int editorSyntaxToColor(int hl) {
    switch(hl) {
    case HL_COMMENT:
    case HL_MLCOMMENT: return 36;     /* 青色。 */
    case HL_KEYWORD1: return 33;      /* 黄色。 */
    case HL_KEYWORD2: return 32;      /* 绿色。 */
    case HL_STRING: return 35;        /* 洋红色。 */
    case HL_NUMBER: return 31;        /* 红色。 */
    case HL_MATCH: return 34;         /* 蓝色。 */
    default: return 37;               /* 白色。 */
    }
}

/* 根据文件名选择语法高亮方案，并写入全局状态 E.syntax。 */
void editorSelectSyntaxHighlight(char *filename) {
    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = HLDB+j;
        unsigned int i = 0;
        while(s->filematch[i]) {
            char *p;
            int patlen = (int)strlen(s->filematch[i]);
            if ((p = strstr(filename,s->filematch[i])) != NULL) {
                if (s->filematch[i][0] != '.' || p[patlen] == '\0') {
                    E.syntax = s;
                    return;
                }
            }
            i++;
        }
    }
}

/* ======================= 编辑器行操作 ====================================== */

/* 更新一行的显示版本和语法高亮信息。 */
void editorUpdateRow(erow *row) {
    unsigned int tabs = 0, nonprint = 0;
    int j, idx;

   /* 创建可以直接输出到屏幕的行副本：展开制表符，并将不可打印字符替换为 '?'. */
    free(row->render);
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == TAB) tabs++;

    unsigned long long allocsize =
        (unsigned long long)row->size +
        (unsigned long long)tabs * 8 +
        (unsigned long long)nonprint * 9 + 1;
    if (allocsize > UINT32_MAX) {
        printf("编辑器无法处理超过限制的超长行。\n");
        exit(1);
    }

    row->render = malloc((size_t)allocsize);
    idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == TAB) {
            row->render[idx++] = ' ';
            while((idx+1) % 8 != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->rsize = idx;
    row->render[idx] = '\0';

    /* 更新本行的语法高亮属性。 */
    editorUpdateSyntax(row);
}

/* 在指定位置插入一行；必要时将后面的行向后移动。 */
void editorInsertRow(int at, char *s, size_t len) {
    if (at > E.numrows) return;
    if (len > INT_MAX) {
        fprintf(stderr,"行内容超过编辑器支持的长度。\n");
        exit(1);
    }
    E.row = realloc(E.row,sizeof(erow)*(E.numrows+1));
    if (at != E.numrows) {
        memmove(E.row+at+1,E.row+at,sizeof(E.row[0])*(E.numrows-at));
        for (int j = at+1; j <= E.numrows; j++) E.row[j].idx++;
    }
    E.row[at].size = (int)len;
    E.row[at].chars = malloc(len+1);
    memcpy(E.row[at].chars,s,len+1);
    E.row[at].hl = NULL;
    E.row[at].hl_oc = 0;
    E.row[at].render = NULL;
    E.row[at].rsize = 0;
    E.row[at].idx = at;
    editorUpdateRow(E.row+at);
    E.numrows++;
    E.dirty++;
}

/* 释放行在堆上分配的内容。 */
void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

/* 删除指定位置的行，并将后面的行向前移动。 */
void editorDelRow(int at) {
    erow *row;

    if (at >= E.numrows) return;
    row = E.row+at;
    editorFreeRow(row);
    memmove(E.row+at,E.row+at+1,sizeof(E.row[0])*(E.numrows-at-1));
    for (int j = at; j < E.numrows-1; j++) E.row[j].idx++;
    E.numrows--;
    E.dirty++;
}

/* 将编辑器中的所有行拼接为一个堆字符串。返回字符串指针，并把不含结尾空字符
 * 的字节数写入 buflen。每一行后面都会补一个换行符。 */
char *editorRowsToString(int *buflen) {
    char *buf = NULL, *p;
    int totlen = 0;
    int j;

    /* 计算总字节数。 */
    for (j = 0; j < E.numrows; j++)
        totlen += E.row[j].size+1; /* 每行末尾的换行符额外占 1 字节。 */
    *buflen = totlen;
    totlen++; /* 另外为结尾空字符预留 1 字节。 */

    p = buf = malloc(totlen);
    for (j = 0; j < E.numrows; j++) {
        memcpy(p,E.row[j].chars,E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    *p = '\0';
    return buf;
}

/* 在行内指定位置插入字符，必要时将右侧内容向后移动。 */
void editorRowInsertChar(erow *row, int at, int c) {
    if (at > row->size) {
        /* 如果插入位置超出当前行长度，则用空格补齐中间的缺口。 */
        int padlen = at-row->size;
        /* 下面的 +2 分别为新字符和结尾空字符预留空间。 */
        row->chars = realloc(row->chars,row->size+padlen+2);
        memset(row->chars+row->size,' ',padlen);
        row->chars[row->size+padlen+1] = '\0';
        row->size += padlen+1;
    } else {
        /* 在行中间插入时，只需为一个新字符和原有的结尾空字符腾出空间。 */
        row->chars = realloc(row->chars,row->size+2);
        memmove(row->chars+at+1,row->chars+at,row->size-at+1);
        row->size++;
    }
    row->chars[at] = (char)c;
    editorUpdateRow(row);
    E.dirty++;
}

/* 将字符串 s 追加到行尾。 */
void editorRowAppendString(erow *row, char *s, size_t len) {
    if (len > (size_t)(INT_MAX - row->size)) {
        fprintf(stderr,"合并后的行超过编辑器支持的长度。\n");
        exit(1);
    }
    row->chars = realloc(row->chars,row->size+len+1);
    memcpy(row->chars+row->size,s,len);
    row->size += (int)len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

/* 删除指定行中偏移为 at 的字符。 */
void editorRowDelChar(erow *row, int at) {
    if (row->size <= at) return;
    memmove(row->chars+at,row->chars+at+1,row->size-at);
    editorUpdateRow(row);
    row->size--;
    E.dirty++;
}

/* 在当前光标位置插入指定字符。 */
void editorInsertChar(int c) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    /* 如果光标所在行尚不存在于文件模型中，则补充足够的空行。 */
    if (!row) {
        while(E.numrows <= filerow)
            editorInsertRow(E.numrows,"",0);
    }
    row = &E.row[filerow];
    editorRowInsertChar(row,filecol,c);
    if (E.cx == E.screencols-1)
        E.coloff++;
    else
        E.cx++;
    E.dirty++;
}

/* 插入换行略复杂：当光标位于行中间时，需要把当前行拆分为两行。 */
void editorInsertNewline(void) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    if (!row) {
        if (filerow == E.numrows) {
            editorInsertRow(filerow,"",0);
            goto fixcursor;
        }
        return;
    }
    /* 光标超过当前行长度时，按位于最后一个字符之后处理。 */
    if (filecol >= row->size) filecol = row->size;
    if (filecol == 0) {
        editorInsertRow(filerow,"",0);
    } else {
        /* 光标位于行中间，将内容拆分为前后两行。 */
        editorInsertRow(filerow+1,row->chars+filecol,row->size-filecol);
        row = &E.row[filerow];
        row->chars[filecol] = '\0';
        row->size = filecol;
        editorUpdateRow(row);
    }
fixcursor:
    if (E.cy == E.screenrows-1) {
        E.rowoff++;
    } else {
        E.cy++;
    }
    E.cx = 0;
    E.coloff = 0;
}

/* 删除当前光标位置之前的字符，必要时合并相邻两行。 */
void editorDelChar(void) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    if (!row || (filecol == 0 && filerow == 0)) return;
    if (filecol == 0) {
        /* 光标位于第 0 列时，将当前行追加到上一行末尾。 */
        filecol = E.row[filerow-1].size;
        editorRowAppendString(&E.row[filerow-1],row->chars,row->size);
        editorDelRow(filerow);
        row = NULL;
        if (E.cy == 0)
            E.rowoff--;
        else
            E.cy--;
        E.cx = filecol;
        if (E.cx >= E.screencols) {
            int shift = (E.screencols-E.cx)+1;
            E.cx -= shift;
            E.coloff += shift;
        }
    } else {
        editorRowDelChar(row,filecol-1);
        if (E.cx == 0 && E.coloff)
            E.coloff--;
        else
            E.cx--;
    }
    if (row) editorUpdateRow(row);
    E.dirty++;
}

/* 将指定文件加载到编辑器内存中；成功返回 0，文件不存在时返回 1。 */

/* 读取一整行，不依赖 POSIX getline，因此可同时用于 GCC 和 MSVC。
 * 返回 1 表示读到一行，0 表示文件结束，-1 表示内存分配失败。 */
static int kiloReadLine(FILE *fp, char **line, size_t *linecap,
                        size_t *linelen) {
    size_t len = 0;
    int ch;

    if (*line == NULL || *linecap < 2) {
        *linecap = 128;
        *line = malloc(*linecap);
        if (*line == NULL) return -1;
    }

    while ((ch = fgetc(fp)) != EOF) {
        if (len + 1 >= *linecap) {
            size_t newcap = *linecap * 2;
            char *new_line = realloc(*line,newcap);
            if (new_line == NULL) return -1;
            *line = new_line;
            *linecap = newcap;
        }
        (*line)[len++] = (char)ch;
        if (ch == '\n') break;
    }

    if (len == 0 && ch == EOF) return 0;
    (*line)[len] = '\0';
    *linelen = len;
    return 1;
}

#ifdef _WIN32
#define KILO_CP936 936

/* 严格判断一段字节是否为合法 UTF-8；ASCII 文件也视为 UTF-8。 */
static int kiloIsValidUtf8(const unsigned char *data, size_t length) {
    size_t i = 0;

    while (i < length) {
        unsigned char first = data[i];
        if (first <= 0x7f) {
            i++;
        } else if (first >= 0xc2 && first <= 0xdf) {
            if (length - i < 2 || data[i+1] < 0x80 || data[i+1] > 0xbf)
                return 0;
            i += 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            unsigned char second;
            if (length - i < 3 || data[i+1] < 0x80 || data[i+1] > 0xbf ||
                data[i+2] < 0x80 || data[i+2] > 0xbf)
                return 0;
            second = data[i+1];
            if ((first == 0xe0 && second < 0xa0) ||
                (first == 0xed && second >= 0xa0))
                return 0;
            i += 3;
        } else if (first >= 0xf0 && first <= 0xf4) {
            unsigned char second;
            if (length - i < 4 || data[i+1] < 0x80 || data[i+1] > 0xbf ||
                data[i+2] < 0x80 || data[i+2] > 0xbf ||
                data[i+3] < 0x80 || data[i+3] > 0xbf)
                return 0;
            second = data[i+1];
            if ((first == 0xf0 && second < 0x90) ||
                (first == 0xf4 && second >= 0x90))
                return 0;
            i += 4;
        } else {
            return 0;
        }
    }
    return 1;
}

/* 检测文件编码；读取失败或文件过大时保守地按 UTF-8 处理。 */
static int kiloFileIsUtf8(FILE *fp) {
    long saved_position;
    long file_size;
    unsigned char *data;
    size_t length;
    int result;

    saved_position = ftell(fp);
    if (saved_position < 0 || fseek(fp,0,SEEK_END) != 0)
        return 1;
    file_size = ftell(fp);
    if (file_size < 0 ||
        (unsigned long long)file_size > (unsigned long long)SIZE_MAX) {
        fseek(fp,saved_position,SEEK_SET);
        return 1;
    }
    length = (size_t)file_size;
    if (fseek(fp,0,SEEK_SET) != 0) {
        fseek(fp,saved_position,SEEK_SET);
        return 1;
    }

    data = malloc(length ? length : 1);
    if (data == NULL) {
        fseek(fp,saved_position,SEEK_SET);
        return 1;
    }
    result = length == 0 || fread(data,1,length,fp) == length
        ? kiloIsValidUtf8(data,length) : 1;
    free(data);
    fseek(fp,saved_position,SEEK_SET);
    return result;
}

/* 使用 Windows API 在 UTF-8、CP936 等代码页之间转换文本。 */
static char *kiloConvertTextEncoding(const char *source, size_t source_length,
                                     UINT source_code_page,
                                     UINT target_code_page,
                                     size_t *target_length) {
    int wide_length;
    int converted_length;
    wchar_t *wide;
    char *converted;
    BOOL used_default = FALSE;
    DWORD source_flags = MB_ERR_INVALID_CHARS;
    DWORD target_flags = target_code_page == CP_UTF8 ? 0 : WC_NO_BEST_FIT_CHARS;
    const char *default_char = target_code_page == CP_UTF8 ? NULL : "?";

    if (source_length > INT_MAX) return NULL;
    if (source_length == 0) {
        converted = malloc(1);
        if (converted != NULL) {
            converted[0] = '\0';
            *target_length = 0;
        }
        return converted;
    }

    wide_length = MultiByteToWideChar(source_code_page,source_flags,
                                      source,(int)source_length,NULL,0);
    if (wide_length == 0 && source_code_page != CP_UTF8) {
        /* CP936 对少数非标准字节放宽校验，尽量保留可读文本。 */
        wide_length = MultiByteToWideChar(source_code_page,0,
                                          source,(int)source_length,NULL,0);
    }
    if (wide_length <= 0) return NULL;
    wide = malloc((size_t)wide_length * sizeof(*wide));
    if (wide == NULL) return NULL;
    if (MultiByteToWideChar(source_code_page,source_flags,
                            source,(int)source_length,wide,wide_length) == 0 &&
        source_code_page != CP_UTF8) {
        if (MultiByteToWideChar(source_code_page,0,
                                source,(int)source_length,wide,wide_length) == 0) {
            free(wide);
            return NULL;
        }
    }

    converted_length = WideCharToMultiByte(target_code_page,target_flags,
                                            wide,wide_length,NULL,0,
                                            default_char,&used_default);
    if (converted_length <= 0) {
        free(wide);
        return NULL;
    }
    converted = malloc((size_t)converted_length + 1);
    if (converted == NULL) {
        free(wide);
        return NULL;
    }
    used_default = FALSE;
    if (WideCharToMultiByte(target_code_page,target_flags,
                            wide,wide_length,converted,converted_length,
                            default_char,&used_default) <= 0 || used_default) {
        free(converted);
        free(wide);
        return NULL;
    }
    converted[converted_length] = '\0';
    *target_length = (size_t)converted_length;
    free(wide);
    return converted;
}
#endif

int editorOpen(char *filename) {
    FILE *fp;

    E.dirty = 0;
    free(E.filename);
    size_t fnlen = strlen(filename)+1;
    E.filename = malloc(fnlen);
    memcpy(E.filename,filename,fnlen);

    fp = fopen(filename,"rb");
    if (!fp) {
        if (errno != ENOENT) {
            perror("打开文件失败");
            exit(1);
        }
        return 1;
    }
#ifdef _WIN32
    E.file_code_page = kiloFileIsUtf8(fp) ? CP_UTF8 : KILO_CP936;
#endif

    char *line = NULL;
    size_t linecap = 0;
    size_t linelen;
    int read_result;
    while((read_result = kiloReadLine(fp,&line,&linecap,&linelen)) > 0) {
        /* 同时去除 Unix、Windows 和旧式 Mac 文件的行结束符。 */
        while (linelen && (line[linelen-1] == '\n' ||
                           line[linelen-1] == '\r'))
            line[--linelen] = '\0';
#ifdef _WIN32
        if (E.numrows == 0 && linelen >= 3 &&
            (unsigned char)line[0] == 0xef &&
            (unsigned char)line[1] == 0xbb &&
            (unsigned char)line[2] == 0xbf) {
            memmove(line,line+3,linelen-3);
            linelen -= 3;
            line[linelen] = '\0';
        }
        if (E.file_code_page == KILO_CP936 && linelen > 0) {
            size_t converted_length;
            char *converted = kiloConvertTextEncoding(
                line,linelen,KILO_CP936,CP_UTF8,&converted_length);
            if (converted == NULL) {
                fprintf(stderr,"无法将 CP936 文件转换为 UTF-8：%s\n",filename);
                free(line);
                fclose(fp);
                exit(1);
            }
            free(line);
            line = converted;
            linecap = converted_length + 1;
            linelen = converted_length;
        }
#endif
        editorInsertRow(E.numrows,line,linelen);
    }
    if (read_result < 0) {
        perror("读取文件失败");
        free(line);
        fclose(fp);
        exit(1);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
    return 0;
}

/* 将当前文件保存到磁盘；成功返回 0，失败返回 1。 */

/* 将文件截断到指定长度，封装 POSIX ftruncate 与 Windows _chsize_s 的差异。 */
static int kiloTruncate(int fd, int length) {
#ifdef _WIN32
    return _chsize_s(fd,(__int64)length) == 0 ? 0 : -1;
#else
    return ftruncate(fd,length);
#endif
}

/* 生成无文件名新建文档的默认保存路径：Windows 使用 exe 所在目录，
 * 其他系统使用当前目录。 */
static char *kiloDefaultFilename(void) {
#ifdef _WIN32
    char module_path[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL,module_path,sizeof(module_path));
    char *separator;
    char *filename;
    size_t directory_length;

    if (length == 0 || length >= sizeof(module_path))
        return NULL;
    module_path[length] = '\0';
    separator = strrchr(module_path,'\\');
    if (separator == NULL) separator = strrchr(module_path,'/');
    if (separator == NULL) return NULL;
    separator[1] = '\0';
    directory_length = strlen(module_path);
    filename = malloc(directory_length + strlen("temp.c") + 1);
    if (filename == NULL) return NULL;
    memcpy(filename,module_path,directory_length);
    memcpy(filename + directory_length,"temp.c",strlen("temp.c") + 1);
    return filename;
#else
    char *filename = malloc(strlen("temp.c") + 1);
    if (filename == NULL) return NULL;
    memcpy(filename,"temp.c",strlen("temp.c") + 1);
    return filename;
#endif
}

#define KILO_FILENAME_LEN 512

/* 保存文件名提示需要在每次输入后重绘屏幕，实际定义位于终端刷新模块。 */
void editorRefreshScreen(void);

/* 在状态栏中逐字符输入保存路径。Enter 确认，Esc 或 Ctrl-C 取消。 */
static char *editorPrompt(int fd) {
    char *filename = malloc(KILO_FILENAME_LEN + 1);
    int length = 0;

    if (filename == NULL) return NULL;
    filename[0] = '\0';

    while (1) {
        editorSetStatusMessage(
            "保存文件名（Enter 确认；留空使用默认 temp.c；Esc 取消）：%s",
            filename);
        editorRefreshScreen();

        {
            int c = editorReadKey(fd);
            if (c == ENTER) return filename;
            if (c == ESC || c == CTRL_C) {
                free(filename);
                return NULL;
            }
            if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE) {
                if (length > 0) filename[--length] = '\0';
            } else if (kiloIsPrintableByte((unsigned char)c) &&
                       length < KILO_FILENAME_LEN) {
                filename[length++] = (char)c;
                filename[length] = '\0';
            }
        }
    }
}

int editorSave(int fd) {
    int len;
    int write_len;
    int out_fd = -1;
    char *buf;
    char *encoded_buf = NULL;
    char *new_filename = NULL;
    const char *filename = E.filename;

    if (filename == NULL) {
        new_filename = editorPrompt(fd);
        if (new_filename == NULL) {
            editorSetStatusMessage("已取消保存");
            return 1;
        }
        if (new_filename[0] == '\0') {
            free(new_filename);
            new_filename = kiloDefaultFilename();
            if (new_filename == NULL) {
                editorSetStatusMessage("保存失败：无法生成默认文件路径");
                return 1;
            }
        }
        filename = new_filename;
    }

    buf = editorRowsToString(&len);
    write_len = len;
#ifdef _WIN32
    if (E.file_code_page == KILO_CP936 && len > 0) {
        size_t encoded_length;
        encoded_buf = kiloConvertTextEncoding(
            buf,(size_t)len,CP_UTF8,KILO_CP936,&encoded_length);
        if (encoded_buf == NULL || encoded_length > INT_MAX) {
            errno = EILSEQ;
            goto writeerr;
        }
        write_len = (int)encoded_length;
    }
#endif
#ifdef _WIN32
    out_fd = _open(filename,O_RDWR|O_CREAT|O_BINARY,
                   _S_IREAD|_S_IWRITE);
#else
    out_fd = open(filename,O_RDWR|O_CREAT,0644);
#endif
    if (out_fd == -1) goto writeerr;

    /* 先截断，再一次性写入，尽量降低小型编辑器保存过程中的风险。 */
    if (kiloTruncate(out_fd,write_len) == -1) goto writeerr;
    if (write(out_fd,encoded_buf ? encoded_buf : buf,write_len) != write_len)
        goto writeerr;

    close(out_fd);
    free(encoded_buf);
    free(buf);
    if (new_filename != NULL) {
        E.filename = new_filename;
        new_filename = NULL;
        editorSelectSyntaxHighlight(E.filename);
    }
    E.dirty = 0;
    editorSetStatusMessage("已写入 %d 字节", write_len);
    return 0;

writeerr:
    free(encoded_buf);
    free(buf);
    if (out_fd != -1) close(out_fd);
    free(new_filename);
    editorSetStatusMessage("保存失败！输入/输出错误：%s",strerror(errno));
    return 1;
}

/* ============================= 终端刷新 ==================================== */

/* 简单的追加缓冲区：它是一个堆字符串，可以连续追加内容。
 * 将所有转义序列先写入缓冲区，再一次性输出，可减少屏幕闪烁。 */
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL,0}

/* 将字符串片段追加到缓冲区末尾。内存不足时保留原缓冲区。 */
void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b,ab->len+len);

    if (new == NULL) return;
    memcpy(new+ab->len,s,len);
    ab->b = new;
    ab->len += len;
}

/* 释放追加缓冲区的堆内存。 */
void abFree(struct abuf *ab) {
    free(ab->b);
}

/* 根据全局状态 E 中的逻辑内容，使用 VT100 转义序列重绘整个屏幕。 */
void editorRefreshScreen(void) {
    int y;
    erow *r;
    char buf[32];
    struct abuf ab = ABUF_INIT;

#ifdef _WIN32
    /* Windows 没有 SIGWINCH；每次重绘时检查一次窗口尺寸。 */
    {
        int rows, cols;
        if (getWindowSize(STDIN_FILENO,STDOUT_FILENO,&rows,&cols) == 0 &&
            rows > 2) {
            E.screenrows = rows - 2;
            E.screencols = cols;
            if (E.cy >= E.screenrows) E.cy = E.screenrows - 1;
            if (E.cx >= E.screencols) E.cx = E.screencols - 1;
        }
    }
#endif

    abAppend(&ab,"\x1b[?25l",6); /* 隐藏光标。 */
    abAppend(&ab,"\x1b[H",3); /* 将光标移到左上角。 */
    for (y = 0; y < E.screenrows; y++) {
        int filerow = E.rowoff+y;

        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows/3) {
                char welcome[80];
                int welcomelen = snprintf(welcome,sizeof(welcome),
                    "Kilo 编辑器 -- 版本 %s\x1b[0K\r\n", KILO_VERSION);
                int padding = (E.screencols-welcomelen)/2;
                if (padding) {
                    abAppend(&ab,"~",1);
                    padding--;
                }
                while(padding--) abAppend(&ab," ",1);
                abAppend(&ab,welcome,welcomelen);
            } else {
                abAppend(&ab,"~\x1b[0K\r\n",7);
            }
            continue;
        }

        r = &E.row[filerow];

        int len = r->rsize - E.coloff;
        int current_color = -1;
        if (len > 0) {
            if (len > E.screencols) len = E.screencols;
            char *c = r->render+E.coloff;
            unsigned char *hl = r->hl+E.coloff;
            int j;
            for (j = 0; j < len; j++) {
                if (hl[j] == HL_NONPRINT) {
                    char sym;
                    abAppend(&ab,"\x1b[7m",4);
                    if (c[j] <= 26)
                        sym = '@'+c[j];
                    else
                        sym = '?';
                    abAppend(&ab,&sym,1);
                    abAppend(&ab,"\x1b[0m",4);
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        abAppend(&ab,"\x1b[39m",5);
                        current_color = -1;
                    }
                    abAppend(&ab,c+j,1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        char colorbuf[16];
                        int clen = snprintf(colorbuf,sizeof(colorbuf),"\x1b[%dm",color);
                        current_color = color;
                        abAppend(&ab,colorbuf,clen);
                    }
                    abAppend(&ab,c+j,1);
                }
            }
        }
        abAppend(&ab,"\x1b[39m",5);
        abAppend(&ab,"\x1b[0K",4);
        abAppend(&ab,"\r\n",2);
    }

    /* 绘制两行状态栏。第一行显示文件名、行数和修改状态。 */
    abAppend(&ab,"\x1b[0K",4);
    abAppend(&ab,"\x1b[7m",4);
    char status[80], rstatus[80];
    const char *display_filename = E.filename ? E.filename : "[未命名]";
    int len = snprintf(status, sizeof(status), "%.20s - %d 行 %s",
        display_filename, E.numrows, E.dirty ? "（已修改）" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus),
        "%d/%d",E.rowoff+E.cy+1,E.numrows);
    if (len > E.screencols) len = E.screencols;
    abAppend(&ab,status,len);
    while(len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(&ab,rstatus,rlen);
            break;
        } else {
            abAppend(&ab," ",1);
            len++;
        }
    }
    abAppend(&ab,"\x1b[0m\r\n",6);

    /* 第二行显示 E.statusmsg；消息只保留 5 秒。 */
    abAppend(&ab,"\x1b[0K",4);
    int msglen = (int)strlen(E.statusmsg);
    if (msglen && time(NULL)-E.statusmsg_time < 5)
        abAppend(&ab,E.statusmsg,msglen <= E.screencols ? msglen : E.screencols);

    /* 将光标放回逻辑位置。由于制表符会展开，屏幕列位置可能不同于 E.cx。 */
    int j;
    int cx = 1;
    int filerow = E.rowoff+E.cy;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];
    if (row) {
        for (j = E.coloff; j < (E.cx+E.coloff); j++) {
            if (j < row->size && row->chars[j] == TAB) cx += 7-((cx)%8);
            cx++;
        }
    }
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy+1,cx);
    abAppend(&ab,buf,(int)strlen(buf));
    abAppend(&ab,"\x1b[?25h",6); /* 显示光标。 */
    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);
}

/* 设置状态栏第二行的临时消息。 */
void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(E.statusmsg,sizeof(E.statusmsg),fmt,ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/* =============================== 搜索模式 ================================== */

#define KILO_QUERY_LEN 256

void editorFind(int fd) {
    char query[KILO_QUERY_LEN+1] = {0};
    int qlen = 0;
    int last_match = -1; /* 上一次命中的行；-1 表示尚未命中。 */
    int find_next = 0; /* 1 表示向后搜索，-1 表示向前搜索。 */
    int saved_hl_line = -1;  /* 尚未保存高亮的行。 */
    char *saved_hl = NULL;

#define FIND_RESTORE_HL do { \
    if (saved_hl) { \
        memcpy(E.row[saved_hl_line].hl,saved_hl, E.row[saved_hl_line].rsize); \
        free(saved_hl); \
        saved_hl = NULL; \
    } \
} while (0)

    /* 保存光标位置，退出搜索时恢复。 */
    int saved_cx = E.cx, saved_cy = E.cy;
    int saved_coloff = E.coloff, saved_rowoff = E.rowoff;

    while(1) {
        editorSetStatusMessage(
            "搜索：%s（ESC 取消，方向键切换，Enter 确认）", query);
        editorRefreshScreen();

        int c = editorReadKey(fd);
        if (c == DEL_KEY || c == CTRL_H || c == BACKSPACE) {
            if (qlen != 0) query[--qlen] = '\0';
            last_match = -1;
        } else if (c == ESC || c == ENTER) {
            if (c == ESC) {
                E.cx = saved_cx; E.cy = saved_cy;
                E.coloff = saved_coloff; E.rowoff = saved_rowoff;
            }
            FIND_RESTORE_HL;
            editorSetStatusMessage("");
            return;
        } else if (c == ARROW_RIGHT || c == ARROW_DOWN) {
            find_next = 1;
        } else if (c == ARROW_LEFT || c == ARROW_UP) {
            find_next = -1;
        } else if (kiloIsPrintableByte((unsigned char)c)) {
            if (qlen < KILO_QUERY_LEN) {
                    query[qlen++] = (char)c;
                query[qlen] = '\0';
                last_match = -1;
            }
        }

        /* 查找下一个匹配项。 */
        if (last_match == -1) find_next = 1;
        if (find_next) {
            char *match = NULL;
            int match_offset = 0;
            int i, current = last_match;

            for (i = 0; i < E.numrows; i++) {
                current += find_next;
                if (current == -1) current = E.numrows-1;
                else if (current == E.numrows) current = 0;
                match = strstr(E.row[current].render,query);
                if (match) {
                    match_offset = (int)(match-E.row[current].render);
                    break;
                }
            }
            find_next = 0;

            /* 临时高亮当前匹配项。 */
            FIND_RESTORE_HL;

            if (match) {
                erow *row = &E.row[current];
                last_match = current;
                if (row->hl) {
                    saved_hl_line = current;
                    saved_hl = malloc(row->rsize);
                    memcpy(saved_hl,row->hl,row->rsize);
                    memset(row->hl+match_offset,HL_MATCH,qlen);
                }
                E.cy = 0;
                E.cx = match_offset;
                E.rowoff = current;
                E.coloff = 0;
                /* 必要时水平滚动，使匹配内容进入可视区域。 */
                if (E.cx > E.screencols) {
                    int diff = E.cx - E.screencols;
                    E.cx -= diff;
                    E.coloff += diff;
                }
            }
        }
    }
}

/* ========================= 编辑器事件处理 ================================== */

/* 根据方向键调整光标位置和滚动偏移。 */
void editorMoveCursor(int key) {
    int filerow = E.rowoff+E.cy;
    int filecol = E.coloff+E.cx;
    int rowlen;
    erow *row = (filerow >= E.numrows) ? NULL : &E.row[filerow];

    switch(key) {
    case ARROW_LEFT:
        if (E.cx == 0) {
            if (E.coloff) {
                E.coloff--;
            } else {
                if (filerow > 0) {
                    E.cy--;
                    E.cx = E.row[filerow-1].size;
                    if (E.cx > E.screencols-1) {
                        E.coloff = E.cx-E.screencols+1;
                        E.cx = E.screencols-1;
                    }
                }
            }
        } else {
            E.cx -= 1;
        }
        break;
    case ARROW_RIGHT:
        if (row && filecol < row->size) {
            if (E.cx == E.screencols-1) {
                E.coloff++;
            } else {
                E.cx += 1;
            }
        } else if (row && filecol == row->size) {
            E.cx = 0;
            E.coloff = 0;
            if (E.cy == E.screenrows-1) {
                E.rowoff++;
            } else {
                E.cy += 1;
            }
        }
        break;
    case ARROW_UP:
        if (E.cy == 0) {
            if (E.rowoff) E.rowoff--;
        } else {
            E.cy -= 1;
        }
        break;
    case ARROW_DOWN:
        if (filerow < E.numrows) {
            if (E.cy == E.screenrows-1) {
                E.rowoff++;
            } else {
                E.cy += 1;
            }
        }
        break;
    }
    /* 当前行较短时，修正超出行尾的光标位置。 */
    filerow = E.rowoff+E.cy;
    filecol = E.coloff+E.cx;
    row = (filerow >= E.numrows) ? NULL : &E.row[filerow];
    rowlen = row ? row->size : 0;
    if (filecol > rowlen) {
        E.cx -= filecol-rowlen;
        if (E.cx < 0) {
            E.coloff += E.cx;
            E.cx = 0;
        }
    }
}

/* 处理来自标准输入的键盘事件。 */
#define KILO_QUIT_TIMES 3
void editorProcessKeypress(int fd) {
    /* 文件有未保存修改时，必须连续按 N 次 Ctrl-Q 才真正退出。 */
    static int quit_times = KILO_QUIT_TIMES;

    int c = editorReadKey(fd);
    switch(c) {
    case ENTER:         /* Enter。 */
        editorInsertNewline();
        break;
    case CTRL_C:        /* Ctrl-C。 */
        /* 忽略 Ctrl-C，避免用户轻易丢失文件修改。 */
        break;
    case CTRL_Q:        /* Ctrl-Q。 */
        /* 文件已保存，或用户已确认多次后退出。 */
        if (E.dirty && quit_times) {
            editorSetStatusMessage("警告：文件有未保存的修改，还需按 Ctrl-Q %d 次才能退出。",
                quit_times);
            quit_times--;
            return;
        }
        exit(0);
        break;
    case CTRL_S:        /* Ctrl-S。 */
        editorSave(fd);
        break;
    case CTRL_F:
        editorFind(fd);
        break;
    case BACKSPACE:     /* Backspace。 */
    case CTRL_H:        /* Ctrl-H。 */
    case DEL_KEY:
        editorDelChar();
        break;
    case PAGE_UP:
    case PAGE_DOWN:
        if (c == PAGE_UP && E.cy != 0)
            E.cy = 0;
        else if (c == PAGE_DOWN && E.cy != E.screenrows-1)
            E.cy = E.screenrows-1;
        {
        int times = E.screenrows;
        while(times--)
            editorMoveCursor(c == PAGE_UP ? ARROW_UP:
                                            ARROW_DOWN);
        }
        break;

    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
    case CTRL_L: /* Ctrl-L，刷新屏幕。 */
        /* 实际重绘由主循环完成。 */
        break;
    case ESC:
        /* 普通编辑模式下忽略 ESC。 */
        break;
    default:
        editorInsertChar(c);
        break;
    }

    quit_times = KILO_QUIT_TIMES; /* 其他操作会重置退出确认次数。 */
}

/* 返回当前文件是否存在未保存修改。 */
int editorFileWasModified(void) {
    return E.dirty;
}

/* 更新编辑器可用的终端尺寸，并扣除两行状态栏空间。 */
void updateWindowSize(void) {
    if (getWindowSize(STDIN_FILENO,STDOUT_FILENO,
                      &E.screenrows,&E.screencols) == -1) {
        perror("无法获取终端窗口大小（行/列）");
        exit(1);
    }
    E.screenrows -= 2; /* 为两行状态栏预留空间。 */
}

/* POSIX 终端窗口尺寸变化时的信号处理函数。 */
void handleSigWinCh(int unused) {
    (void)unused;
    updateWindowSize();
    if (E.cy > E.screenrows) E.cy = E.screenrows - 1;
    if (E.cx > E.screencols) E.cx = E.screencols - 1;
    editorRefreshScreen();
}

/* 初始化编辑器状态、终端尺寸和窗口变化处理。 */
void initEditor(void) {
    E.cx = 0;
    E.cy = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.syntax = NULL;
#ifdef _WIN32
    E.file_code_page = CP_UTF8;
#endif
    updateWindowSize();
#ifndef _WIN32
    signal(SIGWINCH, handleSigWinCh);
#endif
}

/* 命令行入口：校验参数、打开文件并进入编辑事件循环。 */
int main(int argc, char **argv) {
#ifdef _WIN32
    kiloConfigureConsoleEncoding();
#endif
    if (argc > 2) {
        fprintf(stderr,"用法：kilo [文件名]\n");
        exit(1);
    }

    initEditor();
    if (argc == 2) {
        editorSelectSyntaxHighlight(argv[1]);
        editorOpen(argv[1]);
    }
    if (enableRawMode(STDIN_FILENO) == -1) {
        perror("无法启用终端原始模式");
        return 1;
    }
    if (argc == 2) {
        editorSetStatusMessage(
            "帮助：Ctrl-S 保存 | Ctrl-Q 退出 | Ctrl-F 搜索");
    } else {
        editorSetStatusMessage(
            "新建未命名文件；Ctrl-S 保存 | Ctrl-Q 退出 | Ctrl-F 搜索");
    }
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress(STDIN_FILENO);
    }
    return 0;
}
