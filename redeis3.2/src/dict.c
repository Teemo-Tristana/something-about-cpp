/* Hash Tables Implementation.
 *
 * This file implements in memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include "fmacros.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>

#include "dict.h"
#include "zmalloc.h"
#ifndef DICT_BENCHMARK_MAIN
#include "redisassert.h"
#else
#include <assert.h>
#endif

/* Using dictEnableResize() / dictDisableResize() we make possible to
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory
 * around when there is a child performing saving operations.
 *
 * Note that even when dict_can_resize is set to 0, not all resizes are
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio. */
static int dict_can_resize = 1;
static unsigned int dict_force_resize_ratio = 5;

/* -------------------------- private prototypes ---------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static long _dictKeyIndex(dict *ht, const void *key, uint64_t hash, dictEntry **existing);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* -------------------------- hash functions -------------------------------- */

static uint8_t dict_hash_function_seed[16];

void dictSetHashFunctionSeed(uint8_t *seed)
{
    memcpy(dict_hash_function_seed, seed, sizeof(dict_hash_function_seed));
}

uint8_t *dictGetHashFunctionSeed(void)
{
    return dict_hash_function_seed;
}

/* The default hashing function uses SipHash implementation
 * in siphash.c. */
/* Redis3.0及其之前使用的是 MurmurHash2 算法来计算hash值, 之后使用 SipHash 实现的
 * 在 siphash.c 文件中 */

uint64_t siphash(const uint8_t *in, const size_t inlen, const uint8_t *k);
uint64_t siphash_nocase(const uint8_t *in, const size_t inlen, const uint8_t *k);

uint64_t dictGenHashFunction(const void *key, int len)
{
    return siphash(key, len, dict_hash_function_seed);
}

uint64_t dictGenCaseHashFunction(const unsigned char *buf, int len)
{
    return siphash_nocase(buf, len, dict_hash_function_seed);
}

/* ----------------------------- API implementation ------------------------- */

/* Reset a hash table already initialized with ht_init().
 * NOTE: This function should only be called by ht_destroy(). */
