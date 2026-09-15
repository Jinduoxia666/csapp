/* csapp.c —— CS:APP 第三版使用的函数
 * 
 * 2016 年 10 月，reb 更新：
 * 修复 sio_ltoa 未处理负数的问题。
 * 
 * 2016 年 2 月，droh 更新：
 * 改进 open_clientfd 和 open_listenfd 的失败处理。
 * 
 * 2014 年 8 月，droh 更新：
 * 新版 open_clientfd 和 open_listenfd 可重入，且与协议版本无关。
 * 增加与协议版本无关的 inet_ntop 和 inet_pton；inet_ntoa 和 inet_aton 已过时。
 * 
 * 2014 年 7 月，droh 更新：
 * 增加可重入的信号安全 I/O（SIO）函数。
 * 
 * 2013 年 4 月，droh 更新：
 * 修复 rio_readlineb 的边界情况错误；移除 rio_readnb 中多余的 EINTR 检查。 */
/* 教材代码片段开始：$begin csapp.c */
#include "csapp.h"

/* 错误处理函数 */
/* 教材代码片段开始：$begin errorfuns */
/* 教材代码片段开始：$begin unixerror */
void unix_error(char *msg) /* Unix 风格的错误 */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}
/* 教材代码片段结束：$end unixerror */

void posix_error(int code, char *msg) /* POSIX 风格的错误 */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(code));
    exit(0);
}

void gai_error(int code, char *msg) /* getaddrinfo 风格的错误 */
{
    fprintf(stderr, "%s: %s\n", msg, gai_strerror(code));
    exit(0);
}

void app_error(char *msg) /* 应用程序错误 */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}
/* 教材代码片段结束：$end errorfuns */

void dns_error(char *msg) /* 已过时的 gethostbyname 错误处理 */
{
    fprintf(stderr, "%s\n", msg);
    exit(0);
}


/* Unix 进程控制函数的包装 */

/* 教材代码片段开始：$begin forkwrapper */
pid_t Fork(void) 
{
    pid_t pid;

    if ((pid = fork()) < 0)
	unix_error("Fork error");
    return pid;
}
/* 教材代码片段结束：$end forkwrapper */

void Execve(const char *filename, char *const argv[], char *const envp[]) 
{
    if (execve(filename, argv, envp) < 0)
	unix_error("Execve error");
}

/* 教材代码片段开始：$begin wait */
pid_t Wait(int *status) 
{
    pid_t pid;

    if ((pid  = wait(status)) < 0)
	unix_error("Wait error");
    return pid;
}
/* 教材代码片段结束：$end wait */

pid_t Waitpid(pid_t pid, int *iptr, int options) 
{
    pid_t retpid;

    if ((retpid  = waitpid(pid, iptr, options)) < 0) 
	unix_error("Waitpid error");
    return(retpid);
}

/* 教材代码片段开始：$begin kill */
void Kill(pid_t pid, int signum) 
{
    int rc;

    if ((rc = kill(pid, signum)) < 0)
	unix_error("Kill error");
}
/* 教材代码片段结束：$end kill */

void Pause() 
{
    (void)pause();
    return;
}

unsigned int Sleep(unsigned int secs) 
{
    unsigned int rc;

    if ((rc = sleep(secs)) < 0)
	unix_error("Sleep error");
    return rc;
}

unsigned int Alarm(unsigned int seconds) {
    return alarm(seconds);
}
 
void Setpgid(pid_t pid, pid_t pgid) {
    int rc;

    if ((rc = setpgid(pid, pgid)) < 0)
	unix_error("Setpgid error");
    return;
}

pid_t Getpgrp(void) {
    return getpgrp();
}

/* Unix 信号函数的包装 */

/* 教材代码片段开始：$begin sigaction */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* 阻塞正在处理的同类型信号 */
    action.sa_flags = SA_RESTART; /* 尽可能重新启动被中断的系统调用 */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}
/* 教材代码片段结束：$end sigaction */

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    if (sigprocmask(how, set, oldset) < 0)
	unix_error("Sigprocmask error");
    return;
}

void Sigemptyset(sigset_t *set)
{
    if (sigemptyset(set) < 0)
	unix_error("Sigemptyset error");
    return;
}

