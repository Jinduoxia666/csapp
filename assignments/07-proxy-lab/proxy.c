#include "csapp.h"
#include <ctype.h>
#include "cache.h"

/* 代码中保留这条较长的语句不会被扣除风格分 */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

/* 对请求行和全部请求头设置上限，避免越界或无界分配。 */
#define HEADER_LIMIT 65536

static void close_fd(int fd)
{
    if (close(fd) < 0)
        perror("close");
}

/* 单次请求失败只结束该连接，不调用会退出进程的错误包装函数。 */
static void send_error(int fd, int status, const char *reason)
{
    char response[MAXLINE];
    int n = snprintf(response, sizeof(response),
                     "HTTP/1.0 %d %s\r\n"
                     "Content-Type: text/plain\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n\r\n%s\n",
                     status, reason, strlen(reason) + 1, reason);
    if (n > 0 && (size_t)n < sizeof(response) &&
        rio_writen(fd, response, (size_t)n) < 0)
        perror("write error response");
}

static int valid_port(const char *port)
{
    unsigned int value = 0;
    if (!*port)
        return 0;
    for (; *port; port++) {
        if (*port < '0' || *port > '9')
            return 0;
        value = value * 10 + (unsigned int)(*port - '0');
        if (value > 65535)
            return 0;
    }
    return value != 0;
}

/* 原地分离 URL 的主机和端口；保留路径及查询字符串。 */
static int parse_url(char *url, char *host, char *port,
                     char *path, char *authority)
{
    char *start, *end, *colon;
    size_t len;

    if (strncasecmp(url, "http://", 7) != 0)
        return -1;
    start = url + 7;
    end = start + strcspn(start, "/?#");
    len = (size_t)(end - start);
    if (!len || len >= MAXLINE || strchr(start, '#'))
        return -1;
    memcpy(authority, start, len);
    authority[len] = '\0';
    if (strchr(authority, '@'))
        return -1;

    if (*end == '?') {
        if (snprintf(path, MAXLINE, "/%s", end) >= MAXLINE)
            return -1;
    } else {
        snprintf(path, MAXLINE, "%s", *end ? end : "/");
    }
    *end = '\0';
    strcpy(port, "80");
    if (*start == '[') {
        /* 方括号包围 IPv6 地址，传给 getaddrinfo 时去掉括号。 */
        char *right = strchr(start, ']');
        struct in6_addr address;
        if (!right || (right[1] && right[1] != ':'))
            return -1;
        if (right[1] == ':')
            strcpy(port, right + 2);
        *right = '\0';
        start++;
        if (inet_pton(AF_INET6, start, &address) != 1)
            return -1;
    } else {
        colon = strchr(start, ':');
        if (colon) {
            *colon = '\0';
            strcpy(port, colon + 1);
        }
        for (const char *p = start; *p; p++)
            if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-')
                return -1;
    }
    if (!*start || !valid_port(port))
        return -1;
    strcpy(host, start);
    return 0;
}

/* 正常返回行长度；拒绝零字节、非法控制字符、不完整或过长的行。 */
static ssize_t read_line(rio_t *rio, char *line)
{
    ssize_t n = rio_readlineb(rio, line, MAXLINE);
    if (n <= 0)
        return n;
    if (n < 2 || line[n - 2] != '\r' || line[n - 1] != '\n')
        return -1;
    for (ssize_t i = 0; i < n - 2; i++)
        if (((unsigned char)line[i] < 32 && line[i] != '\t') ||
            (unsigned char)line[i] == 127)
            return -1;
    return n;
}

static int header_is(const char *line, size_t len, const char *name)
{
    return strlen(name) == len && strncasecmp(line, name, len) == 0;
}

/* 检查分块响应是否包含完整数据块、结束块和尾部空行；缓存保留原始编码。 */
static int complete_chunks(const unsigned char *data, size_t size)
{
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = 0, digits = 0;
        while (offset < size && isxdigit(data[offset])) {
            unsigned char c = data[offset++];
            chunk = chunk * 16 + (isdigit(c) ? c - '0' : tolower(c) - 'a' + 10);
            if (chunk > MAX_OBJECT_SIZE) return 0;
            digits++;
        }
        if (!digits) return 0;
        if (offset < size && data[offset] == ';') {
            while (offset < size && data[offset] != '\r') offset++;
        }
        if (offset + 2 > size || memcmp(data + offset, "\r\n", 2)) return 0;
        offset += 2;
        if (!chunk) {
            /* 结束块之后允许尾部字段，但必须以空行结束整个消息。 */
            while (offset + 2 <= size) {
                size_t start = offset;
                while (offset + 1 < size && memcmp(data + offset, "\r\n", 2)) offset++;
                if (offset + 2 > size) return 0;
                if (offset == start) return offset + 2 == size;
                if (!memchr(data + start, ':', offset - start)) return 0;
                offset += 2;
            }
            return 0;
        }
        if (chunk + 2 > size - offset) return 0;
        offset += chunk;
        if (memcmp(data + offset, "\r\n", 2)) return 0;
        offset += 2;
    }
    return 0;
}

