#暂时计划阅读的C/C++框架和库
## WebBecnh
+ Webbench是一个在linux下使用的非常简单的网站压测工具。它使用fork()模拟多个客户端同时访问我们设定的URL，测试网站在压力下工作的性能，最多可以模拟3万个并发连接去测试网站的负载能力。Webbench使用C语言编写, 代码实在太简洁，源码加起来不到600行。

## Tinyhttpd
+ tinyhttpd是一个超轻量型Http Server，使用C语言开发，全部代码只有502行(包括注释，附带一个简单的Client，可以通过阅读这段代码理解一个 Http Server 的本质。

## cJSON 
+ cJSON是C语言中的一个JSON编解码器，非常轻量级，C文件只有500多行，速度也非常理想。cJSON也存在几个弱点，虽然功能不是非常强大，但cJSON的小身板和速度是最值得赞赏的。其代码被非常好地维护着，结构也简单易懂，可以作为一个非常好的C语言项目进行学习。

## Memcached
+ Memcached 是一个高性能的分布式内存对象缓存系统，用于动态Web应用以减轻数据库负载。它通过在内存中缓存数据和对象来减少读取数据库的次数，从而提供动态数据库驱动网站的速度。Memcached 基于一个存储键/值对的 hashmap。Memcached-1.4.7的代码量还是可以接受的，只有10K行左右。

## libevent
+ libevent是一个开源的事件驱动库，基于epoll，kqueue等OS提供的基础设施。其以高效出名，它可以将IO事件，定时器，和信号统一起来，统一放在事件处理这一套框架下处理。基于Reactor模式，效率较高，并且代码精简（4.15版本8000多行），是学习事件驱动编程的很好的资源。
+ 参考： https://blog.csdn.net/luotuo44/category_2435521.html
+ 官网： https://libevent.org/

## LevelDb
+ LevelDb是谷歌两位大神级别的工程师发起的开源项目
    - LevelDb是能够处理十亿级别规模Key-Value型数据持久性存储的C++程序库。它是一个持久化存储的KV系统，和Redis这种内存型的KV系统不同，LevelDb不会像Redis一样狂吃内存，而是将大部分数据存储到磁盘上。
    - 其次，LevleDb在存储数据时，是根据记录的key值有序存储的，就是说相邻的key值在存储文件中是依次顺序存储的，而应用可以自定义key大小比较函数，LevleDb会按照用户定义的比较函数依序存储这些记录。

    - 主页:https://github.com/google/leveldb