void Sigfillset(sigset_t *set)
{ 
    if (sigfillset(set) < 0)
	unix_error("Sigfillset error");
    return;
}

void Sigaddset(sigset_t *set, int signum)
{
    if (sigaddset(set, signum) < 0)
	unix_error("Sigaddset error");
    return;
}

void Sigdelset(sigset_t *set, int signum)
{
    if (sigdelset(set, signum) < 0)
	unix_error("Sigdelset error");
    return;
}

int Sigismember(const sigset_t *set, int signum)
{
    int rc;
    if ((rc = sigismember(set, signum)) < 0)
	unix_error("Sigismember error");
    return rc;
}

int Sigsuspend(const sigset_t *set)
{
    int rc = sigsuspend(set); /* 始终返回 -1 */
    if (errno != EINTR)
        unix_error("Sigsuspend error");
    return rc;
}

/* 信号安全 I/O（SIO）包：可在信号处理函数中安全使用的简单可重入输出函数 */

/* SIO 内部函数 */

/* 教材代码片段开始：$begin sioprivate */
/* sio_reverse —— 反转字符串，源自 K&R */
static void sio_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

/* sio_ltoa —— 将 long 转换为以 b 为基数的字符串，源自 K&R */
static void sio_ltoa(long v, char s[], int b) 
{
    int c, i = 0;
    int neg = v < 0;

    if (neg)
	v = -v;

    do {  
        s[i++] = ((c = (v % b)) < 10)  ?  c + '0' : c - 10 + 'a';
    } while ((v /= b) > 0);

    if (neg)
	s[i++] = '-';

    s[i] = '\0';
    sio_reverse(s);
}

/* sio_strlen —— 返回字符串长度，源自 K&R */
static size_t sio_strlen(char s[])
{
    int i = 0;

    while (s[i] != '\0')
        ++i;
    return i;
}
/* 教材代码片段结束：$end sioprivate */

/* SIO 对外提供的函数 */
/* 教材代码片段开始：$begin siopublic */

ssize_t sio_puts(char s[]) /* 输出字符串 */
{
    return write(STDOUT_FILENO, s, sio_strlen(s)); // 教材行号标记：line:csapp:siostrlen
}

ssize_t sio_putl(long v) /* 输出 long 整数 */
{
    char s[128];
    
    sio_ltoa(v, s, 10); /* 基于 K&R 的 itoa() 实现 */  // 教材行号标记：line:csapp:sioltoa
    return sio_puts(s);
}

void sio_error(char s[]) /* 输出错误信息并退出 */
{
    sio_puts(s);
    _exit(1);                                      // 教材行号标记：line:csapp:sioexit
}
/* 教材代码片段结束：$end siopublic */

/* SIO 函数的包装 */
ssize_t Sio_putl(long v)
{
    ssize_t n;
  
    if ((n = sio_putl(v)) < 0)
	sio_error("Sio_putl error");
    return n;
}

ssize_t Sio_puts(char s[])
{
    ssize_t n;
  
    if ((n = sio_puts(s)) < 0)
	sio_error("Sio_puts error");
    return n;
}

void Sio_error(char s[])
{
    sio_error(s);
}

/* Unix I/O 函数的包装 */

int Open(const char *pathname, int flags, mode_t mode) 
{
    int rc;

    if ((rc = open(pathname, flags, mode))  < 0)
	unix_error("Open error");
    return rc;
}

ssize_t Read(int fd, void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = read(fd, buf, count)) < 0) 
	unix_error("Read error");
    return rc;
}

ssize_t Write(int fd, const void *buf, size_t count) 
{
    ssize_t rc;

    if ((rc = write(fd, buf, count)) < 0)
	unix_error("Write error");
    return rc;
}

off_t Lseek(int fildes, off_t offset, int whence) 
{
    off_t rc;

    if ((rc = lseek(fildes, offset, whence)) < 0)
	unix_error("Lseek error");
    return rc;
}

void Close(int fd) 
{
    int rc;

    if ((rc = close(fd)) < 0)
	unix_error("Close error");
}

