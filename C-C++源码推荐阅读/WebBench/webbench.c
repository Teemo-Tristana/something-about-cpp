/*
* (C) Radim Kolar 1997-2004
* This is free software, see GNU Public License version 2 for
* details.
*
* Simple forking WWW Server benchmark:
*
* Usage:
*   webbench --help
*
* Return codes:
*    0 - sucess
*    1 - benchmark failed (server is not on-line)
*    2 - bad param
*    3 - internal error, fork failed
* 
*/ 

#include "socket.c"
#include <unistd.h>
#include <sys/param.h>
#include <rpc/types.h>
#include <getopt.h>
#include <strings.h>
#include <time.h>
#include <signal.h>

/* values */
volatile int timerexpired=0; // 判断测试是否达到预设时间
int speed=0; // 服务器成功响应的数量
int failed=0; // 失败响应的数量
int bytes=0; // 服务器成功响应进程对读取的字节数，force = 0时才有效

/* globals */
// HTTP协议版本：默认版本 HTTP/1.0
int http10=1; /* 0 - http/0.9, 1 - http/1.0, 2 - http/1.1 */
/* Allow: GET, HEAD, OPTIONS, TRACE */
// HTTP的请求方法，默认是 GET
#define METHOD_GET 0
#define METHOD_HEAD 1
#define METHOD_OPTIONS 2
#define METHOD_TRACE 3
#define PROGRAM_VERSION "1.5"
int method=METHOD_GET;
int clients=1; // 并发数目(创建子进程进程个数)，默认位1个
int force=0; // 是否等待读取 server 返回的 结果(=0表示读取)
int force_reload=0; //是否使用缓存 1不使用 0使用
int proxyport=80;//(代理)端口号
char *proxyhost=NULL; // 代理服务器地址
int benchtime=30; // 测压时间(默认30s)

/* internal */
int mypipe[2]; // 管道，用于父子进程间通信
char host[MAXHOSTNAMELEN]; // 目标主机地址
#define REQUEST_SIZE 2048 /
char request[REQUEST_SIZE]; // 构造发送的HTTP请求

// 选项 getopt_long 的参数
static const struct option long_options[]=
{
    {"force",no_argument,&force,1},
    {"reload",no_argument,&force_reload,1},
    {"time",required_argument,NULL,'t'},
    {"help",no_argument,NULL,'?'},
    {"http09",no_argument,NULL,'9'},
    {"http10",no_argument,NULL,'1'},
    {"http11",no_argument,NULL,'2'},
    {"get",no_argument,&method,METHOD_GET},
    {"head",no_argument,&method,METHOD_HEAD},
    {"options",no_argument,&method,METHOD_OPTIONS},
    {"trace",no_argument,&method,METHOD_TRACE},
    {"version",no_argument,NULL,'V'},
    {"proxy",required_argument,NULL,'p'},
    {"clients",required_argument,NULL,'c'},
    {NULL,0,NULL,0}
};

/* prototypes */
static void benchcore(const char* host,const int port, const char *request);
static int bench(void);
static void build_request(const char *url);

// 定时时间 
static void alarm_handler(int signal)
{
    timerexpired=1;
}	

static void usage(void)
{
    fprintf(stderr,
            "webbench [option]... URL\n"
            "  -f|--force               Don't wait for reply from server.\n"
            "  -r|--reload              Send reload request - Pragma: no-cache.\n"
            "  -t|--time <sec>          Run benchmark for <sec> seconds. Default 30.\n"
            "  -p|--proxy <server:port> Use proxy server for request.\n"
            "  -c|--clients <n>         Run <n> HTTP clients at once. Default one.\n"
            "  -9|--http09              Use HTTP/0.9 style requests.\n"
            "  -1|--http10              Use HTTP/1.0 protocol.\n"
            "  -2|--http11              Use HTTP/1.1 protocol.\n"
            "  --get                    Use GET request method.\n"
            "  --head                   Use HEAD request method.\n"
            "  --options                Use OPTIONS request method.\n"
            "  --trace                  Use TRACE request method.\n"
            "  -?|-h|--help             This information.\n"
            "  -V|--version             Display program version.\n"
           );
}

