/* adlist.c - A generic doubly linked list implementation
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * listRelease(), but private value of every node need to be freed
 * by the user before to call listRelease(), or by setting a free method using
 * listSetFreeMethod.
 *
 * On error, NULL is returned. Otherwise the pointer to the new list. */
/**
 * 创建链表
 * 分配内存失败时返回 NULL
 * 执行成功返回构造的链表指针
*/
list *listCreate(void)
{
    struct list *list;

    //分配内存
    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;

    //初始化属性
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}

/* Remove all the elements from the list without destroying the list itself. */
/* 释放链表中所有节点，将链表置为空*/
void listEmpty(list *list)
{
    unsigned long len;
    listNode *current, *next;

    current = list->head; //链表头节点
    len = list->len;
    // 遍历整个链表
    while (len--)
    {
        next = current->next;
        // 若链表的值释放函数不空，则调用它
        if (list->free)
            list->free(current->value);
        zfree(current); //释放该空间
        current = next;
    }
    list->head = list->tail = NULL; //将链表置为空
    list->len = 0;
}

/* Free the whole list.
 *
 * This function can't fail. */
/**
 * 释放链表所有节点和整个链表
*/
void listRelease(list *list)
{
    listEmpty(list); // 释放链表节点
    zfree(list);     // 释放链表结构
}

/* Add a new node to the list, to head, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */
/**
 * 将一个包含给定指针作为value的新节点添加到链表头
 * 若为新节点分配内存失败，则不执行任何操作，返回 NULL
 * 若执行成功，返回此传入的链表指针
 * T = O(1)
*/
list *listAddNodeHead(list *list, void *value)
{
    listNode *node;

    // 分成内存
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;

    // 保存节点值
    node->value = value;
    // 若链表为空
    if (list->len == 0)
    {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    }
    else // 链表非空
    {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }
    //更新节点数量
    list->len++;
    return list;
}

/* Add a new node to the list, to tail, containing the specified 'value'
 * pointer as value.
 *
 * On error, NULL is returned and no operation is performed (i.e. the
 * list remains unaltered).
 * On success the 'list' pointer you pass to the function is returned. */

