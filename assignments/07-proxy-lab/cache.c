#include "cache.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct cache_entry {
    struct cache_entry *next;
    char *key;
    size_t size;
    _Atomic uint64_t accessed;
    unsigned char data[];
} cache_entry;

static pthread_rwlock_t lock = PTHREAD_RWLOCK_INITIALIZER;
static cache_entry *entries;
static size_t bytes_used;
static _Atomic uint64_t sequence;

int cache_get(const char *key, void *output, size_t *size)
{
    int found = 0;
    if (pthread_rwlock_rdlock(&lock) != 0)
        return 0;
    for (cache_entry *entry = entries; entry; entry = entry->next) {
        if (strcmp(entry->key, key))
            continue;
        /* 多个读者可以同时复制；写者必须等待读锁释放才能淘汰条目。 */
        memcpy(output, entry->data, entry->size);
        *size = entry->size;
        atomic_store(&entry->accessed, atomic_fetch_add(&sequence, 1) + 1);
        found = 1;
        break;
    }
    pthread_rwlock_unlock(&lock);
    return found;
}

void cache_put(const char *key, const void *data, size_t size)
{
    cache_entry *entry;
    if (!size || size > MAX_OBJECT_SIZE || pthread_rwlock_wrlock(&lock) != 0)
        return;
    /* 同一对象的并发回源只保留一份，不重复占用容量。 */
    for (entry = entries; entry; entry = entry->next) {
        if (!strcmp(entry->key, key)) {
            atomic_store(&entry->accessed, atomic_fetch_add(&sequence, 1) + 1);
            pthread_rwlock_unlock(&lock);
            return;
        }
    }
    while (bytes_used + size > MAX_CACHE_SIZE) {
        cache_entry **victim = &entries;
        for (cache_entry **p = &entries; *p; p = &(*p)->next)
            if (atomic_load(&(*p)->accessed) < atomic_load(&(*victim)->accessed))
                victim = p;
        entry = *victim;
        *victim = entry->next;
        bytes_used -= entry->size;
        free(entry);
    }
    /* 对象和键放在同一分配块中，淘汰时一次释放；只统计响应字节。 */
    entry = malloc(sizeof(*entry) + size + strlen(key) + 1);
    if (entry) {
        entry->key = (char *)entry->data + size;
        strcpy(entry->key, key);
        memcpy(entry->data, data, size);
        entry->size = size;
        atomic_init(&entry->accessed, atomic_fetch_add(&sequence, 1) + 1);
        entry->next = entries;
        entries = entry;
        bytes_used += size;
    }
    pthread_rwlock_unlock(&lock);
}
