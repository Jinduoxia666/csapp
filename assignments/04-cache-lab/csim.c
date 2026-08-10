/*
 * csim.c - 缓存模拟器
 *
 * 姓名：
 * loginID：
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cachelab.h"

/* 一个缓存行。模拟器不存数据，只记录这个块在不在、是谁、多久没用了 */
typedef struct {
    int valid;
    unsigned long tag;
    unsigned long stamp; /* LRU 时间戳：越大表示越近被访问过 */
} cache_line_t;

/* 缓存参数与状态 */
static int s, E, b;         /* 组索引位数、相联度、块偏移位数 */
static int verbose;         /* -v */
static cache_line_t **sets; /* sets[组号][行号]，共 2^s 组，每组 E 行 */

static int hit_count, miss_count, eviction_count;
static unsigned long timer; /* 全局递增计数器，用来生成 stamp */

/*
 * access_cache - 模拟对地址 addr 的一次访问，更新三个计数器
 *
 * 地址被切成三段：高位是 tag，中间 s 位是组索引，低 b 位是块内偏移。
 * 偏移用不上——假设访问不跨块，所以 trace 里的 size 字段可以忽略。
 *
 * verbose 输出不换行，换行由调用方在一行处理完后统一打印，
 * 这样 M 操作的两次访问才能拼成 "M 20,1 miss hit " 这样的一行。
 * 每个词后面跟一个空格，是为了和 csim-ref 的输出逐字节一致，方便 diff 对比。
 */
static void access_cache(unsigned long addr)
{
    unsigned long set = (addr >> b) & ((1UL << s) - 1);
    unsigned long tag = addr >> (s + b);
    cache_line_t *lines = sets[set];
    int victim = 0;

    /* 1. 在这一组的 E 行里找 valid 且 tag 相同的行 */
    for (int i = 0; i < E; i++) {
        if (lines[i].valid && lines[i].tag == tag) {
            hit_count++;
            lines[i].stamp = ++timer; /* 命中也要更新，否则 LRU 退化成 FIFO */
            if (verbose)
                printf("hit ");
            return;
        }
    }

    /* 2. 没找到就是不命中 */
    miss_count++;
    if (verbose)
        printf("miss ");

    /*
     * 3. 挑一行装这个块：优先用空行；没有空行才逐出 stamp 最小（最久没用）的那行。
     *    victim 在循环中始终指向已考察过的行里 stamp 最小的一个。
     */
    for (int i = 0; i < E; i++) {
        if (!lines[i].valid) {
            victim = i;
            break;
        }
        if (lines[i].stamp < lines[victim].stamp)
            victim = i;
    }

    /* 覆盖掉一个有效行才算逐出；装进空行只算冷不命中 */
    if (lines[victim].valid) {
        eviction_count++;
        if (verbose)
            printf("eviction ");
    }

    lines[victim].valid = 1;
    lines[victim].tag = tag;
    lines[victim].stamp = ++timer;
}

static void usage(const char *name)
{
    printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n", name);
    printf("  -h         Print this help message\n");
    printf("  -v         Optional verbose flag\n");
    printf("  -s <s>     Number of set index bits\n");
    printf("  -E <E>     Number of lines per set\n");
    printf("  -b <b>     Number of block offset bits\n");
    printf("  -t <file>  Trace file\n");
}

int main(int argc, char **argv)
{
    char *tracefile = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
        case 'h': usage(argv[0]); return 0;
        case 'v': verbose = 1; break;
        case 's': s = atoi(optarg); break;
        case 'E': E = atoi(optarg); break;
        case 'b': b = atoi(optarg); break;
        case 't': tracefile = optarg; break;
        default:  usage(argv[0]); return 1;
        }
    }

    if (s <= 0 || E <= 0 || b <= 0 || tracefile == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        usage(argv[0]);
        return 1;
    }

    /* 分配 2^s 组，每组 E 行，全部清零（valid = 0） */
    long num_sets = 1L << s;
    sets = malloc(num_sets * sizeof(cache_line_t *));
    if (sets == NULL) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }
    for (long i = 0; i < num_sets; i++) {
        sets[i] = calloc(E, sizeof(cache_line_t));
        if (sets[i] == NULL) {
            fprintf(stderr, "calloc failed\n");
            return 1;
        }
    }

    FILE *fp = fopen(tracefile, "r");
    if (fp == NULL) {
        fprintf(stderr, "%s: No such file or directory\n", tracefile);
        return 1;
    }

    /* 逐行读 trace：格式为 [空格]操作 地址,大小 */
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        char op;
        unsigned long addr;
        int size;

        /* 'I' 开头（行首无空格）是取指，本实验忽略 */
        if (line[0] == 'I')
            continue;
        if (sscanf(line, " %c %lx,%d", &op, &addr, &size) != 3)
            continue;

        if (verbose)
            printf("%c %lx,%d ", op, addr, size);

        access_cache(addr);
        if (op == 'M')      /* M = 读后紧跟一次对同一地址的写，第二次必命中 */
            access_cache(addr);

        if (verbose)
            printf("\n");
    }

    fclose(fp);
    for (long i = 0; i < num_sets; i++)
        free(sets[i]);
    free(sets);

    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
