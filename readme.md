* C++基础问题：

    - 语言基础

        - struct 结构体大小[注意对齐]：
            - example01.cpp
            - struct整体大小是最宽类型成员的整数倍

        - union联合体[共享内存]
            - 大小等于成员中宽度最长的成员大小

        - enum 
            - 与成员类型相关

        - sizeof() 运算符

        - 内存对齐:
            - 原因： 提高效率
            - eg :

                 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
                 未对齐： 假如某int数据是从1-4，则首先读取0-3，去掉0，然后读取4-7，去掉5，6，7，这样访问一个数据就读取了2次，最后还需要1-3与4合并，等操作
                 对齐： 直接从去0-3或4-7既可一次把数据读取，无需合并

            - 现代计算机内存空间一般都是安装byte划分，从理论上来说，对任何类型变量的访问可以从任何位置开始，但实际上OS对数据在内存中存放位置有所限制，要求首地址必须是某个数k[内存存取粒度]（4|8)的倍数，这就是内存对齐。

        - 指针：
            - example03.cpp
            - 概念： 是一个变量，用于存储内存地址
            + 野指针：指向不可用的内存地址的指针[野指针不是NULL指针]，是指向"垃圾” 内存的指针
                - example02.cpp
                - 原因1： 指针没有初始化
                - 原因2： 指针被free/delete 后没有置空[NULL|nullptr],z只是把指针指向的内存释放，但没有把指针自身释放
                - 原因3： 指针操作超越了变量的作用范围
        - 空指针 ： NULL | nullptr
            - 空指针是一个特殊的指针值，也是唯一一个对任何指针类型都合法的指针值
        - pointer | const 
            + 常量指针[read-only] ： int const *p | const int *p
                - 指针p可变，可以指向其他内存地址
                - 指针p指向的内存地址中的数据不能变[不能通过p修改其内存中的值]，比如 *p = 2 ❌， p = &b, ✔

            + 指针常量 ： int * const p 
                - 指针是一个常量，指针不能表[指针p只能指向该地址]
                - 指针指向的内存地址的数据是可以表的 *p = 2 ✔ , p = &b ❌

            + 指向常量的常量指针：const int * const p 
                - 指向常量的指针是一个常量，且指向的对象也是一个常量
                - 指针不能改，只能指向该地址      *p = 2 ❌ , p = &b ❌
                - 指针指向的内存地址中的数据不能改 *p = 2 ❌ , p = &b ❌

        - 内存泄漏：
            - 用动态存储分配函数动态开辟的空间，在使用完毕后未释放，结果导致一直占据该内存单元。直到程序结束。即所谓内存泄漏[内存泄漏是指堆内存的泄漏]

        - 智能指针

        - new/delete | mallocl|free

        - 右值引用
        - std::move实现及原理

        - const关键字

        - typename 

        - RAII 
        - lamada

    - C++ 内存布局
        + 堆
        + 栈
    - class 与 strcut 区别
    - class相关问题：
        - 空类大小
        - 含成员变量(函数)的大小
        - 含静态成员(函数)的大小
        - const 与 static
        - this指针

    - 重载|重写

    - 多态与虚函数：
        + 多态：
            - 静态联编
            - 动态联编

        

        + 虚函数
            - 虚函数表vpbl
            - 虚指针vptr

            + 纯虚函数 与 抽象类

    - 继承 

    - C++11新特性

        

    - STL
        + 顺序容器：
            + vector
                + 底层原理， resize和reserve方法及原理
            + deque

        + 关联容器：
            + map
            + multimap
            + unordered_map

            + set 
            + multiset
            + unordered_set

        + 容器适配器：

* 数据结构  
    - 链表
    - 队列
    - 堆
    - 栈 
    - 树 
        + 二叉树 
        + 红黑树[red-black-tree]
    - 图 

    

    - 哈希[hash]及hash冲突解决方法
    - 排序
        + [经典的排序算法及其时间|空间复杂度]
    - 查找 
    - 二分 
    - kmp 及其复杂度
    - B+树
    - 线段树

        

* 计算机原理知识点：
    - 32|64位系统字节数： 

        类型                            32位                                     64位
        char                            1                                        1             Bytes
        short                           2                                        2
        int                             4                                        4|8
        long                            4                                        8
        float                           4                                        4
        double                          8                                        8
        指针                            4                                        8

