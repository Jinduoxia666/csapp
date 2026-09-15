/* 教材代码片段开始：$begin tinymain */
/* tiny.c —— 简单的顺序 HTTP/1.0 Web 服务器，
 * 使用 GET 方法提供静态和动态内容。
 * 
 * 2019 年 11 月，droh 更新：
 * 修复 serve_static() 和 clienterror() 中 sprintf() 的源目标别名问题。 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

int main(int argc, char **argv) 
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* 检查命令行参数 */
    if (argc != 2) {
	fprintf(stderr, "usage: %s <port>\n", argv[0]);
	exit(1);
    }

    listenfd = Open_listenfd(argv[1]);
    while (1) {
	clientlen = sizeof(clientaddr);
	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 教材行号标记：line:netp:tiny:accept
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                    port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd);                                             // 教材行号标记：line:netp:tiny:doit
	Close(connfd);                                            // 教材行号标记：line:netp:tiny:close
    }
}
/* 教材代码片段结束：$end tinymain */

/* doit —— 处理一次 HTTP 请求与响应 */
/* 教材代码片段开始：$begin doit */
void doit(int fd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* 读取请求行和请求头 */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  // 教材行号标记：line:netp:doit:readrequest
        return;
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);       // 教材行号标记：line:netp:doit:parserequest
    if (strcasecmp(method, "GET")) {                     // 教材行号标记：line:netp:doit:beginrequesterr
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        return;
    }                                                    // 教材行号标记：line:netp:doit:endrequesterr
    read_requesthdrs(&rio);                              // 教材行号标记：line:netp:doit:readrequesthdrs

    /* 解析 GET 请求中的 URI */
    is_static = parse_uri(uri, filename, cgiargs);       // 教材行号标记：line:netp:doit:staticcheck
    if (stat(filename, &sbuf) < 0) {                     // 教材行号标记：line:netp:doit:beginnotfound
	clienterror(fd, filename, "404", "Not found",
		    "Tiny couldn't find this file");
	return;
    }                                                    // 教材行号标记：line:netp:doit:endnotfound

    if (is_static) { /* 提供静态内容 */          
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 教材行号标记：line:netp:doit:readable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't read the file");
	    return;
	}
	serve_static(fd, filename, sbuf.st_size);        // 教材行号标记：line:netp:doit:servestatic
    }
    else { /* 提供动态内容 */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 教材行号标记：line:netp:doit:executable
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
	serve_dynamic(fd, filename, cgiargs);            // 教材行号标记：line:netp:doit:servedynamic
    }
}
/* 教材代码片段结束：$end doit */

/* read_requesthdrs —— 读取 HTTP 请求头 */
/* 教材代码片段开始：$begin read_requesthdrs */
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {          // 教材行号标记：line:netp:readhdrs:checkterm
	Rio_readlineb(rp, buf, MAXLINE);
	printf("%s", buf);
    }
    return;
}
/* 教材代码片段结束：$end read_requesthdrs */

/* parse_uri —— 将 URI 解析为文件名和 CGI 参数；
 * 动态内容返回 0，静态内容返回 1。 */
/* 教材代码片段开始：$begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* 静态内容 */ // 教材行号标记：line:netp:parseuri:isstatic
	strcpy(cgiargs, "");                             // 教材行号标记：line:netp:parseuri:clearcgi
	strcpy(filename, ".");                           // 教材行号标记：line:netp:parseuri:beginconvert1
	strcat(filename, uri);                           // 教材行号标记：line:netp:parseuri:endconvert1
	if (uri[strlen(uri)-1] == '/')                   // 教材行号标记：line:netp:parseuri:slashcheck
	    strcat(filename, "home.html");               // 教材行号标记：line:netp:parseuri:appenddefault
	return 1;
    }
    else {  /* 动态内容 */                        // 教材行号标记：line:netp:parseuri:isdynamic
	ptr = index(uri, '?');                           // 教材行号标记：line:netp:parseuri:beginextract
	if (ptr) {
	    strcpy(cgiargs, ptr+1);
	    *ptr = '\0';
	}
	else 
	    strcpy(cgiargs, "");                         // 教材行号标记：line:netp:parseuri:endextract
	strcpy(filename, ".");                           // 教材行号标记：line:netp:parseuri:beginconvert2
	strcat(filename, uri);                           // 教材行号标记：line:netp:parseuri:endconvert2
	return 0;
    }
}
/* 教材代码片段结束：$end parse_uri */

/* serve_static —— 将文件内容发送给客户端 */
/* 教材代码片段开始：$begin serve_static */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* 向客户端发送响应头 */
    get_filetype(filename, filetype);    // 教材行号标记：line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); // 教材行号标记：line:netp:servestatic:beginserve
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n", filesize);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));    // 教材行号标记：line:netp:servestatic:endserve

    /* 向客户端发送响应体 */
    srcfd = Open(filename, O_RDONLY, 0); // 教材行号标记：line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 教材行号标记：line:netp:servestatic:mmap
    Close(srcfd);                       // 教材行号标记：line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);     // 教材行号标记：line:netp:servestatic:write
    Munmap(srcp, filesize);             // 教材行号标记：line:netp:servestatic:munmap
}

/* get_filetype —— 根据文件名确定文件类型 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	strcpy(filetype, "image/jpeg");
    else
	strcpy(filetype, "text/plain");
}  
/* 教材代码片段结束：$end serve_static */

/* serve_dynamic —— 根据客户端请求运行 CGI 程序 */
/* 教材代码片段开始：$begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* 返回 HTTP 响应的第一部分 */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* 子进程 */ // 教材行号标记：line:netp:servedynamic:fork
	/* 完整的服务器应在这里设置所有 CGI 环境变量 */
	setenv("QUERY_STRING", cgiargs, 1); // 教材行号标记：line:netp:servedynamic:setenv
	Dup2(fd, STDOUT_FILENO);         /* 将标准输出重定向到客户端连接 */ // 教材行号标记：line:netp:servedynamic:dup2
	Execve(filename, emptylist, environ); /* 运行 CGI 程序 */ // 教材行号标记：line:netp:servedynamic:execve
    }
    Wait(NULL); /* 父进程等待并回收子进程 */ // 教材行号标记：line:netp:servedynamic:wait
}
/* 教材代码片段结束：$end serve_dynamic */

/* clienterror —— 向客户端返回错误信息 */
/* 教材代码片段开始：$begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* 输出 HTTP 响应头 */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* 输出 HTTP 响应体 */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
/* 教材代码片段结束：$end clienterror */
