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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>
#include "sds.h"
#include "sdsalloc.h"

const char *SDS_NOINIT = "SDS_NOINIT";

/*
 * sds header 的 头部大小  
*/
static inline int sdsHdrSize(char type)
{
    switch (type & SDS_TYPE_MASK)
    {
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

static inline char sdsReqType(size_t string_size)
{
    if (string_size < 1 << 5) // 32
        return SDS_TYPE_5;
    if (string_size < 1 << 8) // 512
        return SDS_TYPE_8;
    if (string_size < 1 << 16) //65535
        return SDS_TYPE_16;
#if (LONG_MAX == LLONG_MAX)
    if (string_size < 1ll << 32)
        return SDS_TYPE_32;
    return SDS_TYPE_64;
#else
    return SDS_TYPE_32;
#endif
}

static inline size_t sdsTypeMaxSize(char type)
{
    if (type == SDS_TYPE_5)
        return (1 << 5) - 1;
    if (type == SDS_TYPE_8)
        return (1 << 8) - 1;
    if (type == SDS_TYPE_16)
        return (1 << 16) - 1;
#if (LONG_MAX == LLONG_MAX)
    if (type == SDS_TYPE_32)
        return (1ll << 32) - 1;
#endif
    return -1; /* this is equivalent to the max SDS_TYPE_64 or SDS_TYPE_32 */
}

/* Create a new sds string with the content specified by the 'init' pointer
 * and 'initlen'.
 * If NULL is used for 'init' the string is initialized with zero bytes.
 * If SDS_NOINIT is used, the buffer is left uninitialized;
 *
 * The string is always null-termined (all the sds strings are, always) so
 * even if you create an sds string with:
 *
 * mystring = sdsnewlen("abc",3);
 *
 * You can print the string with printf() as there is an implicit \0 at the
 * end of the string. However the string is binary safe and can contain
 * \0 characters in the middle, as the length is stored in the sds header. */
/*
根据给定 init 和 字符串长度 initlen 创建一个新的sds
参数 ：
    init 初始化字符串指针
    initlen 初始化字符串的长度
返回值：
    成功返回 sds 字符串
    失败返回 NULL
复杂度 ： T = O(N)
*/
sds sdsnewlen(const void *init, size_t initlen)
{
    void *sh;
    sds s;
    char type = sdsReqType(initlen);
    /* Empty strings are usually created in order to append. Use type 8
     * since type 5 is not good at this. */
    if (type == SDS_TYPE_5 && initlen == 0)
        type = SDS_TYPE_8;
    int hdrlen = sdsHdrSize(type);
    unsigned char *fp; /* flags pointer. */
    size_t usable;

    sh = s_malloc_usable(hdrlen + initlen + 1, &usable);
    if (sh == NULL)
        return NULL;
    if (init == SDS_NOINIT)
        init = NULL;
    else if (!init)
        memset(sh, 0, hdrlen + initlen + 1);
    s = (char *)sh + hdrlen;
    fp = ((unsigned char *)s) - 1;
    usable = usable - hdrlen - 1;
    if (usable > sdsTypeMaxSize(type))
        usable = sdsTypeMaxSize(type);
    switch (type)
    {
    case SDS_TYPE_5:
    {
        *fp = type | (initlen << SDS_TYPE_BITS);
        break;
    }
    case SDS_TYPE_8:
    {
        SDS_HDR_VAR(8, s);
        sh->len = initlen;
        sh->alloc = usable;
        *fp = type;
        break;
    }
    case SDS_TYPE_16:
    {
        SDS_HDR_VAR(16, s);
        sh->len = initlen;
        sh->alloc = usable;
        *fp = type;
        break;
    }
    case SDS_TYPE_32:
    {
        SDS_HDR_VAR(32, s);
        sh->len = initlen;
        sh->alloc = usable;
        *fp = type;
        break;
    }
    case SDS_TYPE_64:
    {
        SDS_HDR_VAR(64, s);
        sh->len = initlen;
        sh->alloc = usable;
        *fp = type;
        break;
    }
    }
    if (initlen && init)
        memcpy(s, init, initlen);
    s[initlen] = '\0';
    return s;
}

/* Create an empty (zero length) sds string. Even in this case the string
 * always has an implicit null term. */
/*
创建并返回一个空字符串 "" 的 sds
返回值 ：  
    成功返回空的 sds
    失败返回NULL
*/
sds sdsempty(void)
{
    return sdsnewlen("", 0);
}

/* Create a new sds string starting from a null terminated C string. */
/*
 * 创建一个给定字符串 init 的副本并返回
 * 返回值：
 *  成功返回对应的 sds
 *  失败返回 NULL
 * 时间复杂度 T = O(N) 
*/
sds sdsnew(const char *init)
{
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/* Duplicate an sds string. */
/*
 * 复制 sds字符串,返回副本
 */
sds sdsdup(const sds s)
{
    return sdsnewlen(s, sdslen(s));
}

/* Free an sds string. No operation is performed if 's' is NULL. */
/*
释放 给定的 sds
*/
void sdsfree(sds s)
{
    if (s == NULL)
        return;
    s_free((char *)s - sdsHdrSize(s[-1]));
}

/* Set the sds string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character.
 *
 * This function is useful when the sds string is hacked manually in some
 * way, like in the following example:
 *
 * s = sdsnew("foobar");
 * s[2] = '\0';
 * sdsupdatelen(s);
 * printf("%d\n", sdslen(s));
 *
 * The output will be "2", but if we comment out the call to sdsupdatelen()
 * the output will be "6" as the string was modified but the logical length
 * remains 6 bytes. */
void sdsupdatelen(sds s)
{
    size_t reallen = strlen(s);
    sdssetlen(s, reallen);
}

/* Modify an sds string in-place to make it empty (zero length).
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available. */
/*
在不释放 sds 字符串空间的情况下，重置 sds 所保存的字符串为空
*/
void sdsclear(sds s)
{
    sdssetlen(s, 0);
    s[0] = '\0'; //将结束符放到最前面，相当于惰性地删除 buf 中的内容
}

/* Enlarge the free space at the end of the sds string so that the caller
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for nul term.
 *
 * Note: this does not change the *length* of the sds string as returned
 * by sdslen(), but only the free buffer space we have. */
/* 
 * 对 sds 中 buf 的长度进行扩展， 确保在函数执行之后 buf 至少会有 addlen + 1的额外长度
 * +1 是为 \0 准备的，
*/
sds sdsMakeRoomFor(sds s, size_t addlen)
{
    void *sh, *newsh;
    size_t avail = sdsavail(s); //获取 s 目前的空闲空间长度
    size_t len, newlen;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen;
    size_t usable;

    /* Return ASAP if there is enough space left. */
    // s 的空闲空间主钩，无序扩展，直接返回。
    if (avail >= addlen)
        return s;

    len = sdslen(s); //获取 s 目前已用的空间长度
    sh = (char *)s - sdsHdrSize(oldtype);
    newlen = (len + addlen); // s 需要的至少长度
    if (newlen < SDS_MAX_PREALLOC) //若新长度小于 SDS_MAX_PREALLOC，则为它分配两倍所需长度
        newlen *= 2;
    else //否则，分配长度为目前所需长度加上 SDS_MAX_PREALLOC (1M)
        newlen += SDS_MAX_PREALLOC;

    type = sdsReqType(newlen);

    /* Don't use type 5: the user is appending to the string and type 5 is
     * not able to remember empty space, so sdsMakeRoomFor() must be called
     * at every appending operation. */
    if (type == SDS_TYPE_5)
        type = SDS_TYPE_8;

    hdrlen = sdsHdrSize(type);
    if (oldtype == type)//若不需要更换header，直接在元空地址上重新分配空间
    {
        /*
        s_realloc_usable的具体实现得看Redis编译的时候选用了哪个allocator（在Linux上默认使用jemalloc）。但不管是哪个realloc的实现，
        它所表达的含义基本是相同的：它尽量在原来分配好的地址位置重新分配，如果原来的地址位置有足够的空余空间完成重新分配，
        那么它返回的新地址与传入的旧地址相同；否则，
        */
        newsh = s_realloc_usable(sh, hdrlen + newlen + 1, &usable);
        if (newsh == NULL)//内存不足，分配失败 返回
            return NULL;
        s = (char *)newsh + hdrlen;
    }
    else //需要更换header
    {
        /* Since the header size changes, need to move the string forward,
         * and can't use realloc */
        newsh = s_malloc_usable(hdrlen + newlen + 1, &usable); //重新分配（s_malloc_usable）空间
        if (newsh == NULL)//内存不足，分配失败 返回
            return NULL;
        memcpy((char *)newsh + hdrlen, s, len + 1);//并拷贝原来的数据到新的位置。
        s_free(sh);
        s = (char *)newsh + hdrlen;
        s[-1] = type;
        sdssetlen(s, len);
    }
    usable = usable - hdrlen - 1; 
    if (usable > sdsTypeMaxSize(type))
        usable = sdsTypeMaxSize(type);
    sdssetalloc(s, usable);
    return s;
}

/* Reallocate the sds string so that it has no free space at the end. The
 * contained string remains not altered, but next concatenation operations
 * will require a reallocation.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/*
 * 回收 sds 中未使用的空闲空间，已用的空间不做改变。 
 * 成功返回 sds
 * 失败返回 0
 * 时间复杂度 O(n)
*/
sds sdsRemoveFreeSpace(sds s)
{
    void *sh, *newsh;
    char type, oldtype = s[-1] & SDS_TYPE_MASK;
    int hdrlen, oldhdrlen = sdsHdrSize(oldtype);
    size_t len = sdslen(s);
    size_t avail = sdsavail(s);
    sh = (char *)s - oldhdrlen;

    /* Return ASAP if there is no space left. */
    //如空闲空间为0，则直接返回
    if (avail == 0)
        return s;

    /* Check what would be the minimum SDS header that is just good enough to
     * fit this string. */
    type = sdsReqType(len);
    hdrlen = sdsHdrSize(type);

    /* If the type is the same, or at least a large enough type is still
     * required, we just realloc(), letting the allocator to do the copy
     * only if really needed. Otherwise if the change is huge, we manually
     * reallocate the string to use the different header type. */
    if (oldtype == type || type > SDS_TYPE_8)
    {
        newsh = s_realloc(sh, oldhdrlen + len + 1); //申请分配内存
        if (newsh == NULL)
            return NULL;
        s = (char *)newsh + oldhdrlen;
    }
    else
    {
        newsh = s_malloc(hdrlen + len + 1);
        if (newsh == NULL)
            return NULL;
        memcpy((char *)newsh + hdrlen, s, len + 1);
        s_free(sh);//释放原来的空间
        s = (char *)newsh + hdrlen;
        s[-1] = type;
        sdssetlen(s, len); //重新设置len
    }
    sdssetalloc(s, len);
    return s;
}

/* Return the total size of the allocation of the specified sds string,
 * including:
 * 1) The sds header before the pointer.
 * 2) The string.
 * 3) The free buffer at the end if any.
 * 4) The implicit null term.
 */
/*
 * 返回给定的 sds 的已分配的总的内存
 * 包括:
 * 1) sds 头部
 * 2) 字符串
 * 3) 空buffer
 * 4）隐士的结束标识符\0 
*/
size_t sdsAllocSize(sds s)
{ 
    size_t alloc = sdsalloc(s); //获取 sds header有alloc
    return sdsHdrSize(s[-1]) + alloc + 1; //header大小 + alloc(字符串大小+空闲大小) + 1(结束标识符\0)
}

/* Return the pointer of the actual SDS allocation (normally SDS strings
 * are referenced by the start of the string buffer). */
/**
 * 返回 sds 分配的实际分配的内存首地址，一般sds引用都是指向一个buffer字符串的指针
*/
void *sdsAllocPtr(sds s)
{
    return (void *)(s - sdsHdrSize(s[-1])); //字符串缓冲区的首地址 - header的大小
}

/* Increment the sds length and decrements the left free space at the
 * end of the string according to 'incr'. Also set the null term
 * in the new end of the string.
 *
 * This function is used in order to fix the string length after the
 * user calls sdsMakeRoomFor(), writes something after the end of
 * the current string, and finally needs to set the new length.
 *
 * Note: it is possible to use a negative increment in order to
 * right-trim the string.
 *
 * Usage example:
 *
 * Using sdsIncrLen() and sdsMakeRoomFor() it is possible to mount the
 * following schema, to cat bytes coming from the kernel to the end of an
 * sds string without copying into an intermediate buffer:
 *
 * oldlen = sdslen(s);
 * s = sdsMakeRoomFor(s, BUFFER_SIZE);
 * nread = read(fd, s+oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * sdsIncrLen(s, nread);
 */
/*
 * 增加sds字符串的长度或减少剩余空闲空间的大小。同时也将在新字符串的末尾设置终止符。
 * 用来修正调用sdsMakeRoomFor()函数之后字符串的长度，在当前字符串后追加数据这些需要设置字符串新长度
 * 的操作之后。
 * 注意：可以使用一个负的增量值来右对齐字符串。使用sdsIncrLen()和sdsMakeRoomFor()函数可以用来满足
 * 如下模式：从内核中直接复制一部分字节到一个sds字符串的末尾，且无须把数据先复制到一个中间缓冲区中
 */
void sdsIncrLen(sds s, ssize_t incr)
{
    unsigned char flags = s[-1];
    size_t len;
    switch (flags & SDS_TYPE_MASK)
    {
    case SDS_TYPE_5:
    {
        unsigned char *fp = ((unsigned char *)s) - 1;
        unsigned char oldlen = SDS_TYPE_5_LEN(flags);
        assert((incr > 0 && oldlen + incr < 32) || (incr < 0 && oldlen >= (unsigned int)(-incr)));
        *fp = SDS_TYPE_5 | ((oldlen + incr) << SDS_TYPE_BITS);
        len = oldlen + incr;
        break;
    }
    case SDS_TYPE_8:
    {
        SDS_HDR_VAR(8, s);
        assert((incr >= 0 && sh->alloc - sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
        len = (sh->len += incr);
        break;
    }
    case SDS_TYPE_16:
    {
        SDS_HDR_VAR(16, s);
        assert((incr >= 0 && sh->alloc - sh->len >= incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
        len = (sh->len += incr);
        break;
    }
    case SDS_TYPE_32:
    {
        SDS_HDR_VAR(32, s);
        /* incr 为正数， 增加指定的长度，需要确保 空闲空间( alloc - len )大于该值
           incr 为负数， 减少指定的长度，需要确保 已使用空间大小 incr 的绝对值*/
        assert((incr >= 0 && sh->alloc - sh->len >= (unsigned int)incr) || (incr < 0 && sh->len >= (unsigned int)(-incr)));
        len = (sh->len += incr);
        break;
    }
    case SDS_TYPE_64:
    {
        SDS_HDR_VAR(64, s);
        assert((incr >= 0 && sh->alloc - sh->len >= (uint64_t)incr) || (incr < 0 && sh->len >= (uint64_t)(-incr)));
        len = (sh->len += incr);
        break;
    }
    default:
        len = 0; /* Just to avoid compilation warnings. */
    }
    s[len] = '\0';
}

/* Grow the sds to have the specified length. Bytes that were not part of
 * the original length of the sds will be set to zero.
 *
 * if the specified length is smaller than the current length, no operation
 * is performed. */
/*
    将 sds 的长度变为指定关长度， 若带扩充的长度小于当前字符串长度，直接饭hi，否则 扩充出来的部分用 0 填充
*/
sds sdsgrowzero(sds s, size_t len)
{
    size_t curlen = sdslen(s); /* 当前字符数组的长度 */

    if (len <= curlen) /* 设置的长度小于当前 sds 的长度，则直接返回*/
        return s;
    s = sdsMakeRoomFor(s, len - curlen); /* 扩容 */
    if (s == NULL)  /* 扩容失败 *
        return NULL;

    /* Make sure added region doesn't contain garbage */
    /* 确保新增的区域不包含垃圾数据 ，将新增的部分设置为 0 */
    memset(s + curlen, 0, (len - curlen + 1)); /* also set trailing \0 byte */
    sdssetlen(s, len);/* 更新 sds 字符串 header 中的 len */
    return s;
}

/* Append the specified binary-safe string pointed by 't' of 'len' bytes to the
 * end of the specified sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/* 
    向指定的 sds 字符串的尾部追加由二级制安全的长度为 t 的字符串 len 
    调用此函数后，调用此函数后，原来作为参数传入的sds字符串的指针不再是有效的，
    所有引用必须被替换为函数返回的新指针。
*/
sds sdscatlen(sds s, const void *t, size_t len)
{
    size_t curlen = sdslen(s); /* 当前字符串的长度 */

    s = sdsMakeRoomFor(s, len); /* 扩充 len 字节 */
    if (s == NULL) /* 扩容失败 */
        return NULL;
    memcpy(s + curlen, t, len); /* 将字符串 追加到原字符串末尾 */
    sdssetlen(s, curlen + len); /* 更新 sds 字符串 header 中的 len */
    s[curlen + len] = '\0'; /* 设置终止字符 */
    return s;
}

/* Append the specified null terminated C string to the sds string 's'.
 *
 * After the call, the passed sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/*
    类似 C 语言原生字符串的 cat 函数，拼接两个 sds 字符串
    追加指定的C字符串到sds字符串’s’的尾部。调用此函数后，原来作为参数传入的sds字符串的指针不再是有效的，
    所有引用必须被替换为函数返回的新指针。
*/
sds sdscat(sds s, const char *t)
{
    return sdscatlen(s, t, strlen(t));
}

/* Append the specified sds 't' to the existing sds 's'.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/*
 * 与 sdscat 作用类似
 * 追加指定的 sds 字符串 t 到已经存在的 sds 字符串 s 末尾。调用此函数后，
 * 原来作为参数传入的sds字符串的指针不再是有效的，所有引用必须被替换为函数返回的新指针。
*/
sds sdscatsds(sds s, const sds t)
{
    return sdscatlen(s, t, sdslen(t));
}

/* Destructively modify the sds string 's' to hold the specified binary
 * safe string pointed by 't' of length 'len' bytes. */
/* 将 t 指向的长度为 len 为字符串 拷贝到到 sds 中的字符串，会改变 sds 原来的字符串。 */
sds sdscpylen(sds s, const char *t, size_t len)
{
    if (sdsalloc(s) < len)
    {
        s = sdsMakeRoomFor(s, len - sdslen(s));/* 扩容 */
        if (s == NULL)
            return NULL;
    }
    memcpy(s, t, len); /* 拷贝 t 指向的长度为 len 的字符串*/
    s[len] = '\0';
    sdssetlen(s, len); /* 设置 sds 的长度 */
    return s;
}

/* Like sdscpylen() but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen(). */
/* 和 sdscpylen类似， 但是 t 指向的是一个以 \0 结尾的字符串 ，因此可用用 strlen()获取该字符串长度 */
sds sdscpy(sds s, const char *t)
{
    return sdscpylen(s, t, strlen(t));
}

/* Helper for sdscatlonglong() doing the actual number -> string
 * conversion. 's' must point to a string with room for at least
 * SDS_LLSTR_SIZE bytes.
 *
 * The function returns the length of the null-terminated string
 * representation stored at 's'. */
/* sdscatlonglong（）的帮助程序执行实际数字->字符串转换。 
 * 's'必须指向一个至少有SDS_LLSTR_SIZE字节空间的字符串。 该函数返回存储在“ s”处的以空值终止的字符串表示形式
 * 的长度
 */
#define SDS_LLSTR_SIZE 21
/* 将类型为 long logn 数转为 字符串 并返回转为字符串后的其长度 */
int sdsll2str(char *s, long long value)
{
    char *p, aux;
    unsigned long long v;
    size_t l;

    /* Generate the string representation, this method produces
     * a reversed string. */
    v = (value < 0) ? -value : value;
    p = s;
    do
    {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while (v);
    if (value < 0)
        *p++ = '-';

    /* Compute length and add null term. */
    /* 计算长度 并 添加 结束标识符\0 */
    l = p - s;
    *p = '\0';

    /* Reverse the string. */
    /* 翻转字符串 */
    p--;
    while (s < p)
    {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Identical sdsll2str(), but for unsigned long long type. */
/* 与 sdsll2str()类似，只不过用于 无符号长整型 unsigned long long 类型·*/
int sdsull2str(char *s, unsigned long long v)
{
    char *p, aux;
    size_t l;

    /* Generate the string representation, this method produces
     * a reversed string. */
    p = s;
    do
    {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while (v);

    /* Compute length and add null term. */
    l = p - s;
    *p = '\0';

    /* Reverse the string. */
    p--;
    while (s < p)
    {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/* Create an sds string from a long long value. It is much faster than:
 *
 * sdscatprintf(sdsempty(),"%lld\n", value);
 */
/* 将 输入的 long long 数转为 sds 类型 */
sds sdsfromlonglong(long long value)
{
    char buf[SDS_LLSTR_SIZE];
    int len = sdsll2str(buf, value);

    return sdsnewlen(buf, len);
}

/* Like sdscatprintf() but gets va_list instead of being variadic. */
/* 像 sdscatprintf（）一样，但是获取 va_list 而不是可变参数。 */
sds sdscatvprintf(sds s, const char *fmt, va_list ap)
{
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    size_t buflen = strlen(fmt) * 2;

    /* We try to start using a static buffer for speed.
     * If not possible we revert to heap allocation. */
    if (buflen > sizeof(staticbuf))
    {
        buf = s_malloc(buflen);
        if (buf == NULL)
            return NULL;
    }
    else
    {
        buflen = sizeof(staticbuf);
    }

    /* Try with buffers two times bigger every time we fail to
     * fit the string in the current buffer size. */
    while (1)
    {
        buf[buflen - 2] = '\0';
        va_copy(cpy, ap);
        vsnprintf(buf, buflen, fmt, cpy);
        va_end(cpy);
        if (buf[buflen - 2] != '\0')
        {
            if (buf != staticbuf)
                s_free(buf);
            buflen *= 2;
            buf = s_malloc(buflen);
            if (buf == NULL)
                return NULL;
            continue;
        }
        break;
    }

    /* Finally concat the obtained string to the SDS string and return it. */
    t = sdscat(s, buf);
    if (buf != staticbuf)
        s_free(buf);
    return t;
}

/* Append to the sds string 's' a string obtained using printf-alike format
 * specifier.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sdsnew("Sum is: ");
 * s = sdscatprintf(s,"%d+%d = %d",a,b,a+b).
 *
 * Often you need to create a string from scratch with the printf-alike
 * format. When this is the need, just use sdsempty() as the target string:
 *
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
 */
sds sdscatprintf(sds s, const char *fmt, ...)
{
    va_list ap;
    char *t;
    va_start(ap, fmt);
    t = sdscatvprintf(s, fmt, ap);
    va_end(ap);
    return t;
}

/* This function is similar to sdscatprintf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that
 * are often very slow. Moreover directly handling the sds string as
 * new data is concatenated provides a performance improvement.
 *
 * However this function only handles an incompatible subset of printf-alike
 * format specifiers:
 *
 * %s - C String
 * %S - SDS string
 * %i - signed int
 * %I - 64 bit signed integer (long long, int64_t)
 * %u - unsigned int
 * %U - 64 bit unsigned integer (unsigned long long, uint64_t)
 * %% - Verbatim "%" character.
 */
sds sdscatfmt(sds s, char const *fmt, ...)
{
    size_t initlen = sdslen(s);
    const char *f = fmt;
    long i;
    va_list ap;

    /* To avoid continuous reallocations, let's start with a buffer that
     * can hold at least two times the format string itself. It's not the
     * best heuristic but seems to work in practice. */
    s = sdsMakeRoomFor(s, initlen + strlen(fmt) * 2);
    va_start(ap, fmt);
    f = fmt;     /* Next format specifier byte to process. */
    i = initlen; /* Position of the next byte to write to dest str. */
    while (*f)
    {
        char next, *str;
        size_t l;
        long long num;
        unsigned long long unum;

        /* Make sure there is always space for at least 1 char. */
        if (sdsavail(s) == 0)
        {
            s = sdsMakeRoomFor(s, 1);
        }

        switch (*f)
        {
        case '%':
            next = *(f + 1);
            f++;
            switch (next)
            {
            case 's':
            case 'S':
                str = va_arg(ap, char *);
                l = (next == 's') ? strlen(str) : sdslen(str);
                if (sdsavail(s) < l)
                {
                    s = sdsMakeRoomFor(s, l);
                }
                memcpy(s + i, str, l);
                sdsinclen(s, l);
                i += l;
                break;
            case 'i':
            case 'I':
                if (next == 'i')
                    num = va_arg(ap, int);
                else
                    num = va_arg(ap, long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsll2str(buf, num);
                    if (sdsavail(s) < l)
                    {
                        s = sdsMakeRoomFor(s, l);
                    }
                    memcpy(s + i, buf, l);
                    sdsinclen(s, l);
                    i += l;
                }
                break;
            case 'u':
            case 'U':
                if (next == 'u')
                    unum = va_arg(ap, unsigned int);
                else
                    unum = va_arg(ap, unsigned long long);
                {
                    char buf[SDS_LLSTR_SIZE];
                    l = sdsull2str(buf, unum);
                    if (sdsavail(s) < l)
                    {
                        s = sdsMakeRoomFor(s, l);
                    }
                    memcpy(s + i, buf, l);
                    sdsinclen(s, l);
                    i += l;
                }
                break;
            default: /* Handle %% and generally %<unknown>. */
                s[i++] = next;
                sdsinclen(s, 1);
                break;
            }
            break;
        default:
            s[i++] = *f;
            sdsinclen(s, 1);
            break;
        }
        f++;
    }
    va_end(ap);

    /* Add null-term */
    s[i] = '\0';
    return s;
}

/* Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string.
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call.
 *
 * Example:
 *
 * s = sdsnew("AA...AA.a.aa.aHelloWorld     :::");
 * s = sdstrim(s,"Aa. :");
 * printf("%s\n", s);
 *
 * Output will be just "HelloWorld".
 */
/**
 * 从 sds 字符串左右两边移除 cset 包含是字符
 * 时间复杂度 T = O(M*N), M 为 sds 的长度， n 是cest的长度
*/
sds sdstrim(sds s, const char *cset)
{
    char *start, *end, *sp, *ep;
    size_t len;

    sp = start = s;
    ep = end = s + sdslen(s) - 1;

    /* 修剪, T = O(N^2) */
    while (sp <= end && strchr(cset, *sp))
        sp++;
    while (ep > sp && strchr(cset, *ep))
        ep--;

    /* 计算 trim 完毕之后剩余的字符串长度 */
    len = (sp > ep) ? 0 : ((ep - sp) + 1);
    if (s != sp)
        memmove(s, sp, len);

    s[len] = '\0'; /* 添加终止符\0 */
    sdssetlen(s, len); //更新 len 
    return s;
}

/* Turn the string into a smaller (or equal) string containing only the
 * substring specified by the 'start' and 'end' indexes.
 *
 * start and end can be negative, where -1 means the last character of the
 * string, -2 the penultimate character, and so forth.
 *
 * The interval is inclusive, so the start and end characters will be part
 * of the resulting string.
 *
 * The string is modified in-place.
 *
 * Example:
 *
 * s = sdsnew("Hello World");
 * sdsrange(s,1,-1); => "ello World"
 */
 /*
 * 对 sds 进行截取 索引从 0 开始， 也可以是负数， -1 表示最后一个字符
 * 时间复制度为 O(n)
 * 会对传入的字符串改变
 */
void sdsrange(sds s, ssize_t start, ssize_t end)
{
    size_t newlen, len = sdslen(s); // 获取 当前 sds 字符串 s 的长度

    if (len == 0)
        return;
    if (start < 0) // 负数，-1 表示最后一个字符
    {
        start = len + start; 
        if (start < 0) //若 负数的绝对值大于整个正常，则从 0 开始
            start = 0;
    }
    if (end < 0)
    {
        end = len + end;
        if (end < 0)
            end = 0;
    }
    newlen = (start > end) ? 0 : (end - start) + 1;
    if (newlen != 0)
    {
        if (start >= (ssize_t)len)
        {
            newlen = 0;
        }
        else if (end >= (ssize_t)len)
        {
            end = len - 1;
            newlen = (start > end) ? 0 : (end - start) + 1;
        }
    }
    /* 拷贝指定长度的字符*/
    if (start && newlen)
        memmove(s, s + start, newlen); 
    s[newlen] = 0;
    sdssetlen(s, newlen);
}


/* Apply tolower() to every character of the sds string 's'. */
/* 使用 tolower() 函数将 sds 字符串 全部转为小写*/
void sdstolower(sds s)
{
    size_t len = sdslen(s), j;

    for (j = 0; j < len; j++)
        s[j] = tolower(s[j]);
}

/* Apply toupper() to every character of the sds string 's'. */
/* 使用 toupper() 函数将 sds 字符串 全部转为大写*/
void sdstoupper(sds s)
{
    size_t len = sdslen(s), j;

    for (j = 0; j < len; j++)
        s[j] = toupper(s[j]);
}

/* Compare two sds strings s1 and s2 with memcmp().
 *
 * Return value:
 *
 *     positive if s1 > s2.
 *     negative if s1 < s2.
 *     0 if s1 and s2 are exactly the same binary string.
 *
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater than
 * the smaller one. */
 /**
 * 使用 memcmp() 函数比较两个 sds 字符串 s1 和 s2
 * 返回值 :
    正数 s1 > s2
    负数 s1 < s2
    0    s1 == s2
    若两个字符串有相同的前缀，则常的字符串比短的字符串大
 */
int sdscmp(const sds s1, const sds s2)
{
    size_t l1, l2, minlen;
    int cmp;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1, s2, minlen);
    if (cmp == 0)
        return l1 > l2 ? 1 : (l1 < l2 ? -1 : 0);
    return cmp;
}

/* Split 's' with separator in 'sep'. An array
 * of sds strings is returned. *count will be set
 * by reference to the number of tokens returned.
 *
 * On out of memory, zero length string, zero length
 * separator, NULL is returned.
 *
 * Note that 'sep' is able to split a string using
 * a multi-character separator. For example
 * sdssplit("foo_-_bar","_-_"); will return two
 * elements "foo" and "bar".
 *
 * This version of the function is binary-safe but
 * requires length arguments. sdssplit() is just the
 * same function but for zero-terminated strings.
 */
 
/*
 *  使用分隔符 sep 对 s 进行分割，返回一个 sds 数组
 */
sds *sdssplitlen(const char *s, ssize_t len, const char *sep, int seplen, int *count)
{
    int elements = 0, slots = 5;
    long start = 0, j;
    sds *tokens;

    if (seplen < 1 || len < 0)
        return NULL;

    tokens = s_malloc(sizeof(sds) * slots);
    if (tokens == NULL)
        return NULL;

    if (len == 0)
    {
        *count = 0;
        return tokens;
    }
    for (j = 0; j < (len - (seplen - 1)); j++)
    {
        /* make sure there is room for the next element and the final one */
        if (slots < elements + 2)
        {
            sds *newtokens;

            slots *= 2;
            newtokens = s_realloc(tokens, sizeof(sds) * slots);
            if (newtokens == NULL)
                goto cleanup;
            tokens = newtokens;
        }
        /* search the separator */
        if ((seplen == 1 && *(s + j) == sep[0]) || (memcmp(s + j, sep, seplen) == 0))
        {
            tokens[elements] = sdsnewlen(s + start, j - start);
            if (tokens[elements] == NULL)
                goto cleanup;
            elements++;
            start = j + seplen;
            j = j + seplen - 1; /* skip the separator */
        }
    }
    /* Add the final element. We are sure there is room in the tokens array. */
    tokens[elements] = sdsnewlen(s + start, len - start);
    if (tokens[elements] == NULL)
        goto cleanup;
    elements++;
    *count = elements;
    return tokens;

cleanup:
{
    int i;
    for (i = 0; i < elements; i++)
        sdsfree(tokens[i]);
    s_free(tokens);
    *count = 0;
    return NULL;
}
}

/* Free the result returned by sdssplitlen(), or do nothing if 'tokens' is NULL. */
/* 释放 sds 数组 中的 count 个 sds */ 
void sdsfreesplitres(sds *tokens, int count)
{
    if (!tokens)
        return;
    while (count--)
        sdsfree(tokens[count]);
    s_free(tokens);
}

/* Append to the sds string "s" an escaped string representation where
 * all the non-printable characters (tested with isprint()) are turned into
 * escapes in the form "\n\r\a...." or "\x<hex-number>".
 *
 * After the call, the modified sds string is no longer valid and all the
 * references must be substituted with the new pointer returned by the call. */
/* 将长度为 len 的字符串 p 以带引号的格式追加到 s 的末尾 */
sds sdscatrepr(sds s, const char *p, size_t len)
{
    s = sdscatlen(s, "\"", 1);
    while (len--)
    {
        switch (*p)
        {
        case '\\':
        case '"':
            s = sdscatprintf(s, "\\%c", *p);
            break;
        case '\n':
            s = sdscatlen(s, "\\n", 2);
            break;
        case '\r':
            s = sdscatlen(s, "\\r", 2);
            break;
        case '\t':
            s = sdscatlen(s, "\\t", 2);
            break;
        case '\a':
            s = sdscatlen(s, "\\a", 2);
            break;
        case '\b':
            s = sdscatlen(s, "\\b", 2);
            break;
        default:
            if (isprint(*p))
                s = sdscatprintf(s, "%c", *p);
            else
                s = sdscatprintf(s, "\\x%02x", (unsigned char)*p);
            break;
        }
        p++;
    }
    return sdscatlen(s, "\"", 1);
}

/* Helper function for sdssplitargs() that returns non zero if 'c'
 * is a valid hex digit. */
/* 帮助 sdssplitargs() 函数 ，若 字符 c 是 一个有效的十六进制，则返回非 0 值 */
int is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/* Helper function for sdssplitargs() that converts a hex digit into an
 * integer from 0 to 15 */
int hex_digit_to_int(char c)
{
    switch (c)
    {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'a':
    case 'A':
        return 10;
    case 'b':
    case 'B':
        return 11;
    case 'c':
    case 'C':
        return 12;
    case 'd':
    case 'D':
        return 13;
    case 'e':
    case 'E':
        return 14;
    case 'f':
    case 'F':
        return 15;
    default:
        return 0;
    }
}

/* Split a line into arguments, where every argument can be in the
 * following programming-language REPL-alike form:
 *
 * foo bar "newline are supported\n" and "\xff\x00otherstuff"
 *
 * The number of arguments is stored into *argc, and an array
 * of sds is returned.
 *
 * The caller should free the resulting array of sds strings with
 * sdsfreesplitres().
 *
 * Note that sdscatrepr() is able to convert back a string into
 * a quoted string in the same format sdssplitargs() is able to parse.
 *
 * The function returns the allocated tokens on success, even when the
 * input string is empty, or NULL if the input contains unbalanced
 * quotes or closed quotes followed by non space characters
 * as in: "foo"bar or "foo'
 */
sds *sdssplitargs(const char *line, int *argc)
{
    const char *p = line;
    char *current = NULL;
    char **vector = NULL;

    *argc = 0;
    while (1)
    {
        /* skip blanks */
        while (*p && isspace(*p))
            p++;
        if (*p)
        {
            /* get a token */
            int inq = 0;  /* set to 1 if we are in "quotes" */
            int insq = 0; /* set to 1 if we are in 'single quotes' */
            int done = 0;

            if (current == NULL)
                current = sdsempty();
            while (!done)
            {
                if (inq)
                {
                    if (*p == '\\' && *(p + 1) == 'x' &&
                        is_hex_digit(*(p + 2)) &&
                        is_hex_digit(*(p + 3)))
                    {
                        unsigned char byte;

                        byte = (hex_digit_to_int(*(p + 2)) * 16) +
                               hex_digit_to_int(*(p + 3));
                        current = sdscatlen(current, (char *)&byte, 1);
                        p += 3;
                    }
                    else if (*p == '\\' && *(p + 1))
                    {
                        char c;

                        p++;
                        switch (*p)
                        {
                        case 'n':
                            c = '\n';
                            break;
                        case 'r':
                            c = '\r';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        case 'b':
                            c = '\b';
                            break;
                        case 'a':
                            c = '\a';
                            break;
                        default:
                            c = *p;
                            break;
                        }
                        current = sdscatlen(current, &c, 1);
                    }
                    else if (*p == '"')
                    {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p + 1) && !isspace(*(p + 1)))
                            goto err;
                        done = 1;
                    }
                    else if (!*p)
                    {
                        /* unterminated quotes */
                        goto err;
                    }
                    else
                    {
                        current = sdscatlen(current, p, 1);
                    }
                }
                else if (insq)
                {
                    if (*p == '\\' && *(p + 1) == '\'')
                    {
                        p++;
                        current = sdscatlen(current, "'", 1);
                    }
                    else if (*p == '\'')
                    {
                        /* closing quote must be followed by a space or
                         * nothing at all. */
                        if (*(p + 1) && !isspace(*(p + 1)))
                            goto err;
                        done = 1;
                    }
                    else if (!*p)
                    {
                        /* unterminated quotes */
                        goto err;
                    }
                    else
                    {
                        current = sdscatlen(current, p, 1);
                    }
                }
                else
                {
                    switch (*p)
                    {
                    case ' ':
                    case '\n':
                    case '\r':
                    case '\t':
                    case '\0':
                        done = 1;
                        break;
                    case '"':
                        inq = 1;
                        break;
                    case '\'':
                        insq = 1;
                        break;
                    default:
                        current = sdscatlen(current, p, 1);
                        break;
                    }
                }
                if (*p)
                    p++;
            }
            /* add the token to the vector */
            vector = s_realloc(vector, ((*argc) + 1) * sizeof(char *));
            vector[*argc] = current;
            (*argc)++;
            current = NULL;
        }
        else
        {
            /* Even on empty input string return something not NULL. */
            if (vector == NULL)
                vector = s_malloc(sizeof(void *));
            return vector;
        }
    }

err:
    while ((*argc)--)
        sdsfree(vector[*argc]);
    s_free(vector);
    if (current)
        sdsfree(current);
    *argc = 0;
    return NULL;
}

/* Modify the string substituting all the occurrences of the set of
 * characters specified in the 'from' string to the corresponding character
 * in the 'to' array.
 *
 * For instance: sdsmapchars(mystring, "ho", "01", 2)
 * will have the effect of turning the string "hello" into "0ell1".
 *
 * The function returns the sds string pointer, that is always the same
 * as the input pointer since no resize is needed. */
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen)
{
    size_t j, i, l = sdslen(s);

    for (j = 0; j < l; j++)
    {
        for (i = 0; i < setlen; i++)
        {
            if (s[j] == from[i])
            {
                s[j] = to[i];
                break;
            }
        }
    }
    return s;
}

/* Join an array of C strings using the specified separator (also a C string).
 * Returns the result as an sds string. */
sds sdsjoin(char **argv, int argc, char *sep)
{
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++)
    {
        join = sdscat(join, argv[j]);
        if (j != argc - 1)
            join = sdscat(join, sep);
    }
    return join;
}

/* Like sdsjoin, but joins an array of SDS strings. */
sds sdsjoinsds(sds *argv, int argc, const char *sep, size_t seplen)
{
    sds join = sdsempty();
    int j;

    for (j = 0; j < argc; j++)
    {
        join = sdscatsds(join, argv[j]);
        if (j != argc - 1)
            join = sdscatlen(join, sep, seplen);
    }
    return join;
}

/* Wrappers to the allocators used by SDS. Note that SDS will actually
 * just use the macros defined into sdsalloc.h in order to avoid to pay
 * the overhead of function calls. Here we define these wrappers only for
 * the programs SDS is linked to, if they want to touch the SDS internals
 * even if they use a different allocator. */
void *sds_malloc(size_t size) { return s_malloc(size); }
void *sds_realloc(void *ptr, size_t size) { return s_realloc(ptr, size); }
void sds_free(void *ptr) { s_free(ptr); }

#ifdef REDIS_TEST
#include <stdio.h>
#include "testhelp.h"
#include "limits.h"

#define UNUSED(x) (void)(x)
int sdsTest(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    {
        sds x = sdsnew("foo"), y;

        test_cond("Create a string and obtain the length",
                  sdslen(x) == 3 && memcmp(x, "foo\0", 4) == 0)

            sdsfree(x);
        x = sdsnewlen("foo", 2);
        test_cond("Create a string with specified length",
                  sdslen(x) == 2 && memcmp(x, "fo\0", 3) == 0)

            x = sdscat(x, "bar");
        test_cond("Strings concatenation",
                  sdslen(x) == 5 && memcmp(x, "fobar\0", 6) == 0);

        x = sdscpy(x, "a");
        test_cond("sdscpy() against an originally longer string",
                  sdslen(x) == 1 && memcmp(x, "a\0", 2) == 0)

            x = sdscpy(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
        test_cond("sdscpy() against an originally shorter string",
                  sdslen(x) == 33 &&
                      memcmp(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0", 33) == 0)

            sdsfree(x);
        x = sdscatprintf(sdsempty(), "%d", 123);
        test_cond("sdscatprintf() seems working in the base case",
                  sdslen(x) == 3 && memcmp(x, "123\0", 4) == 0)

            sdsfree(x);
        x = sdsnew("--");
        x = sdscatfmt(x, "Hello %s World %I,%I--", "Hi!", LLONG_MIN, LLONG_MAX);
        test_cond("sdscatfmt() seems working in the base case",
                  sdslen(x) == 60 &&
                      memcmp(x, "--Hello Hi! World -9223372036854775808,"
                                "9223372036854775807--",
                             60) == 0)
            printf("[%s]\n", x);

        sdsfree(x);
        x = sdsnew("--");
        x = sdscatfmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
        test_cond("sdscatfmt() seems working with unsigned numbers",
                  sdslen(x) == 35 &&
                      memcmp(x, "--4294967295,18446744073709551615--", 35) == 0)

            sdsfree(x);
        x = sdsnew(" x ");
        sdstrim(x, " x");
        test_cond("sdstrim() works when all chars match",
                  sdslen(x) == 0)

            sdsfree(x);
        x = sdsnew(" x ");
        sdstrim(x, " ");
        test_cond("sdstrim() works when a single char remains",
                  sdslen(x) == 1 && x[0] == 'x')

            sdsfree(x);
        x = sdsnew("xxciaoyyy");
        sdstrim(x, "xy");
        test_cond("sdstrim() correctly trims characters",
                  sdslen(x) == 4 && memcmp(x, "ciao\0", 5) == 0)

            y = sdsdup(x);
        sdsrange(y, 1, 1);
        test_cond("sdsrange(...,1,1)",
                  sdslen(y) == 1 && memcmp(y, "i\0", 2) == 0)

            sdsfree(y);
        y = sdsdup(x);
        sdsrange(y, 1, -1);
        test_cond("sdsrange(...,1,-1)",
                  sdslen(y) == 3 && memcmp(y, "iao\0", 4) == 0)

            sdsfree(y);
        y = sdsdup(x);
        sdsrange(y, -2, -1);
        test_cond("sdsrange(...,-2,-1)",
                  sdslen(y) == 2 && memcmp(y, "ao\0", 3) == 0)

            sdsfree(y);
        y = sdsdup(x);
        sdsrange(y, 2, 1);
        test_cond("sdsrange(...,2,1)",
                  sdslen(y) == 0 && memcmp(y, "\0", 1) == 0)

            sdsfree(y);
        y = sdsdup(x);
        sdsrange(y, 1, 100);
        test_cond("sdsrange(...,1,100)",
                  sdslen(y) == 3 && memcmp(y, "iao\0", 4) == 0)

            sdsfree(y);
        y = sdsdup(x);
        sdsrange(y, 100, 100);
        test_cond("sdsrange(...,100,100)",
                  sdslen(y) == 0 && memcmp(y, "\0", 1) == 0)

            sdsfree(y);
        sdsfree(x);
        x = sdsnew("foo");
        y = sdsnew("foa");
        test_cond("sdscmp(foo,foa)", sdscmp(x, y) > 0)

            sdsfree(y);
        sdsfree(x);
        x = sdsnew("bar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar,bar)", sdscmp(x, y) == 0)

            sdsfree(y);
        sdsfree(x);
        x = sdsnew("aar");
        y = sdsnew("bar");
        test_cond("sdscmp(bar,bar)", sdscmp(x, y) < 0)

            sdsfree(y);
        sdsfree(x);
        x = sdsnewlen("\a\n\0foo\r", 7);
        y = sdscatrepr(sdsempty(), x, sdslen(x));
        test_cond("sdscatrepr(...data...)",
                  memcmp(y, "\"\\a\\n\\x00foo\\r\"", 15) == 0)

        {
            unsigned int oldfree;
            char *p;
            int i;
            size_t step = 10, j;

            sdsfree(x);
            sdsfree(y);
            x = sdsnew("0");
            test_cond("sdsnew() free/len buffers", sdslen(x) == 1 && sdsavail(x) == 0);

            /* Run the test a few times in order to hit the first two
             * SDS header types. */
            for (i = 0; i < 10; i++)
            {
                size_t oldlen = sdslen(x);
                x = sdsMakeRoomFor(x, step);
                int type = x[-1] & SDS_TYPE_MASK;

                test_cond("sdsMakeRoomFor() len", sdslen(x) == oldlen);
                if (type != SDS_TYPE_5)
                {
                    test_cond("sdsMakeRoomFor() free", sdsavail(x) >= step);
                    oldfree = sdsavail(x);
                    UNUSED(oldfree);
                }
                p = x + oldlen;
                for (j = 0; j < step; j++)
                {
                    p[j] = 'A' + j;
                }
                sdsIncrLen(x, step);
            }
            test_cond("sdsMakeRoomFor() content",
                      memcmp("0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ", x, 101) == 0);
            test_cond("sdsMakeRoomFor() final length", sdslen(x) == 101);

            sdsfree(x);
        }
    }
    test_report() return 0;
}
#endif
