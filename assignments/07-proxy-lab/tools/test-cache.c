/* 直接检查缓存容量、淘汰和锁行为，不向代理增加调试接口。 */
#include "../cache.c"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static unsigned char data[MAX_OBJECT_SIZE + 1];

static void reset_cache(void)
{
    while (entries) {
        cache_entry *next = entries->next;
        free(entries);
        entries = next;
    }
    bytes_used = 0;
}

static _Atomic int copied;

static void *reader(void *unused)
{
    unsigned char output[MAX_OBJECT_SIZE];
    size_t size;
    (void)unused;
    assert(cache_get("shared", output, &size));
    assert(size == 100 && !memcmp(output, data, size));
    atomic_store(&copied, 1);
    return NULL;
}

static void *stress(void *argument)
{
    int id = *(int *)argument;
    unsigned char input[MAX_OBJECT_SIZE], output[MAX_OBJECT_SIZE];
    for (int i = 0; i < 300; i++) {
        int value = (i + id) % 16;
        char key[32];
        size_t size;
        snprintf(key, sizeof(key), "stress-%d", value);
        memset(input, value, sizeof(input));
        if (id % 2)
            cache_put(key, input, sizeof(input));
        if (cache_get(key, output, &size)) {
            assert(size == sizeof(input));
            assert(!memcmp(input, output, size));
        }
    }
    return NULL;
}

int main(void)
{
    unsigned char output[MAX_OBJECT_SIZE];
    size_t size;
    pthread_t thread, workers[8];
    int ids[8];
    memset(data, 0xa5, sizeof(data));
    cache_put("too-large", data, MAX_OBJECT_SIZE + 1);
    assert(!cache_get("too-large", output, &size) && bytes_used == 0);
    cache_put("boundary", data, MAX_OBJECT_SIZE);
    assert(cache_get("boundary", output, &size) && size == MAX_OBJECT_SIZE);
    assert(!memcmp(output, data, size));
    cache_put("boundary", data, MAX_OBJECT_SIZE);
    assert(bytes_used == MAX_OBJECT_SIZE);
    reset_cache();

    /* 装满十个大对象，读取最旧对象后，再插入应淘汰第二旧的对象。 */
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "entry-%d", i);
        cache_put(key, data, MAX_OBJECT_SIZE);
    }
    assert(cache_get("entry-0", output, &size));
    cache_put("entry-10", data, MAX_OBJECT_SIZE);
    assert(cache_get("entry-0", output, &size));
    assert(!cache_get("entry-1", output, &size));
    assert(bytes_used == 10 * MAX_OBJECT_SIZE);
    assert(bytes_used <= MAX_CACHE_SIZE);
    reset_cache();

    /* 主线程持有读锁时，另一线程必须能完成 cache_get。 */
    cache_put("shared", data, 100);
    assert(pthread_rwlock_rdlock(&lock) == 0);
    assert(pthread_create(&thread, NULL, reader, NULL) == 0);
    for (int i = 0; i < 2000 && !atomic_load(&copied); i++)
        usleep(1000);
    int concurrent = atomic_load(&copied);
    pthread_rwlock_unlock(&lock);
    pthread_join(thread, NULL);
    assert(concurrent);
    reset_cache();

    /* 并发读、插入和淘汰，检查读取副本始终完整。 */
    for (int i = 0; i < 8; i++) {
        ids[i] = i;
        assert(pthread_create(&workers[i], NULL, stress, &ids[i]) == 0);
    }
    for (int i = 0; i < 8; i++)
        pthread_join(workers[i], NULL);
    size_t actual = 0;
    for (cache_entry *entry = entries; entry; entry = entry->next)
        actual += entry->size;
    assert(actual == bytes_used && actual <= MAX_CACHE_SIZE);
    reset_cache();
    puts("缓存容量、LRU、重复插入、并行读和并发淘汰检查通过");
    return 0;
}
