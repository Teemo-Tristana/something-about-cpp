/* SDSLib 2.0 -- A C dynamic strings library
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

#ifndef __SDS_H
#define __SDS_H

#define SDS_MAX_PREALLOC (1024 * 1024) //预分配最大长度 1K * 1K = 1M
extern const char *SDS_NOINIT;

#include <sys/types.h>
#include <stdarg.h>
#include <stdint.h>

typedef char *sds; //定义 sds 的类型，也即是 sds 本质上一个 char* 类型

/* Note: sdshdr5 is never used, we just access the flags byte directly.
 * However is here to document the layout of type 5 SDS strings. */
/**
 *  五种 sds header的类型 : 处 sdshdr5外， 其余 四种类型的 len 和 alloc 类型不同
 *  不同类型的头部支持的字符串长度不同 ，这是为了空间效率才这样做的。
 *  sdshdr5类型从未被使用， 只是直接访问其flags
 * __attribute__ ((__packed__)) 表示结构体字节对齐，这是 GNU C的写法
*/
struct __attribute__((__packed__)) sdshdr5
{
    unsigned char flags; /* 3 lsb of type, and 5 msb of string length */
    char buf[];
};
struct __attribute__((__packed__)) sdshdr8
{
    /* 已使用的字符串的长度 也即是 字符串的实际长度 */
    uint8_t len; /* used */
    /* 分配的字符串内存空间大小(也即是buffer大小，但不含\0)，不包含头部和空终止符 : buf的容量 */
    uint8_t alloc; /* excluding the header and null terminator */
    /* 标志位， 一个字节， 占 8 位 : 3个最低位表示类型， 5个高位表示未使用*/
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    /* 字符串数组 */
    char buf[];
};
struct __attribute__((__packed__)) sdshdr16
{
    uint16_t len;        /* used */
    uint16_t alloc;      /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__((__packed__)) sdshdr32
{
    uint32_t len;        /* used */
    uint32_t alloc;      /* excluding the header and null terminator */
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    char buf[];
};
struct __attribute__((__packed__)) sdshdr64
{
    /* 记录 buf 数组种已使用的空间 */
    uint64_t len;        /* used */ 
    /* 记录 buf 数组中分配的长度，不包含头部和空字符结尾\0 */
    uint64_t alloc;      /* excluding the header and null terminator */
    /*标志位 低三位表示类型， 前5位未被使用*/
    unsigned char flags; /* 3 lsb of type, 5 unused bits */
    /* buf数组 */
    char buf[];
};

/* sds 类型 : 共 5 种类型*/
#define SDS_TYPE_5  0
#define SDS_TYPE_8  1
#define SDS_TYPE_16 2
#define SDS_TYPE_32 3
#define SDS_TYPE_64 4
#define SDS_TYPE_MASK 7 /* sds 的类型掩码  :0b 00000111 因为 flags 中只要 3 个最低有效位表示类型 */
#define SDS_TYPE_BITS 3 /* sds 的类型比特位:0b 00000011  3个最低位表示类型*/

/* 从 sds 获取其 header 的起始位置的指针，并声明一个sh变量赋值给它，获得方式是 sds 的地址减去头部大小 */
#define SDS_HDR_VAR(T, s) struct sdshdr##T *sh = (void *)((s) - (sizeof(struct sdshdr##T)));

/* 从sds 获取其 header 的起始位置的指针，作用和上面一个类似，但不赋值 */
#define SDS_HDR(T, s) ((struct sdshdr##T *)((s) - (sizeof(struct sdshdr##T))))

/* 获取type5类型的 sds 的长度，由于其 flags 的5个最高有效位表示字符串长度，所以直接把flags右移3位即是其字符串长度*/
#define SDS_TYPE_5_LEN(f) ((f) >> SDS_TYPE_BITS)

/* 获取 sds 的 长度 */
static inline size_t sdslen(const sds s)
{
    /*获取 sds 的类型标志字段: 向低地址方向偏移1个字节，得到 flags 字段
     flags中的低三位表示 header 的类型 */
    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK) /* flags 和 sds_type_mask 与运算 得到 s 的类型，从而获取 s 的长度*/
    {
    case SDS_TYPE_5:
        return SDS_TYPE_5_LEN(flags);
    case SDS_TYPE_8:
        return SDS_HDR(8, s)->len;
    case SDS_TYPE_16:
        return SDS_HDR(16, s)->len;
    case SDS_TYPE_32:
        return SDS_HDR(32, s)->len;
    case SDS_TYPE_64:
        return SDS_HDR(64, s)->len;
    }
    return 0;
}