/**
 * 重置 或 初始化 给定哈希表的各项属性
 *  T = O(1)
*/
static void _dictReset(dictht *ht)
{
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

/* Create a new hash table */
/** 创建一个新字典
 * T = O(1)
*/
dict *dictCreate(dictType *type,
                 void *privDataPtr)
{
    dict *d = zmalloc(sizeof(*d));

    _dictInit(d, type, privDataPtr);
    return d;
}

/* Initialize the hash table */
/**
 * 初始化哈希表
 * T = O(1)
*/
int _dictInit(dict *d, dictType *type,
              void *privDataPtr)
{
    // 初始化两个哈希表的各项属性
    // 但是暂不分配给哈希数组分配内存
    _dictReset(&d->ht[0]);
    _dictReset(&d->ht[1]);

    // 设置类型特定函数
    d->type = type;

    // 设置私有数据
    d->privdata = privDataPtr;

    // 设置哈希表 rehash 状态
    d->rehashidx = -1;

    // 设置字典的安全迭代器数量
    d->iterators = 0;

    return DICT_OK;
}

/* Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1 */
 /**
  * 重置字典的大小以便能够包含所有的已有元素
  * 但是 used/buckets 比率 仍然 <= 1.
  * 
 */
int dictResize(dict *d)
{
    unsigned long minimal;

    // 不能在 关闭rehash 或 正在rehash时调用
    if (!dict_can_resize || dictIsRehashing(d))
        return DICT_ERR; 
    
    // 重置最少需要的空间
    minimal = d->ht[0].used; 
    if (minimal < DICT_HT_INITIAL_SIZE)
        minimal = DICT_HT_INITIAL_SIZE
    //调整字典的大小
    return dictExpand(d, minimal);
}

/* Expand or create the hash table */
/* 扩展或创建一个hash表 
 * 创建一个新的哈希表，并根据字典的情况，选择以下其中一个动作来进行：
 *
 * 1) 如果字典的 0 号哈希表为空，那么将新哈希表设置为 0 号哈希表
 * 2) 如果字典的 0 号哈希表非空，那么将新哈希表设置为 1 号哈希表，
 *    并打开字典的 rehash 标识，使得程序可以开始对字典进行 rehash
 *
 * size 参数不够大，或者 rehash 已经在进行时，返回 DICT_ERR 。
 *
 * 成功创建 0 号哈希表，或者 1 号哈希表时，返回 DICT_OK 。
 *
 * T = O(N)
 */
int dictExpand(dict *d, unsigned long size)
{
    /* the size is invalid if it is smaller than the number of
     * elements already inside the hash table */
    /* 若size小于已有的hash表已有元素个数则任务size是无效的，返回 DICT_ERR */
    if (dictIsRehashing(d) || d->ht[0].used > size)
        return DICT_ERR;

    // 新的 hash 表
    dictht n; /* the new hash table */
    // 根据 size 计算hash表的大小: hash表的大小时第一个大于等于 size 的2的幂
    unsigned long realsize = _dictNextPower(size);

    /* Rehashing to the same table size is not useful. */
    /* rehash 到 容量相等的 哈希表是没有意义的，两个表大小都一样，元素也意义，那还做什么rehash */
    if (realsize == d->ht[0].size)
        return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
    /* 为新的哈希表分配空间，并将所有指针指向NULL */
    n.size = realsize;
    n.sizemask = realsize - 1;
    n.table = zcalloc(realsize * sizeof(dictEntry *));
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys. */
    /* 若 0 号哈希表为空，则是第一次初始化
     * 将新哈希表设置为 0 号哈希表的指针，然后字典就可以接收键值对了。
     */
    if (d->ht[0].table == NULL) //第一次初始化
    {
        d->ht[0] = n;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    /**
     *  准备 1 号哈希表，以便渐进式 rehash 
     *  0 号哈希表不为空，则是rehash，将新的hash表设置为1号哈希表，并设置rehash 标识 rehashidx = 0，以便进行rehash。
     * */
    d->ht[1] = n; // 将新的hash表设置为1号哈希表
    d->rehashidx = 0; // 打开 rehash标识 
    return DICT_OK;
    
    /* 上述代码重构
    if(d->ht[0].table == NULL)
        d->ht[0] = n;
    else 
    {
        d->ht[0] = n;
        d->rehashidx = 0; 
    }
     return DICT_OK;
    */
}

/* Performs N steps of incremental rehashing. Returns 1 if there are still
 * keys to move from the old to the new hash table, otherwise 0 is returned.
 *
 * Note that a rehashing step consists in moving a bucket (that may have more
 * than one key as we use chaining) from the old to the new hash table, however
 * since part of the hash table may be composed of empty spaces, it is not
 * guaranteed that this function will rehash even a single bucket, since it
 * will visit at max N*10 empty buckets in total, otherwise the amount of
 * work it does would be unbound and the function may block for a long time. */
/**
 * 在每次调用 dictRehash 时，最多迁移 n 个非空 bucket，返回 1 时表示 Rehash 操作还未完成，未处于 Rehash 状态或 Rehash 完成时返回 0.
 * 执行 N 步进行式 rehash . 
 * 返回 1 表示仍然有键 需要从 0 号哈希表移动到 1 号哈希表
 * 返回 0 表示所有键移动完成
 * 
 * 每步 rehash 操作都是以一个哈希索引(桶)为单位(一个桶可能含有多个节点)
 * 被rehash的桶里面的所有节点都将会被移动到新的哈希表中去。
 * 
*/
int dictRehash(dict *d, int n)
{
    int empty_visits = n * 10; /* Max number of empty buckets to visit. */
    // 只有在rehash时才可以进行
    if (!dictIsRehashing(d))
        return 0;

    // 进行 N 步迁移
    while (n-- && d->ht[0].used != 0)
    {
        dictEntry *de, *nextde;

        /* Note that rehashidx can't overflow as we are sure there are more
         * elements because ht[0].used != 0 */
        /* 确保 rehashidx 没有越界 */
        assert(d->ht[0].size > (unsigned long)d->rehashidx);
        while (d->ht[0].table[d->rehashidx] == NULL)  // 略过数组中为空的索引，直到一个非空索引
        {
            d->rehashidx++;
            if (--empty_visits == 0)
                return 1;
        }

        // 指向该索引的链表表头节点
        de = d->ht[0].table[d->rehashidx];
        /* Move all the keys in this bucket from the old to the new hash HT */
        /* 将链表中索引键值对移动到新的哈希表中 */
        while (de)
        {
            uint64_t h;

            // 保持下一个指针
            nextde = de->next;  
            /* Get the index in the new hash table */
            /* 计算在新哈希表的哈希值和节点插入的位置 */
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;

            // 将节点插入新哈希表
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;

            // 更新计数器
            d->ht[0].used--;
            d->ht[1].used++;

            // 继续处理下一个节点
            de = nextde;
        }
        // 将该迁移完成的哈希表索引的指针置为空
        d->ht[0].table[d->rehashidx] = NULL;
        // 更新哈希索引
        d->rehashidx++;
    }

    /* Check if we already rehashed the whole table... */
    /* 检查是否已经对整个hash表完成了rehash */
    if (d->ht[0].used == 0) //当 ht[0] = 0时，即完成了 rehash 操作
    {
        zfree(d->ht[0].table); // 释放空间
        d->ht[0] = d->ht[1]; // 将 ht[1] 置为 ht[0]
        _dictReset(&d->ht[1]); //重置 ht[1]
        d->rehashidx = -1; // 设置 rehash 标识为 -1
        return 0;
    }

    /* More to rehash... */
    return 1;
}

/* 返回以毫秒为单位的 UNIX 时间戳 */
long long timeInMilliseconds(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (((long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

/* Rehash in ms+"delta" milliseconds. The value of "delta" is larger 
 * than 0, and is smaller than 1 in most cases. The exact upper bound 
 * depends on the running time of dictRehash(d,100).*/
/**
 * 在 delta 毫秒内进行 rehash, delta的值大于0小于1
 * 确切的上限时间取决于 dictRehash(d, 100)的运行时间
 * 在给定的毫米内，以 100 步为单位，对字典进行 rehahs 
*/
int dictRehashMilliseconds(dict *d, int ms)
{
    if (d->iterators > 0)
        return 0;

    //记录时间
    long long start = timeInMilliseconds();
    int rehashes = 0;

    while (dictRehash(d, 100))
    {
        rehashes += 100;
        // 如果时间已过，跳出
        if (timeInMilliseconds() - start > ms)
            break;
    }
    return rehashes;
}

/* This function performs just a step of rehashing, and only if there are
 * no safe iterators bound to our hash table. When we have iterators in the
 * middle of a rehashing we can't mess with the two hash tables otherwise
 * some element can be missed or duplicated.
 *
 * This function is called by common lookup or update operations in the
 * dictionary so that the hash table automatically migrates from H1 to H2
 * while it is actively used. */
/**
 * 在字典不安全的情况下，对字典进行单步 rehash
 * 在rehash过程中，不能弄乱哈希表，否则可能出现元素丢失或重复的现象
 * 
 * 这个函数子多个通用的查找、更新操作中调用，
 * 这个函数可以保证让字典在被使用的同时进行rehash
 * 
 * T =  O(1)
*/
static void _dictRehashStep(dict *d)
{
    if (d->iterators == 0)
        dictRehash(d, 1); //单步rehash
}

/* Add an element to the target hash table */
/**
 * 添加一个新元素到 目标哈希表中 
 * 只有键不在 hash 表中时，才会成功
 * 成功返回 DICT_OK，失败返回 DICT_ERR
 * 最坏 T = O(N)，平均 O(1)
 */
int dictAdd(dict *d, void *key, void *val)
{
    // 尝试往字典添加，返回值包含了这个键的新哈希节点
    dictEntry *entry = dictAddRaw(d, key, NULL);

    if (!entry) // 键已存在，添加失败
        return DICT_ERR;

    // 键不存在，设置节点的值
    dictSetVal(d, entry, val);

    // 添加成功
    return DICT_OK;
}

/* Low level add or find:
 * This function adds the entry but instead of setting a value returns the
 * dictEntry structure to the user, that will make sure to fill the value
 * field as he wishes.
 *
 * This function is also directly exposed to the user API to be called
 * mainly in order to store non-pointers inside the hash value, example:
 *
 * entry = dictAddRaw(dict,mykey,NULL);
 * if (entry != NULL) dictSetSignedIntegerVal(entry,1000);
 *
 * Return values:
 *
 * If key already exists NULL is returned, and "*existing" is populated
 * with the existing entry if existing is not NULL.
 *
 * If key was added, the hash entry is returned to be manipulated by the caller.
 */
/**
 * 尝试将键插入到字典
 * 若键已经在字典存在，那么返回 NULL 
 * 若键不存在，则创建新的哈希节点
 * 将节点和键关联并插入字典，然后返回节点本身
*/
dictEntry *dictAddRaw(dict *d, void *key, dictEntry **existing)
{
    long index;
    dictEntry *entry; // 节点
    dictht *ht; // 哈希表

    // 若dictht 正在进行rehash，则 单步 rehash
    if (dictIsRehashing(d))
        _dictRehashStep(d);

    /* Get the index of the new element, or -1 if
     * the element already exists. */
    /* 计算新元素的哈希索引，若为-1，则表示该元素已存在 */
    if ((index = _dictKeyIndex(d, key, dictHashKey(d, key), existing)) == -1)
        return NULL;

    /* Allocate the memory and store the new entry.
     * Insert the element in top, with the assumption that in a database
     * system it is more likely that recently added entries are accessed
     * more frequently. */
    /** 
     * 分配内存，若字典正在进行rehash将新元素添加到1号哈希表，否则添加到0号哈希表
     * 将元素插入到top位置，假定的是新加入的元素更有可能被访问
    */

    // 根据是否正在进行rehash来选择hash表，正在进行rehash，则插入ht[1],否则插入ht[0]
    ht = dictIsRehashing(d) ? &d->ht[1] : &d->ht[0]; 
    
    // 分配空间
    entry = zmalloc(sizeof(*entry)); 

    // 将新节点插入到链表头
    entry->next = ht->table[index]; 
    ht->table[index] = entry;

    // 更有已有节点数目
    ht->used++;

    /* Set the hash entry fields. */
    /* 设置新节点的值 */
    dictSetKey(d, entry, key);
    return entry;
}

/* Add or Overwrite:
 * Add an element, discarding the old value if the key already exists.
 * Return 1 if the key was added from scratch, 0 if there was already an
 * element with such key and dictReplace() just performed a value update
 * operation. */
/** 添加或重写
 *  添加一个新元素，不管旧值是否已存在
 *  若键值对为全新键值对，那么返回 1
 *  若是对已有的键值对更新操作，那么返回 0
 *  T = O(N)
*/
int dictReplace(dict *d, void *key, void *val)
{
    dictEntry *entry, *existing, auxentry;

    /* Try to add the element. If the key
     * does not exists dictAdd will succeed. */
    /* 尝试直接将键值对添加到字典， 若键 key 不存在，则添加成功 T = O(N) */
    entry = dictAddRaw(d, key, &existing);
    if (entry) // 键不存在
    {
        dictSetVal(d, entry, val); // 设置键的值
        return 1;
    }

    /* Set the new value and free the old one. Note that it is important
     * to do that in this order, as the value may just be exactly the same
     * as the previous one. In this context, think to reference counting,
     * you want to increment (set), and then decrement (free), and not the
     * reverse. */
    /**
     * 设置新的value并释放旧值
     * 按顺序进行操作很重要 因为该值可能与前一个值相同，这种情况下，优先考虑使用引用计数，
     * 设置value时增加引用计数，释放value时减少引用计数
    */
    auxentry = *existing;
    dictSetVal(d, existing, val); // 设置新的值
    dictFreeVal(d, &auxentry); // 释放旧值
    return 0;
}

/* Add or Find:
 * dictAddOrFind() is simply a version of dictAddRaw() that always
 * returns the hash entry of the specified key, even if the key already
 * exists and can't be added (in that case the entry of the already
 * existing key is returned.)
 *
 * See dictAddRaw() for more information. */
dictEntry *dictAddOrFind(dict *d, void *key)
{
    dictEntry *entry, *existing;
    entry = dictAddRaw(d, key, &existing);
    return entry ? entry : existing;
}

/* Search and remove an element. This is an helper function for
 * dictDelete() and dictUnlink(), please check the top comment
 * of those functions. */
/**
 * 查找和删除给定键的节点
 * 是一个用于 dictDelete()和dictUnlink()的辅助函数
*/
static dictEntry *dictGenericDelete(dict *d, const void *key, int nofree)
{
    uint64_t h, idx;
    dictEntry *he, *prevHe;
    int table;

    // 字典为空 
    if (d->ht[0].used == 0 && d->ht[1].used == 0)
        return NULL;

    // 进行单步 rehash
    if (dictIsRehashing(d))
        _dictRehashStep(d);

    // 计算哈希值
    h = dictHashKey(d, key);

    // 遍历哈希表
    for (table = 0; table <= 1; table++)
    {
         // 计算 hash 索引 
        idx = h & d->ht[table].sizemask;
        // 指向该索引的链表
        he = d->ht[table].table[idx];
        prevHe = NULL;

        // 遍历链表上的所有节点
        while (he)
        {
             // 查找目标节点
            if (key == he->key || dictCompareKeys(d, key, he->key))
            {
                /* Unlink the element from the list */
                /* 从链表中删除该节点 */
                if (prevHe)
                    prevHe->next = he->next;
                else
                    d->ht[table].table[idx] = he->next;
                
               
                if (!nofree)  // 释放键值对
                {
                    dictFreeKey(d, he);
                    dictFreeVal(d, he);
                    zfree(he);  // 释放节点本身
                }

                // 更新已有节点数量
                d->ht[table].used--;
                return he;
            }
            prevHe = he;
            he = he->next;
        }

        // 在 0 号哈希表中为查找到该键，且没有正在进行 rehash 则直接跳出，否则去1号哈希表中查找。
        if (!dictIsRehashing(d))
            break;
    }
    return NULL; /* not found */
}

/* Remove an element, returning DICT_OK on success or DICT_ERR if the
 * element was not found. */
/** 从字典中删除给定的键值对
 *  成功 返回 DICT_OK
 *  若没有找到该元素，则返回 DICT_ERR
 *  T = O(1)
*/
int dictDelete(dict *ht, const void *key)
{
    return dictGenericDelete(ht, key, 0) ? DICT_OK : DICT_ERR;
}

/* Remove an element from the table, but without actually releasing
 * the key, value and dictionary entry. The dictionary entry is returned
 * if the element was found (and unlinked from the table), and the user
 * should later call `dictFreeUnlinkedEntry()` with it in order to release it.
 * Otherwise if the key is not found, NULL is returned.
 *
 * This function is useful when we want to remove something from the hash
 * table but want to use its value before actually deleting the entry.
 * Without this function the pattern would require two lookups:
 *
 *  entry = dictFind(...);
 *  // Do something with entry
 *  dictDelete(dictionary,entry);
 *
 * Thanks to this function it is possible to avoid this, and use
 * instead:
 *
 * entry = dictUnlink(dictionary,entry);
 * // Do something with entry
 * dictFreeUnlinkedEntry(entry); // <- This does not need to lookup again.
 */
/**
 * 从字典中移除给定key值，但并不删除键值对和节点 
*/
dictEntry *dictUnlink(dict *ht, const void *key)
{
    return dictGenericDelete(ht, key, 1);
}

/* You need to call this function to really free the entry after a call
 * to dictUnlink(). It's safe to call this function with 'he' = NULL. */
/* 释放节点空间 */
void dictFreeUnlinkedEntry(dict *d, dictEntry *he)
{
    if (he == NULL)
        return;
    dictFreeKey(d, he); // 释放键空间
    dictFreeVal(d, he); // 释放值空间
    zfree(he); // 释放节点
}

/* Destroy an entire dictionary */
/* 销毁整个字典 */
int _dictClear(dict *d, dictht *ht, void(callback)(void *))
{
    unsigned long i;

    /* Free all the elements */
    /* 遍历整个哈希表 释放所有元素 */
    for (i = 0; i < ht->size && ht->used > 0; i++)
    {
        dictEntry *he, *nextHe;

        if (callback && (i & 65535) == 0)
            callback(d->privdata);
        
        // 跳过空索引 
        if ((he = ht->table[i]) == NULL)
            continue;

        // 遍历整个链表
        while (he)
        {
            nextHe = he->next;
            dictFreeKey(d, he); // 删除键
            dictFreeVal(d, he); // 删除值
            zfree(he); // 释放值
            ht->used--; // 更新节点数
            he = nextHe; // 处理下一个节点 
        }
    }
    /* Free the table and the allocated cache structure */
    /* 释放哈希表和分配的缓存结构 */
    zfree(ht->table);
    /* Re-initialize the table */
    /* 重置哈希表属性 */
    _dictReset(ht);
    return DICT_OK; /* never fails */
}

/* Clear & Release the hash table */
/* 删除并释放整个字典 */
void dictRelease(dict *d)
{
    // 删除并清空两个哈希表
    _dictClear(d, &d->ht[0], NULL);
    _dictClear(d, &d->ht[1], NULL);
    zfree(d); // 释放节点结构
}

/**
 * 查找字典中含key的节点
 * 成功返回节点，失败返回 NULL
 * T = O(1)
*/
dictEntry *dictFind(dict *d, const void *key)
{
    dictEntry *he;
    uint64_t h, idx, table;

    // 字典(的哈希表)为空
    if (dictSize(d) == 0)
        return NULL; /* dict is empty */
    
    // 若正在进行rehash，则单步查找
    if (dictIsRehashing(d))
        _dictRehashStep(d);

    // 计算键的哈希值
    h = dictHashKey(d, key);

    // 在字典的哈希表中查找这个键
    for (table = 0; table <= 1; table++)
    {
        // 计算索引值
        idx = h & d->ht[table].sizemask;

        // 遍历给定索引上的链表的索引节点，查找 key
        he = d->ht[table].table[idx];
        while (he)
        {
            if (key == he->key || dictCompareKeys(d, key, he->key))
                return he;
            he = he->next;
        }
        // 若遍历完 0 号哈希表，仍然没有找到指定的键的节点
        // 若字典正在进行 rehash，则在 1 号哈希表中进行查找，否则直接返回NULL
        if (!dictIsRehashing(d))
            return NULL;
    }
    // 未查找到，返回NULL
    return NULL;
}

/**
 * 返回给定键的值
 * T = O(1)
*/
void *dictFetchValue(dict *d, const void *key)
{
    dictEntry *he;

    // T = O(1)
    he = dictFind(d, key); // 查找含键的节点
    return he ? dictGetVal(he) : NULL; //节点不为空，返回节点值，否则返回NULL
}

/* A fingerprint is a 64 bit number that represents the state of the dictionary
 * at a given time, it's just a few dict properties xored together.
 * When an unsafe iterator is initialized, we get the dict fingerprint, and check
 * the fingerprint again when the iterator is released.
 * If the two fingerprints are different it means that the user of the iterator
 * performed forbidden operations against the dictionary while iterating. */
/**
 * 计算字典的哈希值 
 * 
 * fingerprint 是一个64位数字，表示字典的在给定时刻的状态，它将字典的几种属性结合在一起。
 * 当一个不安全的迭代器初始化时，获取字典的 figerprint， 在迭代器释放时再次检查该 fingerprint
 * 当两个 fingerprints 不同时，意味着迭代器的用对在字典迭代时执行禁止操作
*/
long long dictFingerprint(dict *d)
{
    long long integers[6], hash = 0;
    int j;

    // 字典的几种属性
    integers[0] = (long)d->ht[0].table; // 0 号哈希表
    integers[1] = d->ht[0].size; // 0号哈希表的大小
    integers[2] = d->ht[0].used; // 0号哈希表已有节点数量
    integers[3] = (long)d->ht[1].table; // 1 号哈希表
    integers[4] = d->ht[1].size; // 1 号哈希表的大小
    integers[5] = d->ht[1].used; // 1 号哈希表已有节点数量

    /* We hash N integers by summing every successive integer with the integer
     * hashing of the previous sum. Basically:
     *
     * Result = hash(hash(hash(int1)+int2)+int3) ...
     *
     * This way the same set of integers in a different order will (likely) hash
     * to a different number. */
    /**
     * 通过将 整数与前面之和的散列和来对 N 个整数进行散列，大致如下
     *  Result = hash(hash(hash(int1)+int2)+int3) ...
     * 以不同顺序排列的同一组整数将可能散列为不同数字
    */
    for (j = 0; j < 6; j++)
    {
        hash += integers[j];
        /* For the hashing step we use Tomas Wang's 64 bit integer hash. */
        /* 对于哈希步骤，我们使用Tomas Wang的64位整数哈希。 */
        hash = (~hash) + (hash << 21); // hash = (hash << 21) - hash - 1;
        hash = hash ^ (hash >> 24);
        hash = (hash + (hash << 3)) + (hash << 8); // hash * 265
        hash = hash ^ (hash >> 14);
        hash = (hash + (hash << 2)) + (hash << 4); // hash * 21
        hash = hash ^ (hash >> 28);
        hash = hash + (hash << 31);
    }
    return hash;
}

/**
 * 创建并返回给定字典的不安全迭代器 
 * 
 * T = O(1)
*/
dictIterator *dictGetIterator(dict *d)
{
    dictIterator *iter = zmalloc(sizeof(*iter)); // 分配空间

    iter->d = d; // 被迭代字典
    iter->table = 0; // 正在使用的哈希表
    iter->index = -1; // 哈希表索引的位置
    iter->safe = 0; // 是否安全 0不安全  1安全
    iter->entry = NULL;  // 节点指针
    iter->nextEntry = NULL; // 下一个节点指针
    return iter;
}

/**
 * 创建并返回给定字典的安全迭代器 
 * 
 * T = O(1)
*/
dictIterator *dictGetSafeIterator(dict *d)
{
    dictIterator *i = dictGetIterator(d);

    i->safe = 1; // 将迭代器设置为安全的
    return i;
}

/**
 * 返回迭代器指向的当前节点
 * 字典迭代完毕时，返回 NULL
 * 
 * T = O(1)
*/
dictEntry *dictNext(dictIterator *iter)
{
    while (1)
    {
        /**
         * 进行循环有2中可能
         *  1) 迭代器第一次运行
         *  2) 当前索引链表中的节点已经迭代完(NULL 为链表的表尾)
        */
        if (iter->entry == NULL)
        {
            // 指向被迭代的哈希表
            dictht *ht = &iter->d->ht[iter->table];

            // 初次被迭代
            if (iter->index == -1 && iter->table == 0)
            {
                if (iter->safe) // 迭代器安全 ，则更新迭代器计数
                    iter->d->iterators++;
                else // 迭代器不安全，则计算 fingerprint 
                    iter->fingerprint = dictFingerprint(iter->d);
            }

            // 更新索引
            iter->index++;

            // 若迭代器的所有大于当前正在的哈希表的大小，则说明当前哈希表已迭代完毕 
            if (iter->index >= (long)ht->size)
            {
                // 若正在进行rehash，则说明1号哈希表正在使用中，则继续对1号哈希表进行迭代
                if (dictIsRehashing(iter->d) && iter->table == 0)
                {
                    iter->table++;
                    iter->index = 0;
                    ht = &iter->d->ht[1];
                }
                else //否则
                {
                    break;
                }
            }
            // 更新节点指针，指向下一个索引链表的表头节点
            iter->entry = ht->table[iter->index];
        }
        else
        {
            // 指针迭代某个节点链表
            iter->entry = iter->nextEntry;
        }
        if (iter->entry)
        {
            /* We need to save the 'next' here, the iterator user
             * may delete the entry we are returning. */
            /**
             * 需要保持 next 指针，因为迭代器在使用过程中可能删除某个节点
            */
            iter->nextEntry = iter->entry->next;
            return iter->entry;
        }
    }

    // 迭代完毕
    return NULL;
}

/* 释放给定迭代器 */
void dictReleaseIterator(dictIterator *iter)
{
    if (!(iter->index == -1 && iter->table == 0))
    {
        if (iter->safe)
            iter->d->iterators--;
        else
            assert(iter->fingerprint == dictFingerprint(iter->d));
    }
    zfree(iter);
}

/* Return a random entry from the hash table. Useful to
 * implement randomized algorithms */
/** 随机返回字典中任意一个节点 用于实现随机化算法 
 *  若字典为空，返回 NULL
*/
dictEntry *dictGetRandomKey(dict *d)
{
    dictEntry *he, *orighe;
    unsigned long h;
    int listlen, listele;

    // 字典为空 
    if (dictSize(d) == 0)
        return NULL;

    // 字典指针进行 rehash ，则进行单步 rehash
    if (dictIsRehashing(d))
        _dictRehashStep(d);
    
    // 若在进行 rehahs，那么将1号哈希表也作为随机查找的目标
    if (dictIsRehashing(d))
    {
        do
        {
            /* We are sure there are no elements in indexes from 0
             * to rehashidx-1 */
            h = d->rehashidx + (random() % (dictSlots(d) - d->rehashidx));
            he = (h >= d->ht[0].size) ? d->ht[1].table[h - d->ht[0].size] : d->ht[0].table[h];
        } while (he == NULL);
    }
    else // 否则只将0号哈希表作为随机查找的目标
    {
        do
        {
            h = random() & d->ht[0].sizemask;
            he = d->ht[0].table[h];
        } while (he == NULL);
    }

    /* Now we found a non empty bucket, but it is a linked
     * list and we need to get a random element from the list.
     * The only sane way to do so is counting the elements and
     * select a random index. */
    /**
     * he指向一个链表的非空节点
     * 程序将从这个链表随机返回一个节点
    */
    listlen = 0;
    orighe = he;
    // 计算该链表节点数
    while (he)
    {
        he = he->next;
        listlen++;
    }
    // 取模，得出随机节点的索引
    listele = random() % listlen; // random()% n 的范围是 0 ~ n-1
    he = orighe;
    // 按索引查找链表的节点
    while (listele--)
        he = he->next;
    return he; // 返回随机节点
}

/* This function samples the dictionary to return a few keys from random
 * locations.
 *
 * It does not guarantee to return all the keys specified in 'count', nor
 * it does guarantee to return non-duplicated elements, however it will make
 * some effort to do both things.
 *
 * Returned pointers to hash table entries are stored into 'des' that
 * points to an array of dictEntry pointers. The array must have room for
 * at least 'count' elements, that is the argument we pass to the function
 * to tell how many random elements we need.
 *
 * The function returns the number of items stored into 'des', that may
 * be less than 'count' if the hash table has less than 'count' elements
 * inside, or if not enough elements were found in a reasonable amount of
 * steps.
 *
 * Note that this function is not suitable when you need a good distribution
 * of the returned items, but only when you need to "sample" a given number
 * of continuous elements to run some kind of algorithm or to produce
 * statistics. However the function is much faster than dictGetRandomKey()
 * at producing N elements. */
/**
 * 本函数对字典随机采样，以返回一些随机位置的key值
 * 
 * 会尽量保证返回 count指定个数的键和键重复，但是不保证
 * 
 * 返回时，在des中存储采样所得的 dictEntry 数组结果，不保证元素不重复，返回的 des 中的 dictEntry 个数可能小于 count 个
 * des 所指的内存空间需要能容纳 count 个 dictEntry 指针
 * 
 * 比调用 count 次 dictGetRandomKey() 高效
*/
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count)
{
    unsigned long j;      /* internal hash table id, 0 or 1. */ // 内存哈希表的id 
    unsigned long tables; /* 1 or 2 tables? */
    unsigned long stored = 0, maxsizemask;
    unsigned long maxsteps;

    if (dictSize(d) < count)
        count = dictSize(d);
    maxsteps = count * 10;

    /* Try to do a rehashing work proportional to 'count'. */
    /* 尝试与计数存比例的哈休处理工作 */
    for (j = 0; j < count; j++)
    {
        if (dictIsRehashing(d))
            _dictRehashStep(d);
        else
            break;
    }

    tables = dictIsRehashing(d) ? 2 : 1;
    maxsizemask = d->ht[0].sizemask;
    if (tables > 1 && maxsizemask < d->ht[1].sizemask)
        maxsizemask = d->ht[1].sizemask;

    /* Pick a random point inside the larger table. */
    unsigned long i = random() & maxsizemask;
    unsigned long emptylen = 0; /* Continuous empty entries so far. */
    while (stored < count && maxsteps--)
    {
        for (j = 0; j < tables; j++)
        {
            /* Invariant of the dict.c rehashing: up to the indexes already
             * visited in ht[0] during the rehashing, there are no populated
             * buckets, so we can skip ht[0] for indexes between 0 and idx-1. */
            if (tables == 2 && j == 0 && i < (unsigned long)d->rehashidx)
            {
                /* Moreover, if we are currently out of range in the second
                 * table, there will be no elements in both tables up to
                 * the current rehashing index, so we jump if possible.
                 * (this happens when going from big to small table). */
                if (i >= d->ht[1].size)
                    i = d->rehashidx;
                else
                    continue;
            }
            if (i >= d->ht[j].size)
                continue; /* Out of range for this table. */
            dictEntry *he = d->ht[j].table[i];

            /* Count contiguous empty buckets, and jump to other
             * locations if they reach 'count' (with a minimum of 5). */
            if (he == NULL)
            {
                emptylen++;
                if (emptylen >= 5 && emptylen > count)
                {
                    i = random() & maxsizemask;
                    emptylen = 0;
                }
            }
            else
            {
                emptylen = 0;
                while (he)
                {
                    /* Collect all the elements of the buckets found non
                     * empty while iterating. */
                    *des = he;
                    des++;
                    he = he->next;
                    stored++;
                    if (stored == count)
                        return stored;
                }
            }
        }
        i = (i + 1) & maxsizemask;
    }
    return stored;
}

/* This is like dictGetRandomKey() from the POV of the API, but will do more
 * work to ensure a better distribution of the returned element.
 *
 * This function improves the distribution because the dictGetRandomKey()
 * problem is that it selects a random bucket, then it selects a random
 * element from the chain in the bucket. However elements being in different
 * chain lengths will have different probabilities of being reported. With
 * this function instead what we do is to consider a "linear" range of the table
 * that may be constituted of N buckets with chains of different lengths
 * appearing one after the other. Then we report a random element in the range.
 * In this way we smooth away the problem of different chain lengths. */
#define GETFAIR_NUM_ENTRIES 15
dictEntry *dictGetFairRandomKey(dict *d)
{
    dictEntry *entries[GETFAIR_NUM_ENTRIES];
    unsigned int count = dictGetSomeKeys(d, entries, GETFAIR_NUM_ENTRIES);
    /* Note that dictGetSomeKeys() may return zero elements in an unlucky
     * run() even if there are actually elements inside the hash table. So
     * when we get zero, we call the true dictGetRandomKey() that will always
     * yeld the element if the hash table has at least one. */
    if (count == 0)
        return dictGetRandomKey(d);
    unsigned int idx = rand() % count;
    return entries[idx];
}

/* Function to reverse bits. Algorithm from:
 * http://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel */
static unsigned long rev(unsigned long v)
{
    unsigned long s = CHAR_BIT * sizeof(v); // bit size; must be power of 2
    unsigned long mask = ~0UL;
    while ((s >>= 1) > 0)
    {
        mask ^= (mask << s);
        v = ((v >> s) & mask) | ((v << s) & ~mask);
    }
    return v;
}

/* dictScan() is used to iterate over the elements of a dictionary.
 *
 * Iterating works the following way:
 *
 * 1) Initially you call the function using a cursor (v) value of 0.
 * 2) The function performs one step of the iteration, and returns the
 *    new cursor value you must use in the next call.
 * 3) When the returned cursor is 0, the iteration is complete.
 *
 * The function guarantees all elements present in the
 * dictionary get returned between the start and end of the iteration.
 * However it is possible some elements get returned multiple times.
 *
 * For every element returned, the callback argument 'fn' is
 * called with 'privdata' as first argument and the dictionary entry
 * 'de' as second argument.
 *
 * HOW IT WORKS.
 *
 * The iteration algorithm was designed by Pieter Noordhuis.
 * The main idea is to increment a cursor starting from the higher order
 * bits. That is, instead of incrementing the cursor normally, the bits
 * of the cursor are reversed, then the cursor is incremented, and finally
 * the bits are reversed again.
 *
 * This strategy is needed because the hash table may be resized between
 * iteration calls.
 *
 * dict.c hash tables are always power of two in size, and they
 * use chaining, so the position of an element in a given table is given
 * by computing the bitwise AND between Hash(key) and SIZE-1
 * (where SIZE-1 is always the mask that is equivalent to taking the rest
 *  of the division between the Hash of the key and SIZE).
 *
 * For example if the current hash table size is 16, the mask is
 * (in binary) 1111. The position of a key in the hash table will always be
 * the last four bits of the hash output, and so forth.
 *
 * WHAT HAPPENS IF THE TABLE CHANGES IN SIZE?
 *
 * If the hash table grows, elements can go anywhere in one multiple of
 * the old bucket: for example let's say we already iterated with
 * a 4 bit cursor 1100 (the mask is 1111 because hash table size = 16).
 *
 * If the hash table will be resized to 64 elements, then the new mask will
 * be 111111. The new buckets you obtain by substituting in ??1100
 * with either 0 or 1 can be targeted only by keys we already visited
 * when scanning the bucket 1100 in the smaller hash table.
 *
 * By iterating the higher bits first, because of the inverted counter, the
 * cursor does not need to restart if the table size gets bigger. It will
 * continue iterating using cursors without '1100' at the end, and also
 * without any other combination of the final 4 bits already explored.
 *
 * Similarly when the table size shrinks over time, for example going from
 * 16 to 8, if a combination of the lower three bits (the mask for size 8
 * is 111) were already completely explored, it would not be visited again
 * because we are sure we tried, for example, both 0111 and 1111 (all the
 * variations of the higher bit) so we don't need to test it again.
 *
 * WAIT... YOU HAVE *TWO* TABLES DURING REHASHING!
 *
 * Yes, this is true, but we always iterate the smaller table first, then
 * we test all the expansions of the current cursor into the larger
 * table. For example if the current cursor is 101 and we also have a
 * larger table of size 16, we also test (0)101 and (1)101 inside the larger
 * table. This reduces the problem back to having only one table, where
 * the larger one, if it exists, is just an expansion of the smaller one.
 *
 * LIMITATIONS
 *
 * This iterator is completely stateless, and this is a huge advantage,
 * including no additional memory used.
 *
 * The disadvantages resulting from this design are:
 *
 * 1) It is possible we return elements more than once. However this is usually
 *    easy to deal with in the application level.
 * 2) The iterator must return multiple elements per call, as it needs to always
 *    return all the keys chained in a given bucket, and all the expansions, so
 *    we are sure we don't miss keys moving during rehashing.
 * 3) The reverse cursor is somewhat hard to understand at first, but this
 *    comment is supposed to help.
 */
unsigned long dictScan(dict *d,
                       unsigned long v,
                       dictScanFunction *fn,
                       dictScanBucketFunction *bucketfn,
                       void *privdata)
{
    dictht *t0, *t1;
    const dictEntry *de, *next;
    unsigned long m0, m1;

    if (dictSize(d) == 0)
        return 0;

    /* Having a safe iterator means no rehashing can happen, see _dictRehashStep.
     * This is needed in case the scan callback tries to do dictFind or alike. */
    d->iterators++;

    if (!dictIsRehashing(d))
    {
        t0 = &(d->ht[0]);
        m0 = t0->sizemask;

        /* Emit entries at cursor */
        if (bucketfn)
            bucketfn(privdata, &t0->table[v & m0]);
        de = t0->table[v & m0];
        while (de)
        {
            next = de->next;
            fn(privdata, de);
            de = next;
        }

        /* Set unmasked bits so incrementing the reversed cursor
         * operates on the masked bits */
        v |= ~m0;

        /* Increment the reverse cursor */
        v = rev(v);
        v++;
        v = rev(v);
    }
    else
    {
        t0 = &d->ht[0];
        t1 = &d->ht[1];

        /* Make sure t0 is the smaller and t1 is the bigger table */
        if (t0->size > t1->size)
        {
            t0 = &d->ht[1];
            t1 = &d->ht[0];
        }

        m0 = t0->sizemask;
        m1 = t1->sizemask;

        /* Emit entries at cursor */
        if (bucketfn)
            bucketfn(privdata, &t0->table[v & m0]);
        de = t0->table[v & m0];
        while (de)
        {
            next = de->next;
            fn(privdata, de);
            de = next;
        }

        /* Iterate over indices in larger table that are the expansion
         * of the index pointed to by the cursor in the smaller table */
        do
        {
            /* Emit entries at cursor */
            if (bucketfn)
                bucketfn(privdata, &t1->table[v & m1]);
            de = t1->table[v & m1];
            while (de)
            {
                next = de->next;
                fn(privdata, de);
                de = next;
            }

            /* Increment the reverse cursor not covered by the smaller mask.*/
            v |= ~m1;
            v = rev(v);
            v++;
            v = rev(v);

            /* Continue while bits covered by mask difference is non-zero */
        } while (v & (m0 ^ m1));
    }

    /* undo the ++ at the top */
    d->iterators--;

    return v;
}

/* ------------------------- private functions ------------------------------ */

/* Expand the hash table if needed */
static int _dictExpandIfNeeded(dict *d)
{
    /* Incremental rehashing already in progress. Return. */
    if (dictIsRehashing(d))
        return DICT_OK;

    /* If the hash table is empty expand it to the initial size. */
    if (d->ht[0].size == 0)
        return dictExpand(d, DICT_HT_INITIAL_SIZE);

    /* If we reached the 1:1 ratio, and we are allowed to resize the hash
     * table (global setting) or we should avoid it but the ratio between
     * elements/buckets is over the "safe" threshold, we resize doubling
     * the number of buckets. */
    if (d->ht[0].used >= d->ht[0].size &&
        (dict_can_resize ||
         d->ht[0].used / d->ht[0].size > dict_force_resize_ratio))
    {
        return dictExpand(d, d->ht[0].used * 2);
    }
    return DICT_OK;
}

/* Our hash table capability is a power of two */
/* 哈希表的容量是 2 的幂*/
static unsigned long _dictNextPower(unsigned long size)
{
    unsigned long i = DICT_HT_INITIAL_SIZE;

    if (size >= LONG_MAX)
        return LONG_MAX + 1LU;
    while (1)
    {
        if (i >= size)
            return i;
        i *= 2;
    }
}

/* Returns the index of a free slot that can be populated with
 * a hash entry for the given 'key'.
 * If the key already exists, -1 is returned
 * and the optional output parameter may be filled.
 *
 * Note that if we are in the process of rehashing the hash table, the
 * index is always returned in the context of the second (new) hash table. */
static long _dictKeyIndex(dict *d, const void *key, uint64_t hash, dictEntry **existing)
{
    unsigned long idx, table;
    dictEntry *he;
    if (existing)
        *existing = NULL;

    /* Expand the hash table if needed */
    if (_dictExpandIfNeeded(d) == DICT_ERR)
        return -1;
    for (table = 0; table <= 1; table++)
    {
        idx = hash & d->ht[table].sizemask;
        /* Search if this slot does not already contain the given key */
        he = d->ht[table].table[idx];
        while (he)
        {
            if (key == he->key || dictCompareKeys(d, key, he->key))
            {
                if (existing)
                    *existing = he;
                return -1;
            }
            he = he->next;
        }
        if (!dictIsRehashing(d))
            break;
    }
    return idx;
}

void dictEmpty(dict *d, void(callback)(void *))
{
    _dictClear(d, &d->ht[0], callback);
    _dictClear(d, &d->ht[1], callback);
    d->rehashidx = -1;
    d->iterators = 0;
}

void dictEnableResize(void)
{
    dict_can_resize = 1;
}

void dictDisableResize(void)
{
    dict_can_resize = 0;
}

uint64_t dictGetHash(dict *d, const void *key)
{
    return dictHashKey(d, key);
}

/* Finds the dictEntry reference by using pointer and pre-calculated hash.
 * oldkey is a dead pointer and should not be accessed.
 * the hash value should be provided using dictGetHash.
 * no string / key comparison is performed.
 * return value is the reference to the dictEntry if found, or NULL if not found. */
dictEntry **dictFindEntryRefByPtrAndHash(dict *d, const void *oldptr, uint64_t hash)
{
    dictEntry *he, **heref;
    unsigned long idx, table;

    if (dictSize(d) == 0)
        return NULL; /* dict is empty */
    for (table = 0; table <= 1; table++)
    {
        idx = hash & d->ht[table].sizemask;
        heref = &d->ht[table].table[idx];
        he = *heref;
        while (he)
        {
            if (oldptr == he->key)
                return heref;
            heref = &he->next;
            he = *heref;
        }
        if (!dictIsRehashing(d))
            return NULL;
    }
    return NULL;
}

/* ------------------------------- Debugging ---------------------------------*/

#define DICT_STATS_VECTLEN 50
size_t _dictGetStatsHt(char *buf, size_t bufsize, dictht *ht, int tableid)
{
    unsigned long i, slots = 0, chainlen, maxchainlen = 0;
    unsigned long totchainlen = 0;
    unsigned long clvector[DICT_STATS_VECTLEN];
    size_t l = 0;

    if (ht->used == 0)
    {
        return snprintf(buf, bufsize,
                        "No stats available for empty dictionaries\n");
    }

    /* Compute stats. */
    for (i = 0; i < DICT_STATS_VECTLEN; i++)
        clvector[i] = 0;
    for (i = 0; i < ht->size; i++)
    {
        dictEntry *he;

        if (ht->table[i] == NULL)
        {
            clvector[0]++;
            continue;
        }
        slots++;
        /* For each hash entry on this slot... */
        chainlen = 0;
        he = ht->table[i];
        while (he)
        {
            chainlen++;
            he = he->next;
        }
        clvector[(chainlen < DICT_STATS_VECTLEN) ? chainlen : (DICT_STATS_VECTLEN - 1)]++;
        if (chainlen > maxchainlen)
            maxchainlen = chainlen;
        totchainlen += chainlen;
    }

    /* Generate human readable stats. */
    l += snprintf(buf + l, bufsize - l,
                  "Hash table %d stats (%s):\n"
                  " table size: %ld\n"
                  " number of elements: %ld\n"
                  " different slots: %ld\n"
                  " max chain length: %ld\n"
                  " avg chain length (counted): %.02f\n"
                  " avg chain length (computed): %.02f\n"
                  " Chain length distribution:\n",
                  tableid, (tableid == 0) ? "main hash table" : "rehashing target",
                  ht->size, ht->used, slots, maxchainlen,
                  (float)totchainlen / slots, (float)ht->used / slots);

    for (i = 0; i < DICT_STATS_VECTLEN - 1; i++)
    {
        if (clvector[i] == 0)
            continue;
        if (l >= bufsize)
            break;
        l += snprintf(buf + l, bufsize - l,
                      "   %s%ld: %ld (%.02f%%)\n",
                      (i == DICT_STATS_VECTLEN - 1) ? ">= " : "",
                      i, clvector[i], ((float)clvector[i] / ht->size) * 100);
    }

    /* Unlike snprintf(), return the number of characters actually written. */
    if (bufsize)
        buf[bufsize - 1] = '\0';
    return strlen(buf);
}

void dictGetStats(char *buf, size_t bufsize, dict *d)
{
    size_t l;
    char *orig_buf = buf;
    size_t orig_bufsize = bufsize;

    l = _dictGetStatsHt(buf, bufsize, &d->ht[0], 0);
    buf += l;
    bufsize -= l;
    if (dictIsRehashing(d) && bufsize > 0)
    {
        _dictGetStatsHt(buf, bufsize, &d->ht[1], 1);
    }
    /* Make sure there is a NULL term at the end. */
    if (orig_bufsize)
        orig_buf[orig_bufsize - 1] = '\0';
}

/* ------------------------------- Benchmark ---------------------------------*/

#ifdef DICT_BENCHMARK_MAIN

#include "sds.h"

uint64_t hashCallback(const void *key)
{
    return dictGenHashFunction((unsigned char *)key, sdslen((char *)key));
}

int compareCallback(void *privdata, const void *key1, const void *key2)
{
    int l1, l2;
    DICT_NOTUSED(privdata);

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2)
        return 0;
    return memcmp(key1, key2, l1) == 0;
}

void freeCallback(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    sdsfree(val);
}

dictType BenchmarkDictType = {
    hashCallback,
    NULL,
    NULL,
    compareCallback,
    freeCallback,
    NULL};

#define start_benchmark() start = timeInMilliseconds()
#define end_benchmark(msg)                                      \
    do                                                          \
    {                                                           \
        elapsed = timeInMilliseconds() - start;                 \
        printf(msg ": %ld items in %lld ms\n", count, elapsed); \
    } while (0);

/* dict-benchmark [count] */
int main(int argc, char **argv)
{
    long j;
    long long start, elapsed;
    dict *dict = dictCreate(&BenchmarkDictType, NULL);
    long count = 0;

    if (argc == 2)
    {
        count = strtol(argv[1], NULL, 10);
    }
    else
    {
        count = 5000000;
    }

    start_benchmark();
    for (j = 0; j < count; j++)
    {
        int retval = dictAdd(dict, sdsfromlonglong(j), (void *)j);
        assert(retval == DICT_OK);
    }
    end_benchmark("Inserting");
    assert((long)dictSize(dict) == count);

    /* Wait for rehashing. */
    while (dictIsRehashing(dict))
    {
        dictRehashMilliseconds(dict, 100);
    }

    start_benchmark();
    for (j = 0; j < count; j++)
    {
        sds key = sdsfromlonglong(j);
        dictEntry *de = dictFind(dict, key);
        assert(de != NULL);
        sdsfree(key);
    }
    end_benchmark("Linear access of existing elements");

    start_benchmark();
    for (j = 0; j < count; j++)
    {
        sds key = sdsfromlonglong(j);
        dictEntry *de = dictFind(dict, key);
        assert(de != NULL);
        sdsfree(key);
    }
    end_benchmark("Linear access of existing elements (2nd round)");

    start_benchmark();
    for (j = 0; j < count; j++)
    {
        sds key = sdsfromlonglong(rand() % count);
        dictEntry *de = dictFind(dict, key);
        assert(de != NULL);
        sdsfree(key);
    }
    end_benchmark("Random access of existing elements");

    start_benchmark();
    for (j = 0; j < count; j++)
    {
        sds key = sdsfromlonglong(rand() % count);
        key[0] = 'X';
        dictEntry *de = dictFind(dict, key);
        assert(de == NULL);
        sdsfree(key);
    }
    end_benchmark("Accessing missing");

    start_benchmark();
    for (j = 0; j < count; j++)
    {
        sds key = sdsfromlonglong(j);
        int retval = dictDelete(dict, key);
        assert(retval == DICT_OK);
        key[0] += 17; /* Change first number to letter. */
        retval = dictAdd(dict, key, (void *)j);
        assert(retval == DICT_OK);
    }
    end_benchmark("Removing and adding");
}
#endif