/* 只保存完整的成功响应，防止正常 EOF 掩盖长度不足或缺少结束块。 */
static int cacheable_response(const unsigned char *data, size_t size)
{
    size_t offset = 0, body = 0, length = 0;
    int has_length = 0, chunked = 0;
    char line[MAXLINE];
    while (offset < size) {
        size_t end = offset;
        while (end + 1 < size && !(data[end] == '\r' && data[end + 1] == '\n'))
            end++;
        if (end + 1 >= size || end - offset >= sizeof(line))
            return 0;
        memcpy(line, data + offset, end - offset);
        line[end - offset] = '\0';
        if (memchr(data + offset, 0, end - offset))
            return 0;
        if (offset == 0) {
            if (strncmp(line, "HTTP/1.0 200 ", 13) && strncmp(line, "HTTP/1.1 200 ", 13))
                return 0;
        } else if (!*line) {
            body = end + 2;
            break;
        } else {
            char *colon = strchr(line, ':');
            if (!colon)
                return 0;
            if (header_is(line, (size_t)(colon - line), "Transfer-Encoding")) {
                char *value = colon + 1, *end;
                while (*value == ' ' || *value == '\t') value++;
                end = value + strlen(value);
                while (end > value && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
                if (chunked || strcasecmp(value, "chunked")) return 0;
                chunked = 1;
            }
            if (header_is(line, (size_t)(colon - line), "Content-Length")) {
                char *p = colon + 1;
                if (has_length++)
                    return 0;
                while (*p == ' ' || *p == '\t') p++;
                if (!isdigit((unsigned char)*p)) return 0;
                while (isdigit((unsigned char)*p)) {
                    length = length * 10 + (size_t)(*p++ - '0');
                    if (length > MAX_OBJECT_SIZE) return 0;
                }
                while (*p == ' ' || *p == '\t') p++;
                if (*p) return 0;
            }
        }
        offset = end + 2;
    }
    if (!body || (chunked && has_length)) return 0;
    return chunked ? complete_chunks(data + body, size - body) :
                     (!has_length || size - body == length);
}

static void handle_client(int clientfd)
{
    rio_t rio;
    char line[MAXLINE], method[MAXLINE], url[MAXLINE], version[MAXLINE], extra;
    char host[MAXLINE], port[MAXLINE], path[MAXLINE], authority[MAXLINE];
    char headers[HEADER_LIMIT], request_line[MAXLINE + 32];
    size_t used = 0, total = 0;
    int has_host = 0, serverfd;
    unsigned char *object = NULL;
    char *cache_key = NULL;
    size_t object_size = 0;
    ssize_t n;

    rio_readinitb(&rio, clientfd);
    n = read_line(&rio, line);
    if (n == 0)
        return;
    /* 输入行最长 MAXLINE-1 字节，因此各字段不会超过同样大小的数组。 */
    if (n < 0 || sscanf(line, "%s %s %s %c", method, url, version, &extra) != 3 ||
        (strcmp(version, "HTTP/1.0") && strcmp(version, "HTTP/1.1"))) {
        send_error(clientfd, 400, "Bad Request");
        return;
    }
    if (strcmp(method, "GET")) {
        send_error(clientfd, 501, "Not Implemented");
        return;
    }
    if (parse_url(url, host, port, path, authority) < 0) {
        send_error(clientfd, 400, "Bad Request");
        return;
    }

    /* 先完整读取请求头；保留浏览器提供的 Host 和其他附加头。 */
    while ((n = read_line(&rio, line)) > 0) {
        char *colon, *value;
        size_t name_len;
        total += (size_t)n;
        if (total >= HEADER_LIMIT) {
            send_error(clientfd, 431, "Request Header Fields Too Large");
            return;
        }
        if (!strcmp(line, "\r\n"))
            break;
        colon = strchr(line, ':');
        if (!colon || colon == line)
            goto bad_request;
        name_len = (size_t)(colon - line);
        for (char *p = line; p < colon; p++)
            if (!isalnum((unsigned char)*p) && !strchr("!#$%&'*+-.^_`|~", *p))
                goto bad_request;
        value = colon + 1;
        while (*value == ' ' || *value == '\t')
            value++;
        if (header_is(line, name_len, "Host")) {
            if (has_host || !strcmp(value, "\r\n"))
                goto bad_request;
            has_host = 1;
        }
        /* 本阶段只支持无请求体的 GET，不等待或转发请求体。 */
        if (header_is(line, name_len, "Transfer-Encoding"))
            goto bad_request;
        if (header_is(line, name_len, "Content-Length")) {
            char *p = value;
            if (*p != '0')
                goto bad_request;
            while (*p == '0')
                p++;
            while (*p == ' ' || *p == '\t')
                p++;
            if (strcmp(p, "\r\n"))
                goto bad_request;
        }
        if (header_is(line, name_len, "User-Agent") ||
            header_is(line, name_len, "Connection") ||
            header_is(line, name_len, "Proxy-Connection"))
            continue;
        memcpy(headers + used, line, (size_t)n);
        used += (size_t)n;
    }
    if (n <= 0)
        goto bad_request;

    /* 附加转发头也纳入键，避免不同 Host、Range 等请求误用同一响应。 */
    headers[used] = '\0';
    size_t key_size = strlen(host) + strlen(port) + strlen(path) +
                      strlen(authority) + used + 8;
    cache_key = malloc(key_size);
    object = malloc(MAX_OBJECT_SIZE);
    if (cache_key && object) {
        snprintf(cache_key, key_size, "%s\n%s\n%s\n%s\n%s", host, port, path,
                 has_host ? "" : authority, headers);
        if (cache_get(cache_key, object, &object_size)) {
            if (rio_writen(clientfd, object, object_size) < 0)
                perror("write cached response");
            free(cache_key);
            free(object);
            return;
        }
    } else {
        free(cache_key);
        free(object);
        cache_key = NULL;
        object = NULL;
    }

    serverfd = open_clientfd(host, port);
    if (serverfd < 0) {
        send_error(clientfd, 502, "Bad Gateway");
        free(cache_key);
        free(object);
        return;
    }
    n = snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.0\r\n", path);
    if (rio_writen(serverfd, request_line, (size_t)n) < 0)
        goto upstream_error;
    if (!has_host) {
        n = snprintf(request_line, sizeof(request_line), "Host: %s\r\n", authority);
        if (rio_writen(serverfd, request_line, (size_t)n) < 0)
            goto upstream_error;
    }
    if (rio_writen(serverfd, headers, used) < 0 ||
        rio_writen(serverfd, (void *)user_agent_hdr, strlen(user_agent_hdr)) < 0)
        goto upstream_error;
    strcpy(line, "Connection: close\r\nProxy-Connection: close\r\n\r\n");
    if (rio_writen(serverfd, line, strlen(line)) < 0)
        goto upstream_error;

    /* 响应按实际字节数原样转发，兼容图片和零字节；直到源服务器关闭连接。 */
    while ((n = read(serverfd, line, sizeof(line))) != 0) {
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("read upstream");
            break;
        }
        if (rio_writen(clientfd, line, (size_t)n) < 0) {
            perror("write client");
            break;
        }
        if (object) {
            if (object_size + (size_t)n <= MAX_OBJECT_SIZE) {
                memcpy(object + object_size, line, (size_t)n);
                object_size += (size_t)n;
            } else {
                free(object);
                object = NULL;
            }
        }
    }
    if (n == 0 && object && cacheable_response(object, object_size))
        cache_put(cache_key, object, object_size);
    free(cache_key);
    free(object);
    close_fd(serverfd);
    return;

upstream_error:
    free(cache_key);
    free(object);
    close_fd(serverfd);
    send_error(clientfd, 502, "Bad Gateway");
    return;
bad_request:
    send_error(clientfd, 400, "Bad Request");
}