/**
 * 将一个包含给定指针作为value的新节点添加到链表尾
 * 若为新节点分配内存失败，则不执行任何操作，返回 NULL
 * 若执行成功，返回此传入的链表指针
 * T = O(1)
*/
list *listAddNodeTail(list *list, void *value)
{
    listNode *node;

    // 分配内存·
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;
    // 保存value 指针
    node->value = value;
    // 链表为空
    if (list->len == 0)
    {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    }
    //链表非空
    else
    {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    //更新链表节点数量
    list->len++;
    return list;
}

/**
 * 插入一个值为 value 的指针，将其插入到 old_node 节点 之前或之后
 * after 用来判断方向
 * after == 0，表示正向插入，插入到 old_node 之后
 * after == 1，表示逆向插入，插入到 old_node 之前
 * 
 * T = O(1)
*/
list *listInsertNode(list *list, listNode *old_node, void *value, int after)
{
    listNode *node;

    // 分配空间
    if ((node = zmalloc(sizeof(*node))) == NULL)
        return NULL;

    // 保存 value 值
    node->value = value;
    if (after) // 正向插入，将新节点插入到给定节点之后
    {
        node->prev = old_node;
        node->next = old_node->next;
        //若给定节点是尾节点
        if (list->tail == old_node)
        {
            list->tail = node;
        }
    }
    else // 逆向插入，将新节点插入到给定节点之前
    {
        node->next = old_node;
        node->prev = old_node->prev;
        //若给定节点是头结点
        if (list->head == old_node)
        {
            list->head = node;
        }
    }

    //更新节点前置指针
    if (node->prev != NULL)
    {
        node->prev->next = node;
    }
    //更新节点的后置指针
    if (node->next != NULL)
    {
        node->next->prev = node;
    }
    //更新节点数量
    list->len++;
    return list;
}

/* Remove the specified node from the specified list.
 * It's up to the caller to free the private value of the node.
 *
 * This function can't fail. */
/**
 * 从给定链表中删除指定节点
 * 对于节点的私有值的释放工作又调用者进行
 * T = O(1)
*/
void listDelNode(list *list, listNode *node)
{
    // 调整前置节点的指针
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;

    // 调整后置节点的值
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;

    // 释放该节点的值
    if (list->free)
        list->free(node->value);

    // 释放该节点
    zfree(node);
    //更新节点数
    list->len--;
}

/* Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list.
 *
 * This function can't fail. */
/**
 * 为给定链表创建一个迭代器：
 * 之后每次调用 listNext() 则返回该链表的下一个元素
*/
listIter *listGetIterator(list *list, int direction)
{
    listIter *iter;

    // 为迭代器分配内存
    if ((iter = zmalloc(sizeof(*iter))) == NULL)
        return NULL;

    // 根据迭代方向，设置迭代器的起点
    if (direction == AL_START_HEAD) // 正向迭代器
        iter->next = list->head;
    else // 逆向迭代器
        iter->next = list->tail;
    // 设置迭代方向
    iter->direction = direction;
    return iter;
}

/* Release the iterator memory */
/* 释放迭代器*/
void listReleaseIterator(listIter *iter)
{
    zfree(iter);
}

/* Create an iterator in the list private iterator structure */
/**
 *  将迭代器重置为正序迭代器 al_start_head 
 *  将迭代器的方向设置为 al_start_head
 *  并将迭代指针指向表头节点
 *  T = O(1)
 */
void listRewind(list *list, listIter *li)
{
    li->next = list->head;         // 重置迭代器指针为头结点
    li->direction = AL_START_HEAD; // 重置迭代器方向 正向
}
/**
 *  将迭代器重置为正序迭代器 al_start_tail
 *  将迭代器的方向设置为 al_start_tail
 *  并将迭代指针指向表尾节点
 *  T = O(1)
 */
void listRewindTail(list *list, listIter *li)
{
    li->next = list->tail;         // 重置迭代器指针为尾节点
    li->direction = AL_START_TAIL; // 重置迭代器方向 逆向
}

/* Return the next element of an iterator.
 * It's valid to remove the currently returned element using
 * listDelNode(), but not to remove other elements.
 *
 * The function returns a pointer to the next element of the list,
 * or NULL if there are no more elements, so the classical usage
 * pattern is:
 *
 * iter = listGetIterator(list,<direction>);
 * while ((node = listNext(iter)) != NULL) {
 *     doSomethingWith(listNodeValue(node));
 * }
 *
 * */
/**
 * 返回迭代器当前所指向的节点
 * 删除当前节点是运行的，但不能修改链表的其他节点
*/
listNode *listNext(listIter *iter)
{
    listNode *current = iter->next;

    if (current != NULL)
    {
        // 根据方向选择下一个节点
        if (iter->direction == AL_START_HEAD)
            iter->next = current->next; // 保存下一个节点，防止当前节点被删除而导致指针丢失
        else
            iter->next = current->prev;
    }
    return current;
}

/* Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 *
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of
 * the original node is used as value of the copied node.
 *
 * The original list both on success or error is never modified. */
/**
 * 复制整个链表
 * 若因为内存不足而造成复制失败，返回NULL
 * 若链表有设置值复制函数 dup，则使用该函数进行复制
 * 否则，新旧节点将共享一个指针
 * 
 * 执行失败返回NULL 
 * 执行成功返回复制链表指针 copy
 * 不论成功与否原链表都不会被修改
 * 
 * T = O(N)
*/
list *listDup(list *orig)
{
    list *copy;
    listIter iter;
    listNode *node;

    // 创建新链表
    if ((copy = listCreate()) == NULL)
        return NULL;

    //设置节点的值处理函数
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;

    // 迭代整个链表
    listRewind(orig, &iter);
    while ((node = listNext(&iter)) != NULL)
    {
        void *value;

        if (copy->dup)
        {
            // 复制节点到新节点
            value = copy->dup(node->value);
            if (value == NULL)
            {
                listRelease(copy);
                return NULL;
            }
        }
        else
            value = node->value;

        // 将节点添加到链表中
        if (listAddNodeTail(copy, value) == NULL)
        {
            listRelease(copy);
            return NULL;
        }
    }
    return copy;
}

/* Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly
 * compared with the 'key' pointer.
 *
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists
 * NULL is returned. */
/**
 * 根据给定的值指针，寻找值相等的链表节点
 * 比对操作由链表的 match 函数负责进行
 * 若没有 match 函数，那么直接通过对比值的比对函数来决定是否匹配。
 * 返回值:
 *     成功找到时，返回第一个值相等的链表节点指针(正序)
 *     未找到时，但会 NULL
*/
listNode *listSearchKey(list *list, void *key)
{
    listIter iter;
    listNode *node;

    // 正向迭代整个链表
    listRewind(list, &iter);

    while ((node = listNext(&iter)) != NULL)
    {
        //对比
        if (list->match)
        {
            //找到节点
            if (list->match(node->value, key))
            {
                return node;
            }
        }
        else
        { // 直接比对
            if (key == node->value)
            {
                return node;
            }
        }
    }

    // 未找到
    return NULL;
}

/* Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 the penultimate
 * and so on. If the index is out of range NULL is returned. */
/**
 * 返回链表给定索引的值
 * 索引从 0 开始，表示是 head， 也可以是否负数，-1表示是最后一个节点 
 * 索引的范围是 [-len, len-1]
 * 若索引超过范围，则返回 NULL
 * 
 * T = O(N)
*/
listNode *listIndex(list *list, long index)
{
    listNode *n;

    // 如果索引为负数，从表尾开始查找
    if (index < 0)
    {
        index = (-index) - 1;
        n = list->tail;
        while (index-- && n)
            n = n->prev;
    }
    // 索引为正数，从表头开始查找
    else
    {
        n = list->head;
        while (index-- && n)
            n = n->next;
    }
    return n;
}

/* Rotate the list removing the tail node and inserting it to the head. */
/**
 * 取出链表的尾节点，并将其移动到表头节点，成为新的表头节点
 * T = O(1)
*/
void listRotateTailToHead(list *list)
{
    if (listLength(list) <= 1)
        return;

    /* Detach current tail */
    /* 取下表尾节点 */
    listNode *tail = list->tail;
    list->tail = tail->prev;
    list->tail->next = NULL;

    /* Move it as head */
    /* 将表尾节点置为头节点 */
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}

/* Rotate the list removing the head node and inserting it to the tail. */
/**
 * 取出链表的头节点，并将其移动到表尾节点，成为新的表尾节点
 * T = O(1)
*/
void listRotateHeadToTail(list *list)
{
    if (listLength(list) <= 1)
        return;

    listNode *head = list->head;
    /* Detach current head */
    /* 取下表头节点 */
    list->head = head->next;
    list->head->prev = NULL;

    /* Move it as tail */
    /* 将表头节点置为尾节点 */
    list->tail->next = head;
    head->next = NULL;
    head->prev = list->tail;
    list->tail = head;
}

/* Add all the elements of the list 'o' at the end of the
 * list 'l'. The list 'other' remains empty but otherwise valid. */
/**
 * 将链表 o 拼接到链表l 之后，并将链表 o 置为空链表，但是 o 链表并未释放。
*/
void listJoin(list *l, list *o)
{
    if (o->head)
        o->head->prev = l->tail;

    if (l->tail)
        l->tail->next = o->head;
    else
        l->head = o->head;

    if (o->tail)
        l->tail = o->tail;
    l->len += o->len;

    /* Setup other as an empty list. */
    o->head = o->tail = NULL; //将 o 链表置空
    o->len = 0;
}
