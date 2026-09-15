#ifndef PROXY_CACHE_H
#define PROXY_CACHE_H

#include <stddef.h>

/* 沿用官方 handout 的字节数上限；元数据不计入缓存对象容量。 */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* 调用者提供至少 MAX_OBJECT_SIZE 字节的缓冲区。命中返回 1。 */
int cache_get(const char *key, void *output, size_t *size);
void cache_put(const char *key, const void *data, size_t size);

#endif