int Select(int  n, fd_set *readfds, fd_set *writefds,
	   fd_set *exceptfds, struct timeval *timeout) 
{
    int rc;

    if ((rc = select(n, readfds, writefds, exceptfds, timeout)) < 0)
	unix_error("Select error");
    return rc;
}

int Dup2(int fd1, int fd2) 
{
    int rc;

    if ((rc = dup2(fd1, fd2)) < 0)
	unix_error("Dup2 error");
    return rc;
}

void Stat(const char *filename, struct stat *buf) 
{
    if (stat(filename, buf) < 0)
	unix_error("Stat error");
}

void Fstat(int fd, struct stat *buf) 
{
    if (fstat(fd, buf) < 0)
	unix_error("Fstat error");
}

/* 目录操作函数的包装 */

DIR *Opendir(const char *name) 
{
    DIR *dirp = opendir(name); 

    if (!dirp)
        unix_error("opendir error");
    return dirp;
}

struct dirent *Readdir(DIR *dirp)
{
    struct dirent *dep;
    
    errno = 0;
    dep = readdir(dirp);
    if ((dep == NULL) && (errno != 0))
        unix_error("readdir error");
    return dep;
}

int Closedir(DIR *dirp) 
{
    int rc;

    if ((rc = closedir(dirp)) < 0)
        unix_error("closedir error");
    return rc;
}

/* 内存映射函数的包装 */
void *Mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) 
{
    void *ptr;

    if ((ptr = mmap(addr, len, prot, flags, fd, offset)) == ((void *) -1))
	unix_error("mmap error");
    return(ptr);
}

void Munmap(void *start, size_t length) 
{
    if (munmap(start, length) < 0)
	unix_error("munmap error");
}

/* 动态内存分配函数的包装 */

void *Malloc(size_t size) 
{
    void *p;

    if ((p  = malloc(size)) == NULL)
	unix_error("Malloc error");
    return p;
}

void *Realloc(void *ptr, size_t size) 
{
    void *p;

    if ((p  = realloc(ptr, size)) == NULL)
	unix_error("Realloc error");
    return p;
}

void *Calloc(size_t nmemb, size_t size) 
{
    void *p;

    if ((p = calloc(nmemb, size)) == NULL)
	unix_error("Calloc error");
    return p;
}

void Free(void *ptr) 
{
    free(ptr);
}

/* 标准 I/O 函数的包装 */
void Fclose(FILE *fp) 
{
    if (fclose(fp) != 0)
	unix_error("Fclose error");
}

FILE *Fdopen(int fd, const char *type) 
{
    FILE *fp;

    if ((fp = fdopen(fd, type)) == NULL)
	unix_error("Fdopen error");

    return fp;
}

char *Fgets(char *ptr, int n, FILE *stream) 
{
    char *rptr;

    if (((rptr = fgets(ptr, n, stream)) == NULL) && ferror(stream))
	app_error("Fgets error");

    return rptr;
}

FILE *Fopen(const char *filename, const char *mode) 
{
    FILE *fp;

    if ((fp = fopen(filename, mode)) == NULL)
	unix_error("Fopen error");

    return fp;
}

void Fputs(const char *ptr, FILE *stream) 
{
    if (fputs(ptr, stream) == EOF)
	unix_error("Fputs error");
}

size_t Fread(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    size_t n;

    if (((n = fread(ptr, size, nmemb, stream)) < nmemb) && ferror(stream)) 
	unix_error("Fread error");
    return n;
}

void Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    if (fwrite(ptr, size, nmemb, stream) < nmemb)
	unix_error("Fwrite error");
}


/* socket 接口包装函数 */

int Socket(int domain, int type, int protocol) 
{
    int rc;

    if ((rc = socket(domain, type, protocol)) < 0)
	unix_error("Socket error");
    return rc;
}

void Setsockopt(int s, int level, int optname, const void *optval, int optlen) 
{
    int rc;

    if ((rc = setsockopt(s, level, optname, optval, optlen)) < 0)
	unix_error("Setsockopt error");
}

void Bind(int sockfd, struct sockaddr *my_addr, int addrlen) 
{
    int rc;

    if ((rc = bind(sockfd, my_addr, addrlen)) < 0)
	unix_error("Bind error");
}