/* 获取 sds 的 空闲空间 : 已分配空间 alloc - 字符串空间 len */
static inline size_t sdsavail(const sds s)
{
    unsigned char flags = s[-1]; /* 获取 s 的类型标志信息*/
    switch (flags & SDS_TYPE_MASK) /* 返回空闲空间 */
    {
    case SDS_TYPE_5: /* 若是类型位，则直接返回 0 */
    {
        return 0;
    }
    case SDS_TYPE_8:
    {
        SDS_HDR_VAR(8, s); /* 从 sds 获取 header 的起始地址*/
        return sh->alloc - sh->len;
    }
    case SDS_TYPE_16:
    {
        SDS_HDR_VAR(16, s);
        return sh->alloc - sh->len;
    }
    case SDS_TYPE_32:
    {
        SDS_HDR_VAR(32, s);
        return sh->alloc - sh->len;
    }
    case SDS_TYPE_64:
    {
        SDS_HDR_VAR(64, s);
        return sh->alloc - sh->len;
    }
    }
    return 0;
}

/*
    设置 sds 字符串的长度: 也即是将原来的 len 更新位 newlen
*/
static inline void sdssetlen(sds s, size_t newlen)
{
    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK)
    {
    case SDS_TYPE_5:
    {
         /* fp 是 sdshdr5 的 flags 的指针 */
        unsigned char *fp = ((unsigned char *)s) - 1;
        /* 把 newlen 右移 SDS_TYPE_BITS 位再和 SDS_TYPE_5 合成即可 */
        *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS); 
    }
    break;
    case SDS_TYPE_8:
        //直接通过 header 设置 newlen ？？万一 newline 很长，大于 alloc 怎么吧？那不是越界了
        SDS_HDR(8, s)->len = newlen; 
        break;
    case SDS_TYPE_16:
        SDS_HDR(16, s)->len = newlen;
        break;
    case SDS_TYPE_32:
        SDS_HDR(32, s)->len = newlen;
        break;
    case SDS_TYPE_64:
        SDS_HDR(64, s)->len = newlen;
        break;
    }
}

/*
    增加 sds 的长度，增加 inc 字节
*/
static inline void sdsinclen(sds s, size_t inc)
{
    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK)
    {
    case SDS_TYPE_5:
    {
        /* fp 是 sds 的 flags 指针*/
        unsigned char *fp = ((unsigned char *)s) - 1;
        /* 计算新长度 newline */
        unsigned char newlen = SDS_TYPE_5_LEN(flags) + inc;
        /* 设置 长度 */
        *fp = SDS_TYPE_5 | (newlen << SDS_TYPE_BITS);
    }
    break;
    case SDS_TYPE_8:
        /* 直接通过 header 来 访问 len 并 增加 inc */
        SDS_HDR(8, s)->len += inc;
        break;
    case SDS_TYPE_16:
        SDS_HDR(16, s)->len += inc;
        break;
    case SDS_TYPE_32:
        SDS_HDR(32, s)->len += inc;
        break;
    case SDS_TYPE_64:
        SDS_HDR(64, s)->len += inc;
        break;
    }
}

/**
 * 返回sds的已分配内存大小 包括 未使用空间 + 已使用空间
*/
/* sdsalloc() = sdsavail() + sdslen() */
static inline size_t sdsalloc(const sds s)
{
    unsigned char flags = s[-1]; /* 获取 flags */
    switch (flags & SDS_TYPE_MASK) // 获取 sds 的类型
    {
    case SDS_TYPE_5: 
        /* 若是类型，则返回 flags的长度 */
        return SDS_TYPE_5_LEN(flags);
    case SDS_TYPE_8:
        /* 其他类型，则直接返回 提前分配的 alloc 大小*/
        return SDS_HDR(8, s)->alloc;
    case SDS_TYPE_16:
        return SDS_HDR(16, s)->alloc;
    case SDS_TYPE_32:
        return SDS_HDR(32, s)->alloc;
    case SDS_TYPE_64:
        return SDS_HDR(64, s)->alloc;
    }
    return 0;
}

/*
 * 设置 sds 的容量大小 alloc 
*/
static inline void sdssetalloc(sds s, size_t newlen)
{
    unsigned char flags = s[-1];
    switch (flags & SDS_TYPE_MASK)
    {
    case SDS_TYPE_5:
        /* Nothing to do, this type has no total allocation info. */
        break;
    /* 其余四种类型就直接通过 header 设置 alloc 属性*/
    case SDS_TYPE_8:
        SDS_HDR(8, s)->alloc = newlen;
        break;
    case SDS_TYPE_16:
        SDS_HDR(16, s)->alloc = newlen;
        break;
    case SDS_TYPE_32:
        SDS_HDR(32, s)->alloc = newlen;
        break;
    case SDS_TYPE_64:
        SDS_HDR(64, s)->alloc = newlen;
        break;
    }
}