/**
 * 使用示例：
 * webbench -c 1000 -t 60 http://liuzhichao.com/index.php
 * webbench -c 1000 -t 60 http://172.20.20.214:12345/1
*/
/**
 * 首先进程参数解析
 * 然后调用 build_request() 构建 HTTP 请求头
 * 最后调用 bench()执行测压
*/
int main(int argc, char *argv[])
{
    int opt=0; // getopt_long返回值
    int options_index=0; // getopt_long的第5个参数，一般为0
    char *tmp=NULL;

    if(argc==1) // 若参数个数为 1
    {
        usage();
        return 2;
    } 

    /**
     * getopt_long()读取命令行和解析参数，并设置涉及到的全局变量
     * getopt_long原型： int getopt_long(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex);
    */
    while((opt=getopt_long(argc,argv,"912Vfrt:p:c:?h",long_options,&options_index))!=EOF )
    {
        switch(opt)
        {
            case  0 : break;
            case 'f': force=1;break;
            case 'r': force_reload=1;break; 
            case '9': http10=0;break; // HTTP/0.9
            case '1': http10=1;break; // HTTP/1.0
            case '2': http10=2;break; // HTTP/1.1
            case 'V': printf(PROGRAM_VERSION"\n");exit(0);

            // -t 压力测试时间，需要将optarg转为整数
            case 't': benchtime=atoi(optarg);break;	     
            case 'p': 
            /* proxy server parsing server:port */
            /**
             * char *strrchr(const char *str, int c)
             * 在参数 str 所指向的字符串中搜索 最后一次出现字符 c（一个无符号字符）的位置。
             * 
             * 解析端口 
             * 使用 ":"分割字符串
             * eg：-p 127.0.0.1:1080
             * tmp = 1080
            */
            tmp=strrchr(optarg,':'); // 端口
            proxyhost=optarg; // 主机
            if(tmp==NULL)
            {
                break;
            }
            
            // 判断是否提供了主机名
            if(tmp==optarg)
            {
                fprintf(stderr,"Error in option --proxy %s: Missing hostname.\n",optarg);
                return 2;
            }
            // 判断是否提供端口号
            if(tmp==optarg+strlen(optarg)-1)
            {
                fprintf(stderr,"Error in option --proxy %s Port number is missing.\n",optarg);
                return 2;
            }

            // 获取代理地址
            *tmp='\0'; 
            proxyport=atoi(tmp+1);break;
            case ':':
            case 'h':
            case '?': usage();return 2;break;
            // 并发数目 -c N
            case 'c': clients=atoi(optarg);break;
        }
    }
    /*
     * optind返回第一个不包含选项的命令名参数，此处为URL值 
     */
    if(optind==argc) {
        fprintf(stderr,"webbench: Missing URL!\n");
        usage();
        return 2;
    }

    // 判断clients的数量
    if(clients==0) clients=1;
    // 压力测试默认测试时间 30s
    if(benchtime==0) benchtime=30;
 
    /* Copyright */
    fprintf(stderr,"Webbench - Simple Web Benchmark "PROGRAM_VERSION"\n"
            "Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.\n"
            );
 
    // 调用 build_request()构建完整的http的请求头，HTTP request存储在全局变量char request[REQUEST_SIZE]
    build_request(argv[optind]);
 
    // print request info ,do it in function build_request
    /*printf("Benchmarking: ");
 
    switch(method)
    {
        case METHOD_GET:
        default:
        printf("GET");break;
        case METHOD_OPTIONS:
        printf("OPTIONS");break;
        case METHOD_HEAD:
        printf("HEAD");break;
        case METHOD_TRACE:
        printf("TRACE");break;
    }
    
    printf(" %s",argv[optind]);
    
    switch(http10)
    {
        case 0: printf(" (using HTTP/0.9)");break;
        case 2: printf(" (using HTTP/1.1)");break;
    }
 
    printf("\n");
    */

    printf("Runing info: ");

    if(clients==1) 
        printf("1 client");
    else
        printf("%d clients",clients);

    printf(", running %d sec", benchtime);
    
    if(force) printf(", early socket close");
    if(proxyhost!=NULL) printf(", via proxy server %s:%d",proxyhost,proxyport);
    if(force_reload) printf(", forcing reload");
    
    printf(".\n");
    
    // 开始进行测压
    return bench();
}