void Listen(int s, int backlog) 
{
    int rc;

    if ((rc = listen(s,  backlog)) < 0)
	unix_error("Listen error");
}

int Accept(int s, struct sockaddr *addr, socklen_t *addrlen) 
{
    int rc;

    if ((rc = accept(s, addr, addrlen)) < 0)
	unix_error("Accept error");
    return rc;
}

void Connect(int sockfd, struct sockaddr *serv_addr, int addrlen) 
{
    int rc;

    if ((rc = connect(sockfd, serv_addr, addrlen)) < 0)
	unix_error("Connect error");
}

/* 与协议版本无关的包装函数 */
/* 教材代码片段开始：$begin getaddrinfo */
void Getaddrinfo(const char *node, const char *service, 
                 const struct addrinfo *hints, struct addrinfo **res)
{
    int rc;

    if ((rc = getaddrinfo(node, service, hints, res)) != 0) 
        gai_error(rc, "Getaddrinfo error");
}
/* 教材代码片段结束：$end getaddrinfo */

void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, 
                 size_t hostlen, char *serv, size_t servlen, int flags)
{
    int rc;

    if ((rc = getnameinfo(sa, salen, host, hostlen, serv, 
                          servlen, flags)) != 0) 
        gai_error(rc, "Getnameinfo error");
}

void Freeaddrinfo(struct addrinfo *res)
{
    freeaddrinfo(res);
}

void Inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    if (!inet_ntop(af, src, dst, size))
        unix_error("Inet_ntop error");
}

void Inet_pton(int af, const char *src, void *dst) 
{
    int rc;

    rc = inet_pton(af, src, dst);
    if (rc == 0)
	app_error("inet_pton error: invalid dotted-decimal address");
    else if (rc < 0)
        unix_error("Inet_pton error");
}

/* DNS 接口包装函数。
 * 注意：这些接口不是线程安全的，已经过时，请改用 getaddrinfo 和 getnameinfo。 */

/* 教材代码片段开始：$begin gethostbyname */
struct hostent *Gethostbyname(const char *name) 
{
    struct hostent *p;

    if ((p = gethostbyname(name)) == NULL)
	dns_error("Gethostbyname error");
    return p;
}
/* 教材代码片段结束：$end gethostbyname */

struct hostent *Gethostbyaddr(const char *addr, int len, int type) 
{
    struct hostent *p;

    if ((p = gethostbyaddr(addr, len, type)) == NULL)
	dns_error("Gethostbyaddr error");
    return p;
}

/* Pthreads 线程控制函数的包装 */

void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp) 
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0)
	posix_error(rc, "Pthread_create error");
}

void Pthread_cancel(pthread_t tid) {
    int rc;

    if ((rc = pthread_cancel(tid)) != 0)
	posix_error(rc, "Pthread_cancel error");
}

void Pthread_join(pthread_t tid, void **thread_return) {
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0)
	posix_error(rc, "Pthread_join error");
}

/* 教材代码片段开始：$begin detach */
void Pthread_detach(pthread_t tid) {
    int rc;

    if ((rc = pthread_detach(tid)) != 0)
	posix_error(rc, "Pthread_detach error");
}
/* 教材代码片段结束：$end detach */

void Pthread_exit(void *retval) {
    pthread_exit(retval);
}

pthread_t Pthread_self(void) {
    return pthread_self();
}
 
void Pthread_once(pthread_once_t *once_control, void (*init_function)()) {
    pthread_once(once_control, init_function);
}

/* POSIX 信号量的包装 */

void Sem_init(sem_t *sem, int pshared, unsigned int value) 
{
    if (sem_init(sem, pshared, value) < 0)
	unix_error("Sem_init error");
}

void P(sem_t *sem) 
{
    if (sem_wait(sem) < 0)
	unix_error("P error");
}

void V(sem_t *sem) 
{
    if (sem_post(sem) < 0)
	unix_error("V error");
}

/* RIO 包：健壮 I/O 函数 */

