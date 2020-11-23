/* A simple event-driven programming library. Originally I wrote this code
 * for the Jim's event-loop (Jim is a Tcl interpreter) but later translated
 * it in form of a library for easy reuse.
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

#ifndef __AE_H__
#define __AE_H__

#include "monotonic.h"

#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0       /* No events registered. */
#define AE_READABLE 1   /* Fire when descriptor is readable. */
#define AE_WRITABLE 2   /* Fire when descriptor is writable. */ 
/*
 * With WRITABLE, never fire the event if the READABLE event already fired in the same event loop iteration.
 * Useful when you want to persist things to disk before sending replies, 
 * and wantto do that in a group fashion.*/
#define AE_BARRIER 4 
  

#define AE_FILE_EVENTS (1<<0)
#define AE_TIME_EVENTS (1<<1)
#define AE_ALL_EVENTS (AE_FILE_EVENTS|AE_TIME_EVENTS)
#define AE_DONT_WAIT (1<<2)
#define AE_CALL_BEFORE_SLEEP (1<<3)
#define AE_CALL_AFTER_SLEEP (1<<4)

#define AE_NOMORE -1
#define AE_DELETED_EVENT_ID -1

/* Macros */
#define AE_NOTUSED(V) ((void) V)

struct aeEventLoop;

/* Types and data structures */
/* 事件接口 */
// 文件事件处理
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);

// 时间事件处理
typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);

// 事件处理之后需要做的处理
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);

// 事件处理之后前要做的处理
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);

/* File event structure */
/* 文件事件结构 */
typedef struct aeFileEvent {
    // 可监听事件的掩码(可选项 ：ae_readable | ae_writable | ae_barrier)
    int mask; /* one of AE_(READABLE|WRITABLE|BARRIER) */

    // 读事件处理器
    aeFileProc *rfileProc;

    // 写事件处理器
    aeFileProc *wfileProc;

    // 多路复用私有数据
    void *clientData;
} aeFileEvent;

/* Time event structure */
/* 时间事件结构 */
typedef struct aeTimeEvent {
    // 时间事件id，是时间事件唯一标识符
    long long id; /* time event identifier. */

    // 时间事件的到达时间
    monotime when;

    // 时间事件处理函数
    aeTimeProc *timeProc;

    // 时间事件释放函数
    aeEventFinalizerProc *finalizerProc;

    // 多路复用私有数据
    void *clientData;

    // 指向上一个时间事件(前行指针)
    struct aeTimeEvent *prev;
     // 指向下一个时间事件(后续指针)
    struct aeTimeEvent *next;

    // 引用计数： 防止在递归时间事件调用时，时间事件被释放
    int refcount; /* refcount to prevent timer events from being
  		   * freed in recursive time event calls. */
} aeTimeEvent;

/* A fired event */
/* 就绪事件 */
typedef struct aeFiredEvent {
    // 就绪的文件描述符id
    int fd;

    // 事件类型的掩码(可选项 ：ae_readable | ae_writable | ae_barrier)
    int mask;
} aeFiredEvent;

/* State of an event based program */
/* 事件处理器状态 */
typedef struct aeEventLoop {
    // 目前已注册的最大文件描述符数
    int maxfd;   /* highest file descriptor currently registered */

    // 目前已追踪的最大文件描述符数
    int setsize; /* max number of file descriptors tracked */

    // 用于生成时间事件的ID
    long long timeEventNextId;

    // 注册事件
    aeFileEvent *events; /* Registered events */

    // 就绪事件
    aeFiredEvent *fired; /* Fired events */

    // 时间事件
    aeTimeEvent *timeEventHead;

    // 事件处理器开关
    int stop;

    // 多路复用库的私有数据
    void *apidata; /* This is used for polling API specific data */

    // 事件处理前要执行的函数
    aeBeforeSleepProc *beforesleep;

    // 时间出来后要执行的函数
    aeBeforeSleepProc *aftersleep;

    // 标识
    int flags;
} aeEventLoop;

/* Prototypes */
aeEventLoop *aeCreateEventLoop(int setsize);
void aeDeleteEventLoop(aeEventLoop *eventLoop);
void aeStop(aeEventLoop *eventLoop);
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData);
void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask);
int aeGetFileEvents(aeEventLoop *eventLoop, int fd);
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc);
int aeDeleteTimeEvent(aeEventLoop *eventLoop, long long id);
int aeProcessEvents(aeEventLoop *eventLoop, int flags);
int aeWait(int fd, int mask, long long milliseconds);
void aeMain(aeEventLoop *eventLoop);
char *aeGetApiName(void);
void aeSetBeforeSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *beforesleep);
void aeSetAfterSleepProc(aeEventLoop *eventLoop, aeBeforeSleepProc *aftersleep);
int aeGetSetSize(aeEventLoop *eventLoop);
int aeResizeSetSize(aeEventLoop *eventLoop, int setsize);
void aeSetDontWait(aeEventLoop *eventLoop, int noWait);

#endif