/**
 * 用于构建 url 的请求头部
 * 
 * 此函数主要目的是要把类似于http GET请求的信息全部存储到全局变量request[REQUEST_SIZE]
 * 中，其中换行操作使用"\r\n"。其中应用了大量的字符串操作函数。
 * 创建url请求连接，HTTP头，创建好的请求放在全局变量re
*/
void build_request(const char *url)
{
    char tmp[10];
    int i;

    //bzero(host,MAXHOSTNAMELEN);
    //bzero(request,REQUEST_SIZE);
    memset(host,0,MAXHOSTNAMELEN);
    memset(request,0,REQUEST_SIZE);

    // 协议选择
    if(force_reload && proxyhost!=NULL && http10<1) http10=1;
    if(method==METHOD_HEAD && http10<1) http10=1;
    if(method==METHOD_OPTIONS && http10<2) http10=2;
    if(method==METHOD_TRACE && http10<2) http10=2;

    // 方法选择
    switch(method)
    {
        default:
        case METHOD_GET: strcpy(request,"GET");break;
        case METHOD_HEAD: strcpy(request,"HEAD");break;
        case METHOD_OPTIONS: strcpy(request,"OPTIONS");break;
        case METHOD_TRACE: strcpy(request,"TRACE");break;
    }

    // 追加空格
    strcat(request," ");

    if(NULL==strstr(url,"://"))
    {
        fprintf(stderr, "\n%s: is not a valid URL.\n",url);
        exit(2);
    }
    if(strlen(url)>1500)
    {
        fprintf(stderr,"URL is too long.\n");
        exit(2);
    }
    if (0!=strncasecmp("http://",url,7)) 
    { 
        fprintf(stderr,"\nOnly HTTP protocol is directly supported, set --proxy for others.\n");
        exit(2);
    }
    
    /* protocol/host delimiter */
    i=strstr(url,"://")-url+3;

    if(strchr(url+i,'/')==NULL) {
        fprintf(stderr,"\nInvalid URL syntax - hostname don't ends with '/'.\n");
        exit(2);
    }
    
    if(proxyhost==NULL)
    {
        /* get port from hostname */
        if(index(url+i,':')!=NULL && index(url+i,':')<index(url+i,'/'))
        {
            strncpy(host,url+i,strchr(url+i,':')-url-i);
            //bzero(tmp,10);
            memset(tmp,0,10);
            strncpy(tmp,index(url+i,':')+1,strchr(url+i,'/')-index(url+i,':')-1);
            /* printf("tmp=%s\n",tmp); */
            proxyport=atoi(tmp);
            if(proxyport==0) proxyport=80;
        } 
        else
        {
            strncpy(host,url+i,strcspn(url+i,"/"));
        }
        // printf("Host=%s\n",host);
        strcat(request+strlen(request),url+i+strcspn(url+i,"/"));
    } 
    else
    {
        // printf("ProxyHost=%s\nProxyPort=%d\n",proxyhost,proxyport);
        strcat(request,url);
    }

    if(http10==1)
        strcat(request," HTTP/1.0");
    else if (http10==2)
        strcat(request," HTTP/1.1");
  
    strcat(request,"\r\n");
  
    if(http10>0)
        strcat(request,"User-Agent: WebBench "PROGRAM_VERSION"\r\n");
    if(proxyhost==NULL && http10>0)
    {
        strcat(request,"Host: ");
        strcat(request,host);
        strcat(request,"\r\n");
    }
 
    if(force_reload && proxyhost!=NULL)
    {
        strcat(request,"Pragma: no-cache\r\n");
    }
  
    if(http10>1)
        strcat(request,"Connection: close\r\n");
    
    /* add empty line at end */
    if(http10>0) strcat(request,"\r\n"); 
    
    printf("\nRequest:\n%s\n",request);
}