/* rio_readn —— 以健壮方式读取 n 字节，不带缓冲 */
/* 教材代码片段开始：$begin rio_readn */
ssize_t rio_readn(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nread = read(fd, bufp, nleft)) < 0) {
	    if (errno == EINTR) /* 被信号处理过程打断 */
		nread = 0;      /* 随后重新调用 read() */
	    else
		return -1;      /* errno 由 read() 设置 */ 
	} 
	else if (nread == 0)
	    break;              /* 文件结束（EOF） */
	nleft -= nread;
	bufp += nread;
    }
    return (n - nleft);         /* 返回值不小于 0 */
}
/* 教材代码片段结束：$end rio_readn */

/* rio_writen —— 以健壮方式写入 n 字节，不带缓冲 */
/* 教材代码片段开始：$begin rio_writen */
ssize_t rio_writen(int fd, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
	if ((nwritten = write(fd, bufp, nleft)) <= 0) {
	    if (errno == EINTR)  /* 被信号处理过程打断 */
		nwritten = 0;    /* 随后重新调用 write() */
	    else
		return -1;       /* errno 由 write() 设置 */
	}
	nleft -= nwritten;
	bufp += nwritten;
    }
    return n;
}
/* 教材代码片段结束：$end rio_writen */


/* rio_read —— Unix read() 的包装函数。
 * 从内部缓冲区向用户缓冲区复制 min(n, rio_cnt) 字节；
 * n 是用户请求的字节数，rio_cnt 是内部缓冲区中未读取的字节数。
 * 进入函数时若内部缓冲区为空，则调用 read() 补充数据。 */
/* 教材代码片段开始：$begin rio_read */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) {  /* 缓冲区为空时补充数据 */
	rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, 
			   sizeof(rp->rio_buf));
	if (rp->rio_cnt < 0) {
	    if (errno != EINTR) /* 被信号处理过程打断 */
		return -1;
	}
	else if (rp->rio_cnt == 0)  /* 文件结束（EOF） */
	    return 0;
	else 
	    rp->rio_bufptr = rp->rio_buf; /* 重置缓冲区指针 */
    }

    /* 从内部缓冲区向用户缓冲区复制 min(n, rp->rio_cnt) 字节 */
    cnt = n;          
    if (rp->rio_cnt < n)   
	cnt = rp->rio_cnt;
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;
    return cnt;
}
/* 教材代码片段结束：$end rio_read */

/* rio_readinitb —— 将描述符与读取缓冲区关联，并重置缓冲状态 */
/* 教材代码片段开始：$begin rio_readinitb */
void rio_readinitb(rio_t *rp, int fd) 
{
    rp->rio_fd = fd;  
    rp->rio_cnt = 0;  
    rp->rio_bufptr = rp->rio_buf;
}
/* 教材代码片段结束：$end rio_readinitb */

/* rio_readnb —— 以健壮方式读取 n 字节，带缓冲 */
/* 教材代码片段开始：$begin rio_readnb */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;
    
    while (nleft > 0) {
	if ((nread = rio_read(rp, bufp, nleft)) < 0) 
            return -1;          /* errno 由 read() 设置 */ 
	else if (nread == 0)
	    break;              /* 文件结束（EOF） */
	nleft -= nread;
	bufp += nread;
    }
    return (n - nleft);         /* 返回值不小于 0 */
}
/* 教材代码片段结束：$end rio_readnb */

/* rio_readlineb —— 以健壮方式读取一行文本，带缓冲 */
/* 教材代码片段开始：$begin rio_readlineb */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) { 
        if ((rc = rio_read(rp, &c, 1)) == 1) {
	    *bufp++ = c;
	    if (c == '\n') {
                n++;
     		break;
            }
	} else if (rc == 0) {
	    if (n == 1)
		return 0; /* 遇到 EOF，尚未读到数据 */
	    else
		break;    /* 遇到 EOF，已经读到部分数据 */
	} else
	    return -1;	  /* 发生错误 */
    }
    *bufp = 0;
    return n-1;
}
/* 教材代码片段结束：$end rio_readlineb */

/* 健壮 I/O 函数的包装 */
ssize_t Rio_readn(int fd, void *ptr, size_t nbytes) 
{
    ssize_t n;
  
    if ((n = rio_readn(fd, ptr, nbytes)) < 0)
	unix_error("Rio_readn error");
    return n;
}

void Rio_writen(int fd, void *usrbuf, size_t n) 
{
    if (rio_writen(fd, usrbuf, n) != n)
	unix_error("Rio_writen error");
}