* 操作系统：
    - 调度算法
    - 死锁 
    - 进程|线程|协程
        + **多进程通信**
            + fork ：
                + 创建子进程
                    -  成功：
                        + 每次系统调用返回两次 ： 可以用来判断当前进程是子进程还是父进程
                            - 在父进程中返回子进程的PID 
                            - 在子进程中返回 0   
                    - 失败：
                        - 返回 0 并设置 errno

                + fork()函数复制当前进程，在内核进程表中创建一个新的进程表项并进行相应的设置：
                    - 该进程[子进程]的PPID设置为原来的PID 
                    - 信号图被清除[原来进程设在的信号对新进程不再起作用]

                + 子进程和父进程完全相同[复制父进程的数据]：
                    - 复制父进程的堆栈缎，数据段， [共享代码段]
                    - 写时复制[copying-on-write] : **只有任一[父/子进程]对数据进行写操作时复制**
                        - 过程：  缺页中断 -> os给子进程分配农内存空间并复制父进程数据 -> 其他操作

                + 创建子进程：
                    - 父进程已打开的文件描述符 引用计数 +1
                    - 父进程的用户根目录，当前工作目录 引用计数 +1

            + 僵尸进程|孤儿进程 ： 都是父进程未正确处理子进程的返回， 孤儿进程和僵尸进程未被正确处理，会继续占用内核资源资源
                + 僵尸进程： 子进程正常/异常退出， 而父进程未处理

                + 孤儿进程： 父进程结束/异常推出， 而子进程继续运行
                    - 孤儿进程的将被**init进程[PID=1]接收**

                + 处理方法：
                    - **wait()** : 阻塞 直到某个子进程结束为止[返回子进程的PID]
                    - **waitpid()** ：非阻塞，只等待由pid等待的子进程，pid=-1时就退化为wait[阻塞式等待任意一个子进程的结束]
                        -  options = WNOHANG
                            - 子进程未结束|意外终止 返回0
                            - 子进程正常退出       返回子进程PID
                            - 失败返回-1, 并设置errno 

            + 管道： 单项通信[一端读一端写] fd[0] 读端 fd[1]写端
                + 无名管道PIPE ：只能用于有关联的进程间通信
                    + 管道常用于父子进程间通信
                        eg：
                         _____________                                                  _____________ 
                        |             |------close(fd[0])----------|——————————读————————|            |
                        |   父进程     |---------------------|      |————————————————————|   子进程    |
                        |             |—————————————————————|      |--------------- ----|            |
                        |_____________|___________写_______________|----close(fd[1])----|____________| 
               
                + 有名管道FIFO： 可以用于无关联的进程通信[网络编程用得较少]
                
            

            + **3种system V的进程间通信方式**：

                **以下三种system_V的进程间通信都是使用一个全局唯一的键值[key]来描述一个共享资源**
                + 信号量Semaphore：
                    - 等待wait[P-Passeren]    :  若信号量SV > 0, SV -1， SV = 0, 挂起   
                    - 信号signal[V-Vrijigeven]:  若有因等待semaphore而挂起的,,则唤醒， 否则 sv +1

                    + 信号量API 
                        - 头文件 sys/sem.h
                        - semid_ds 结构体
                        + semget() : 创建或获取信号量集合
                            - 成功： 正整数[信号集合标识符] 失败：-1 并设置errno
                            - 特殊键值 IPC_PRIVATE：
                                - 无论信号集合存在与否， semget()都直接从一个新的信号量
                                - 并非似有，其他进程包含子进程都可以访问该信号量

                        + semop() ： 系统调用改变信号量的值[执行P|V]
                        + semctl():  对信号量直接控制
                    
                    + **信号量部分含有大量的结构体，需要仔细查看，见书<<Linux高性能服务器编程>>第13章**

                
                + 共享内存[share_memory]:
                    - 共享内存是最高效的IPC机制：不同进程映射到同一块内存区域，不涉及数据传输问题，
                    - 共享内存需要用其他辅助手段进行辅助同步内存的访问，避免产生竞争

                    + share_memory的API：
                        - shmget() :
                            - 创建或获取已存在的一段共享内存
                            - 成功 ： 返回共享内存标识符   失败： 返回-1,并设置errno
                            - 创建时，相应的共享内存段会被初始化为0
                            - **结构体 shmid_ds**



                        - shmat() :
                            - 将创建|获取的共享内存关联到进程的地址空间

                        - shmdt()：
                            - 将共享内存从地址空间分离出来

                        - shmctl()：
                            - 控制共享内存的某些属性


                        """
                        + 共享内存的POSIX方法：
                            - mmap() 利用MAP_ANONYMOUS标志可以实现父子进程间的匿名内存共享
                            - shm_open() + mmap 实现无关进程之间的共享内存
                                - shm_open() 调用成功返回一个文件描述符 
                                - mmap()调用该文件描述符实现共享内存关联到调用进程

                            - shm_inlink()将共享内存对象删除
                        """

                + 消息队列： 两个进程间传递二进制数据
                    + 消息队列的API：
                        - msgget(): 
                            - 创建或获取一个消息队列[创建时将被初始化]
                        
                        - msgsnd():
                            - 把一条消息添加到消息队列
                        
                        - msgrcv():
                            - 从消息队列中获条消息

                        - msgctl():
                            - 直接控制消息队列的某些属性


                + 进程间传递一个文件描述符：
                    - 进程间传递一个文件描述符并不是传递一个文件描述符的值，而是在接受文件描述符的进程中**创建一个新的文件描述符**，该文件描述符和原文件描述符指向同一个文件表项
              

        - 多线程
        - 多进程通信
        - 多线程通信
    - STL多线程安全问题

    