/* 参数内存属于此工作线程；先复制描述符，再释放参数并处理连接。 */
static void *serve_client(void *arg)
{
    int connfd = *(int *)arg;
    free(arg);
    handle_client(connfd);
    close_fd(connfd);
    return NULL;
}

int main(int argc, char **argv)
{
    int listenfd, rc;
    pthread_attr_t attributes;
    struct sigaction action = {0};

    if (argc != 2 || !valid_port(argv[1])) {
        fprintf(stderr, "Usage: %s <port: 1-65535>\n", argv[0]);
        return 1;
    }
    /* 对方提前断开时由写入返回值报告错误，避免 SIGPIPE 终止代理。 */
    action.sa_handler = SIG_IGN;
    if (sigemptyset(&action.sa_mask) < 0 || sigaction(SIGPIPE, &action, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    /* 创建时即设为分离线程，结束后自动回收线程资源，无需主线程等待。 */
    rc = pthread_attr_init(&attributes);
    if (rc != 0) {
        fprintf(stderr, "pthread_attr_init: %s\n", strerror(rc));
        return 1;
    }
    rc = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    if (rc != 0) {
        fprintf(stderr, "pthread_attr_setdetachstate: %s\n", strerror(rc));
        pthread_attr_destroy(&attributes);
        return 1;
    }
    listenfd = open_listenfd(argv[1]);
    if (listenfd < 0) {
        pthread_attr_destroy(&attributes);
        return 1;
    }
    for (;;) {
        pthread_t tid;
        int *connfdp;
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd < 0) {
            if (errno != EINTR)
                perror("accept");
            continue;
        }
        /* 每条连接单独传参，避免传递循环局部变量地址导致描述符被覆盖。 */
        connfdp = malloc(sizeof(*connfdp));
        if (!connfdp) {
            perror("malloc connection");
            close_fd(connfd);
            continue;
        }
        *connfdp = connfd;
        rc = pthread_create(&tid, &attributes, serve_client, connfdp);
        if (rc != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            free(connfdp);
            close_fd(connfd);
        }
        /* 创建成功后，参数及连接的所有权交给工作线程，主线程不再访问。 */
    }
}