void Rio_readinitb(rio_t *rp, int fd)
{
    rio_readinitb(rp, fd);
} 

ssize_t Rio_readnb(rio_t *rp, void *usrbuf, size_t n) 
{
    ssize_t rc;

    if ((rc = rio_readnb(rp, usrbuf, n)) < 0)
	unix_error("Rio_readnb error");
    return rc;
}

ssize_t Rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen) 
{
    ssize_t rc;

    if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
	unix_error("Rio_readlineb error");
    return rc;
} 

/* 客户端/服务器辅助函数 */
/* open_clientfd —— 连接 <hostname, port> 指定的服务器，
 * 返回可用于读写的 socket 描述符。该函数可重入，且与协议版本无关。
 * 错误返回值：getaddrinfo 出错时返回 -2；其他错误返回 -1，并设置 errno。 */
/* 教材代码片段开始：$begin open_clientfd */
int open_clientfd(char *hostname, char *port) {
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;

    /* 获取候选服务器地址列表 */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;  /* 建立流式连接 */
    hints.ai_flags = AI_NUMERICSERV;  /* 使用数字形式的端口参数 */
    hints.ai_flags |= AI_ADDRCONFIG;  /* 按本机配置筛选连接地址 */
    if ((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(rc));
        return -2;
    }
  
    /* 遍历地址列表，寻找能够成功连接的地址 */
    for (p = listp; p; p = p->ai_next) {
        /* 创建 socket 描述符 */
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
            continue; /* 创建 socket 失败，尝试下一个地址 */

        /* 连接服务器 */
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1) 
            break; /* 成功 */
        if (close(clientfd) < 0) { /* 连接失败，尝试其他地址 */  // 教材行号标记：line:netp:openclientfd:closefd
            fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
            return -1;
        } 
    } 

    /* 清理资源 */
    freeaddrinfo(listp);
    if (!p) /* 所有连接尝试均失败 */
        return -1;
    else    /* 最后一次连接尝试成功 */
        return clientfd;
}
/* 教材代码片段结束：$end open_clientfd */

/* open_listenfd —— 创建并返回监听指定端口的 socket。
 * 该函数可重入，且与协议版本无关。
 * 错误返回值：getaddrinfo 出错时返回 -2；其他错误返回 -1，并设置 errno。 */
/* 教材代码片段开始：$begin open_listenfd */
int open_listenfd(char *port) 
{
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval=1;

    /* 获取候选服务器地址列表 */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;             /* 接受流式连接 */
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG; /* 绑定通配 IP 地址 */
    hints.ai_flags |= AI_NUMERICSERV;            /* 使用数字端口号 */
    if ((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0) {
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    /* 遍历地址列表，寻找能够成功绑定的地址 */
    for (p = listp; p; p = p->ai_next) {
        /* 创建 socket 描述符 */
        if ((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) 
            continue;  /* 创建 socket 失败，尝试下一个地址 */

        /* 设置地址复用，减少重启时 bind 报地址已被使用的情况 */
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,    // 教材行号标记：line:netp:csapp:setsockopt
                   (const void *)&optval , sizeof(int));

        /* 将描述符绑定到地址 */
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
            break; /* 成功 */
        if (close(listenfd) < 0) { /* 绑定失败，尝试下一个地址 */
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }


    /* 清理资源 */
    freeaddrinfo(listp);
    if (!p) /* 没有可用的绑定地址 */
        return -1;

    /* 转为监听 socket，准备接受连接请求 */
    if (listen(listenfd, LISTENQ) < 0) {
        close(listenfd);
	return -1;
    }
    return listenfd;
}
/* 教材代码片段结束：$end open_listenfd */

/* 可重入且与协议版本无关的辅助函数的包装 */
int Open_clientfd(char *hostname, char *port) 
{
    int rc;

    if ((rc = open_clientfd(hostname, port)) < 0) 
	unix_error("Open_clientfd error");
    return rc;
}

int Open_listenfd(char *port) 
{
    int rc;

    if ((rc = open_listenfd(port)) < 0)
	unix_error("Open_listenfd error");
    return rc;
}

/* 教材代码片段结束：$end csapp.c */