* 计算机网络：
    - 帧： 数据链路层封装的数据： 以太帧， 令牌环帧
        - 以太帧：
            - :---------------------------------------------------------------------------

               |   目的物理地址   |   源物理地址   |   类型   |      数据        |   CRC     |
               ---------------------------------------------------------------------------
               |       6Byte     |     6Byte     |   2Byte  |   46-1500Byte   |   4Byte   |
               ----------------------------------------------------------------------------

    - 帧的最大传输单元[Max-Transmit-Unit-MTU]
        - 帧最多能携带上层的数据
            - 以太帧 ： 1500 Byte[IP数据包过程|在传输时会被分配传输]

    - ARP[地址解析协议]： I
        - P地址 -> 物理地址(MAC地址)
        - 原理： 主机向自己所在网络广播一个ARP请求[包含目标机器的网络地址]，网络上所有主机都会收到这个请求，但是只有被请求的目标机器才会回应一个ARP应到[包含自己的物理地址]
        - ARP维护一个高速缓存[包含经常访问的映射]，避免重复ARP请求，提高效率

    - DNS[域名解析协议]: 域名 -> IP 

    - IP协议：
        + IP服务特点：
            - IP服务是上层TCP/IP协议的动力

            - 无状态[stateless]: 通信双方不同步数据的状态信息[IP数据包的发送|传输|接受|均相互独立]
                - 无法处理乱序
                - 无法处理重复
                - IP数据包提供一个标识字段但是是用来处理IP数据包分片和重组的

                - UDP协议和HTTP协议也是无状态的

            - 无连接：IP通信双方不长久地维持对方任何信息[上层每次发送数据时都要加上目的IP地址]

            - 不可靠：
                - 不保IP数据包准确送到，只承诺best effort
                - IP数据包被丢弃是只会返回失败，不会试图重传
                    - IP数据包CRC校验被丢弃
                    - IP数据包超时[TTL]被丢弃 

        + IPv头部结构：

            0                                   15  16                             31
            | 4位版本号| 4位头部长度|   8位服务类型  |           16位总长度              |
            |                  16位标识            | 3位标志|       13位片偏移         |
            | 8位生存时间          |  8位协议       |         16位头部校验和            |
            |                              32位源端IP地址                            |
            |                              32位目的IP地址                            |
            |                    可选项  做多 40字节                                  |

            - 4位版本号 ： IPv4 其值是4 
            - 4位头部长度： 标识IP头部有多少个32bit(4Byte)
                - 2^4 - 1 = 15, 因而IP头部最长位 4 x 15 = 60 Byte
            - 8位服务类型[Type-of-Service]:
            - 16位总长度[total-length]： 整个IP数据包的长度，以字节为单位
                - 最大 2^16 - 1 = 65535 Byte 
                - 超过MTU, 需要分片传输
            - 16位标识[identifiation]: 唯一标识主机发送的每一个数据报
                - 初始值随机生成，每发送一个，其值+1
                - 数据报分片时，标识符被复制到每个分片[同一个数据报的所有分片的标识是相同的]
            - 3位标志字段： 
                - 第一位 ： 保留字段
                - 第二位 ： Don't Frgment[DF]禁止分片
                    - 该位是true的话，IP数据包不会被分片，若是IP数据报超过MTU, 则会被丢弃并返回[ICMP差错报文]
                - 第三位 ： More Fragment[MF] 更多分片
                    - 除最后一个分片外，其他分片将该值设为1
            - 13位片偏移[fragmentation offset]：数据部分相对于原始IP数据报开始位置的偏移
                - 左移3位后得到，因此，除了最后一个分片外，其余每个分片的数据部分长度必须是8的倍数
            - 8位生存时间[Time-to-live]：到达目的地址之前最多允许经过的路由器跳数
                - TTL 通常被设为 64
                - 每经过一个路由， TTL - 1
                - TTL  = 0 时，IP数据报被丢弃，并返回[ICMP差错报文]
                - TTL 可以防止数据包陷入路由循环
            - 8位协议[protocol]: 用来区分上层协议

                - 
                  | ICMP  | TCP  | UDP |
                  |   1   |  6   | 17  |

            - 16位头部校验和： 有发送端填充，接收端使用CRC算法检验IP数据报#头部[只检验头部]#
            - 32位源端IP地址 ： 正常情况传输过程中不变
            - 32位目的IP地址 ： 正常情况传输过程中不变

            - 可选字段[option]：最长 40Byte 
            - IP数据报头部最长是60字节 = 20[固定长度] + 40[可选字段]

        

    - TCP|UDP
        + TCP 连接[三次握手-四次挥手]
            - time_wait 状态  及其作用
            - close_wait 状态 及其作用

        

        + 流量空 
        + 拥塞控制

        + TCP 状态图

    

    - HTTP:

    - socket套接字：

    - 多路复用I|O :
        + socket 
        + poll
        + epoll 
            - ET 模式
            - LT 模式
            - oneshot[??]

* Linux相关知识点：
    - 基本命令：
        - ls 
        - cd 
        - pwd 

* 数据库 ：
    - MySql
    - Redis 
    - Mongodb

* 算法
    - leetcode
    - newcode

* 开源第三方库：
    - libevent