/* vraci system rc error kod */
/**
 * 先进行 socket 连接，若成功，则创建管道，用于子进程向父进程传输数据
 * 子进程是主进程通过fork调用复制出来，每个子进程都会调用 benchcore 进行测试，
 * 并将结果输出到管道
 * 父进程通过读取管道收集子进程的测压数据，并汇总显现结果。
*/
static int bench(void)
{
    int i,j,k;	
    pid_t pid=0;
    FILE *f;

    /* check avaibility of target server */
    // 检查目标服务器数量
    i=Socket(proxyhost==NULL?host:proxyhost,proxyport);
    if(i<0) { 
        fprintf(stderr,"\nConnect to server failed. Aborting benchmark.\n");
        return 1;
    }
    close(i);
    
    /* create pipe */
    // 创建管道
    if(pipe(mypipe))
    {
        perror("pipe failed.");
        return 3;
    }

    /* not needed, since we have alarm() in childrens */
    /* wait 4 next system clock tick */
    /*
    cas=time(NULL);
    while(time(NULL)==cas)
    sched_yield();
    */

    /* fork childs */
    // 创建指定数量的子进程
    for(i=0;i<clients;i++)
    {
        /** 
         * fork()
         * 成功： 在父进程中返回子进程 PID
         *        在子进程中返回 0
         * 失败： 返回< 0 的数
         */
        pid=fork(); 

        // 这里将 返回成功(在进程中)与 失败一起处理了(不推荐)[因为你不知道什么时候进程结束]
        if(pid <= (pid_t) 0)
        {
            /* child process or error*/
            sleep(1); /* make childs faster */
            break; //若是子进程或失败，则跳出循环
        }
    }

    // 创建子进程失败
    if( pid < (pid_t) 0)
    {
        fprintf(stderr,"problems forking worker no. %d\n",i);
        perror("fork failed.");
        return 3;
    }

    // 子进程：在子进程中调用 benchcore 并将结果写入到管道
    if(pid == (pid_t) 0)
    {
        /* I am a child */
        // 在子进程中发送数据，但发送的是全局变量？？
        if(proxyhost==NULL)
            benchcore(host,proxyport,request);
        else
            benchcore(proxyhost,proxyport,request);

        /* write results to pipe */
        // 管道是 单向通信， 0 读 1写， 这里写入管道，但是 mypipe[0]端的写并没有关闭
        f=fdopen(mypipe[1],"w");
        if(f==NULL)
        {
            perror("open pipe for writing failed.");
            return 3;
        }
        /* fprintf(stderr,"Child - %d %d\n",speed,failed); */
        fprintf(f,"%d %d %d\n",speed,failed,bytes);
        fclose(f);

        return 0;
    } 
    // 父进程读取管道中的数据
    else
    {
        // 管道是 单向通信， 0 读 1写， 这里读取管道，但是 mypipe1]端的读并没有关闭
        f=fdopen(mypipe[0],"r");
        if(f==NULL) 
        {
            perror("open pipe for reading failed.");
            return 3;
        }
        
        // _IONBF : 直接从流中读入数据或直接向流中写入数据，而没有缓冲区
        // 设置无缓冲
        setvbuf(f,NULL,_IONBF,0);
        
        // 重置变量，防止使用被污染的数据
        speed=0;
        failed=0;
        bytes=0;
    
        // 从管道中读取数据， fscanf() 是阻塞函数
        while(1)
        {
            pid=fscanf(f,"%d %d %d",&i,&j,&k);
            if(pid<2)
            {
                fprintf(stderr,"Some of our childrens died.\n");
                break;
            }
            
            // 父进程利用管道统计子进程中三种数据的和
            speed+=i;
            failed+=j;
            bytes+=k;
        
            /* fprintf(stderr,"*Knock* %d %d read=%d\n",speed,failed,pid); */
            // 记录已读取子进程数量，读完(=0)则退出
            if(--clients==0) break;
        }
    
        fclose(f);

        // 输出
        printf("\nSpeed=%d pages/min, %d bytes/sec.\nRequests: %d susceed, %d failed.\n",
            (int)((speed+failed)/(benchtime/60.0f)),
            (int)(bytes/(float)benchtime),
            speed,
            failed);
    }
    
    return i;
}

/**
 * 测试函数(被子进程调用)
 *  通过 SIGALRM 信号来控制时间， 通过 alarm 设置在指定时间后触发 SIGALRM 信号，从而执行 alrm_handler()
 * 
 * host 地址
 * port 端口
 * req ： http 格式方法
*/
void benchcore(const char *host,const int port,const char *req)
{
    int rlen;

    // 记录服务器请求的返回结果
    char buf[1500];
    int s,i;
    struct sigaction sa;

    /* setup alarm signal handler */
    // 信号处理函数(SIGALRM 信号的回调函数)
    sa.sa_handler=alarm_handler;
    sa.sa_flags=0;
    /**
     *  信号函数 sigacation() 
     *  成功 返回 0，失败返回 1， 超时产生 SIGALRM 信号，采用sa指定的函数进行处理
     */
    if(sigaction(SIGALRM,&sa,NULL))
        exit(3);
    
    // 设置定时器(开始计时)，超过 benchtime就产生 SIGALRM 信号
    alarm(benchtime); // after benchtime,then exit

    rlen=strlen(req);

    // 无限次请求，直到收到 SIGALRM 信号
    nexttry:while(1)
    {
        // 超时则返回
        if(timerexpired)
        {
            if(failed>0)
            {
                /* fprintf(stderr,"Correcting failed by signal\n"); */
                failed--;
            }
            return;
        }
        
        // 与远程服务器建立连接（通过socket建立TCP连接）
        s=Socket(host,port);                          
        // 失败
        if(s<0) { failed++;continue;} 
        // 发出请求：header大小与发送大小不等，则失败
        if(rlen!=write(s,req,rlen)) {failed++;close(s);continue;}
        if(http10==0) 
        if(shutdown(s,1)) { failed++;close(s);continue;}

        // 等待服务返回结果
        if(force==0) 
        {
            /* read all available data from socket */
            // 读取socket中读取数据
            while(1)
            {
                // timerexpired 默认为 0， 在指定时间内部读取，为1时，则表示定时结束
                if(timerexpired) break; 

                // 从 buf 中读取数
                i=read(s,buf,1500);
                /* fprintf(stderr,"%d\n",i); */
                if(i<0) 
                { 
                    failed++;
                    close(s);
                    goto nexttry;
                }
                else
                if(i==0) break;
                else
                bytes+=i;
            }
        }

        // 关闭连接
        if(close(s)) {failed++;continue;}
        speed++;
    }
}