/* 创建一个长度为 initlen 的sds，使用 init 指向的字符数组来初始化数据 */
sds sdsnewlen(const void *init, size_t initlen);

/* 内部调用sdsnewlen，创建一个 sds */
sds sdsnew(const char *init);

/*  创建一个空的 sds */
sds sdsempty(void);

/*  拷贝一个 sds 并 返回这个拷贝 */
sds sdsdup(const sds s);

/*  释放一个 sds */
void sdsfree(sds s);

/* 使一个 sds 的长度增长到一个指定的值，末尾未使用的空间用 0 填充 */
sds sdsgrowzero(sds s, size_t len);

/*  连接一个 sds 和一个二进制安全的数据 t ，t 的长度为 len*/
sds sdscatlen(sds s, const void *t, size_t len);

/* 连接一个 sds 和一个二进制安全的数据 t，内部调用 sdscatlen */
sds sdscat(sds s, const char *t);  

/* 连接两个 sds */
sds sdscatsds(sds s, const sds t); 

/* 把二进制安全的数据 t 复制到一个sds的内存中，覆盖原来的字符串，t 的长度为 len */
sds sdscpylen(sds s, const char *t, size_t len); 

/* 把二进制安全的数据t复制到一个 sds 的内存中，覆盖原来的字符串，内部调用 sdscpylen */ 
sds sdscpy(sds s, const char *t);  

/* 通过 fmt 指定个格式来格式化字符串 */
sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
sds sdscatprintf(sds s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char *fmt, ...);
#endif

/* 将格式化后的任意数量个字符串追加到 s 的末尾 */
sds sdscatfmt(sds s, char const *fmt, ...);

/* 删除 sds 两端由 cset 指定的字符*/
sds sdstrim(sds s, const char *cset);

/*  截取 s 区间[start, end]字符串 */
void sdsrange(sds s, ssize_t start, ssize_t end);

/* 根据字符串占用的空间来更新 len */
void sdsupdatelen(sds s);

/* 把字符串的第一个字符设置为 '\0'，把字符串设置为空字符串，但是并不释放内存(惰性释放) */
void sdsclear(sds s);

/* 比较两个 sds 的是否相等 */
int sdscmp(const sds s1, const sds s2);

/* 使用分隔符 sep 对 s 进行分割，返回一个 sds 数组 */
sds *sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count);

/* 释放 sds 数组 中的 count 个 sds */
void sdsfreesplitres(sds *tokens, int count);

/* 将 sds 所有字符转换为小写*/
void sdstolower(sds s);

/* 将 sds 所有字符转换为大写 */
void sdstoupper(sds s);

/* 将长整型转换为字符串 */
sds sdsfromlonglong(long long value);

/* 将长度为 len 的字符串 p 以带引号的格式追加到 s 的末尾 */
sds sdscatrepr(sds s, const char *p, size_t len);

/* 将一行文本分割成多个参数，参数的个数存在 argc */
sds *sdssplitargs(const char *line, int *argc);

/* 将字符串 s 中，出现存在 from 中指定的字符，都转换成 to 中的字符，from 与 to 有位置关系 */
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);

/* 使用分隔符 sep 将字符数组 argv 拼接成一个字符串 */
sds sdsjoin(char **argv, int argc, char *sep);

/* sdsjoin类似，不过拼接的是一个 sds 数组 */
sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen);


/* Low level functions exposed to the user API */
/* 暴露出来作为用户 API 的低级函数 */

/* 为指定的 sds 扩充大小，扩充的大小为 addlen */
sds sdsMakeRoomFor(sds s, size_t addlen);

/* 根据 incr 增加或减少 sds 的字符串长度  */
void sdsIncrLen(sds s, ssize_t incr);

/* 移除一个 sds 的空闲空间 */
sds sdsRemoveFreeSpace(sds s);

/* 获取一个 sds 的总大小（包括h eader、字符串、末尾的空闲空间和隐式项目） */
size_t sdsAllocSize(sds s);

/* 获取一个 sds 确切的内存空间的指针（一般的sds引用都是一个指向其字符串的指针） */
void *sdsAllocPtr(sds s);




/* Export the allocator used by SDS to the program using SDS.
 * Sometimes the program SDS is linked to, may use a different set of
 * allocators, but may want to allocate or free things that SDS will
 * respectively free or allocate. */
/* 导出供外部程序调用的sds的分配/释放函数 */

/* sds 分配器的包装函数，内部调用 s_malloc */
void *sds_malloc(size_t size);

/* sds 分配器的包装函数，内部调用 s_realloc */
void *sds_realloc(void *ptr, size_t size);

/* sds 释放器的包装函数，内部调用 s_free */
void sds_free(void *ptr);

#ifdef REDIS_TEST
int sdsTest(int argc, char *argv[]);
#endif

#endif
