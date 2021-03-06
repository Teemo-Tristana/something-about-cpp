如何阅读Redis源码
==============================================
+ 1. 阅读数据结构的实现：
    + 相关文件：
        - sds.h     sds.c
        - adlist.h  adlist.c
        - dict.h    dict.c
        - redis.h   zskiplist zskiplistNode t_zset.c 所有zsl开头的函数如zslCreate, zslInsert, zslDeleteNode
        - hyperloglog.c 中hlhdr结构，以及所有hll开头的文件

+ 2. 阅读内存编码[encoding]数据结构的实现
    + 和普通的数据结构一样， 内存编码数据结构基本上是独立的， 不和其他模块耦合
        - intset.h intset.c[整数集合intset数据结构]
        - ziplist.h ziplist.c[压缩列表zip-list数据结构]

+ 3. 阅读数据类型实现：
    + Redis6种不同类型的键[字符串-散列-列表-集合-有序集合-HyperLogLog]所有底层实现结构
        - object.c Redis对象类型系统实现
        - t_string.c 字符串键的实现
        - t_list.c 列表键的实现
        - t_hash.c 散列键的实现
        - t_set.c 集合键的实现
        - t_zset.c中除zsl开头的函数之外的所有函数 有序集合键的实现
        - hyperloglog.c 中所有以pf开头的函数

+ 4. 阅读数据库相关实现代码:
    + 数据库相关
        - redis.h中redisDb结构和db.c  Redis数据库实现
        - notify.c                   Redis的数据库通知功能实现代码
        - rdb.h和rdb.c               Redis的RDB持久化实现代码
        - aof.c                      Redis的AOF持久化实现代码

    + Redis独立功能模块：
        - redis.h中pubsubPattern结构以及pubsub.c   发布与阅读功能实现
        - redis.h中multiState结构以及multiCmd结构   事务功能的实现
        - sort.c                                   SORT命令的实现
        - bitops.c                                 GETBIT|SETBIT等二进制操作命令的实现

+ 5. 阅读客户端和服务器相关代码：
    + 客户端-服务器代码：
        - ae.c 以及任意一个ae_*.c文件           Redis的事件处理器实现[基于Reactor模式]
        - networking.c                         Redis的网络链接库，负责发送回复和接受命令请求，同时也负责创建/销毁客户端以及分析通信协议等工作
        - redis.h 和 redis.c中和单机Redis服务器相关部分     单机Redis服务器的实现
    
    + 独立模块:
        - scripting.c   Lua脚本功能实现
        - slowlog.c     慢查询功能的实现
        - monitor.c     监视器功能的实现

+ 6. 阅读多机功能的实现
    +  多级功能代码：
        - replication.c     复制功能的实现
        - sentinel.c        Redis Sentinel的实现代码
        - cluster.c         Redis集群的实现代码
    ![readRedis.png](readRedis.png)

