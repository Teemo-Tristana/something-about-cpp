/* adlist.h - A generic doubly linked list implementation
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */

/**
 * Redis使用的是C语言，而C语言没有原生的链表，因此Redis自己实现了双向链表
*/
/**
 * 双向链表节点
*/
typedef struct listNode
{
    struct listNode *prev; // 前置节点
    struct listNode *next; // 后置节点
    void *value;           // 节点值
} listNode;

/**
 * 双向链表迭代器
*/
typedef struct listIter
{
    listNode *next; // 当前迭代器的节点
    int direction;  // 迭代的方向
} listIter;

/**
 * 双端链表结构
*/
typedef struct list
{
    listNode *head;                     // 链表头节点
    listNode *tail;                     // 链表尾节点
    void *(*dup)(void *ptr);            // 节点值复制函数，指针函数指针
    void (*free)(void *ptr);            // 节点值释放函数, 函数指针
    int (*match)(void *ptr, void *key); // 节点值对比函数, 函数指针
    unsigned long len;                  // 链表所包含的节点数量
} list;

/* Functions implemented as macros */
/* 宏定义的函数 */
#define listLength(l) ((l)->len)      // 返回链表的节点数量
#define listFirst(l) ((l)->head)      // 返回链表的头节点
#define listLast(l) ((l)->tail)       // 返回链表的尾部节点
#define listPrevNode(n) ((n)->prev)   // 返回给定节点的前置节点
#define listNextNode(n) ((n)->next)   // 返回给定节点的后置节点
#define listNodeValue(n) ((n)->value) // 返回给定节点的当前值

#define listSetDupMethod(l, m) ((l)->dup = (m))     // 将链表的值复制函数设置为 m
#define listSetFreeMethod(l, m) ((l)->free = (m))   // 将链表的值释放函数设置为 m
#define listSetMatchMethod(l, m) ((l)->match = (m)) // 将链表的值比对函数设置为 m

#define listGetDupMethod(l) ((l)->dup)     // 返回链表的值复制函数
#define listGetFreeMethod(l) ((l)->free)   // 返回链表的值释放函数
#define listGetMatchMethod(l) ((l)->match) // 返回链表的值比对函数

/* Prototypes */
/* 函数原型 */
list *listCreate(void);                                                       //创建链表
void listRelease(list *list);                                                 //释放链表
void listEmpty(list *list);                                                   // 链表置为空
list *listAddNodeHead(list *list, void *value);                               //添加头节点
list *listAddNodeTail(list *list, void *value);                               //添加尾节点
list *listInsertNode(list *list, listNode *old_node, void *value, int after); //在中间位置插入节点
void listDelNode(list *list, listNode *node);                                 //删除节点
listIter *listGetIterator(list *list, int direction);                         //获取迭代器
listNode *listNext(listIter *iter);                                           // 返回迭代器当前所指节点
void listReleaseIterator(listIter *iter);                                     //释放迭代器
list *listDup(list *orig);                                                    //复制链表
listNode *listSearchKey(list *list, void *key);                               // 在链表中查找key值
listNode *listIndex(list *list, long index);                                  //索引函数
void listRewind(list *list, listIter *li);                                    //将给定迭代器重置为正序迭代器
void listRewindTail(list *list, listIter *li);                                // 将给定迭代器重置为逆序迭代器
void listRotateTailToHead(list *list);                                        //旋转链表，删除尾节点并将其插入到头部
void listRotateHeadToTail(list *list);                                        //旋转链表，删除头节点并将其插入尾部
void listJoin(list *l, list *o);                                              //将链表 o 拼接到链表 l 之后，随后将链表 o 置为空链表，但是并未释放链表指针。

/* Directions for iterators */
/* 迭代器进行迭代的方向 */
#define AL_START_HEAD 0 // 正向迭代器,从头到位进行迭代
#define AL_START_TAIL 1 // 逆向迭代器,从尾到头进行迭代

#endif /* __ADLIST_H__ */
