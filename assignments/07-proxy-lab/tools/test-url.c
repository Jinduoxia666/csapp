/* 单独验证 URL 解析边界，无需监听特权端口或暴露生产接口。 */
#define main proxy_main
#include "../proxy.c"
#undef main
#include <assert.h>

static void check(const char *input, const char *expected_host,
                  const char *expected_port, const char *expected_path)
{
    char url[MAXLINE], host[MAXLINE], port[MAXLINE], path[MAXLINE], authority[MAXLINE];
    strcpy(url, input);
    int result = parse_url(url, host, port, path, authority);
    if (!expected_host) {
        assert(result == -1);
        return;
    }
    assert(result == 0);
    assert(!strcmp(host, expected_host));
    assert(!strcmp(port, expected_port));
    assert(!strcmp(path, expected_path));
}

int main(void)
{
    check("http://example.com/index.html?q=1", "example.com", "80", "/index.html?q=1");
    check("http://example.com", "example.com", "80", "/");
    check("http://example.com?x=1", "example.com", "80", "/?x=1");
    check("http://example.com:8080/a", "example.com", "8080", "/a");
    check("http://[::1]:8080/a", "::1", "8080", "/a");
    check("http://[::1]/", "::1", "80", "/");
    check("http://example.com:0/", NULL, NULL, NULL);
    check("http://example.com:65536/", NULL, NULL, NULL);
    check("http://example.com:/", NULL, NULL, NULL);
    check("http://example.com:abc/", NULL, NULL, NULL);
    check("http:///a", NULL, NULL, NULL);
    check("http://[bad]/", NULL, NULL, NULL);
    check("https://example.com/", NULL, NULL, NULL);
    puts("URL 解析边界检查通过（13 个场景）");
    return 0;
}
