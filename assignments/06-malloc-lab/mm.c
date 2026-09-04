/*
 * 基于边界标记的显式空闲链表分配器。
 *
 * 每个块都通过块头和块尾保存总大小及分配标记。空闲块在有效载荷区域中保存
 * 前驱和后继指针，并按地址顺序组织成双向链表。块释放后会立即与相邻空闲块
 * 合并。
 */
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * 学生注意：开始实验前，请先在下面的结构体中填写团队信息。
 ********************************************************/
team_t team = {
    /* 团队名称 */
    "ateam",
    /* 第一位成员的姓名 */
    "Harry Bovik",
    /* 第一位成员的邮箱 */
    "bovik@cs.cmu.edu",
    /* 第二位成员的姓名（没有则留空） */
    "",
    /* 第二位成员的邮箱（没有则留空） */
    ""
};

/* 基本常量和宏。 */
#define ALIGNMENT 8
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)
#define MIN_BLOCK_SIZE (DSIZE + 2 * sizeof(void *))

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))
#define PRED(bp) (*(void **)(bp))
#define SUCC(bp) (*(void **)((char *)(bp) + sizeof(void *)))

static char *heap_listp;
static void *free_listp;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void insert_free(void *bp);
static void remove_free(void *bp);
static size_t adjust_block_size(size_t size);
static void resize_block(void *bp, size_t total_size, size_t asize);
static int in_heap(const void *p);
static int aligned(const void *p);
int mm_check(void);

/*
 * extend_heap - 扩展堆，并创建一个对齐的空闲块。
 */
static void *extend_heap(size_t words)
{
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    char *bp = mem_sbrk((int)size);

    if (bp == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp);
}

/*
 * coalesce - 将当前空闲块与相邻空闲块合并。
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    void *prev = PREV_BLKP(bp);
    void *next = NEXT_BLKP(bp);

    if (!prev_alloc) {
        remove_free(prev);
        size += GET_SIZE(HDRP(prev));
        bp = prev;
    }

    if (!next_alloc) {
        remove_free(next);
        size += GET_SIZE(HDRP(next));
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_free(bp);
    return bp;
}

/*
 * mm_init - 创建初始空堆及第一个空闲块。
 */
int mm_init(void)
{
    free_listp = NULL;
    heap_listp = mem_sbrk(4 * WSIZE);
    if (heap_listp == (void *)-1)
        return -1;

    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1));
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));
    heap_listp += 2 * WSIZE;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

#ifdef DEBUG
    assert(mm_check());
#endif
    return 0;
}

/*
 * find_fit - 查找第一个总大小不小于 asize 的空闲块。
 */
static void *find_fit(size_t asize)
{
    void *bp;

    for (bp = free_listp; bp != NULL; bp = SUCC(bp)) {
        if (asize <= GET_SIZE(HDRP(bp)))
            return bp;
    }
    return NULL;
}

/*
 * place - 在空闲块中完成分配，并在剩余空间足够时切分。
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    remove_free(bp);
    if (csize - asize >= MIN_BLOCK_SIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_free(bp);
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * insert_free - 将空闲块按地址顺序插入空闲链表。
 */
static void insert_free(void *bp)
{
    void *prev = NULL;
    void *next = free_listp;

    while (next != NULL && (char *)next < (char *)bp) {
        prev = next;
        next = SUCC(next);
    }

    PRED(bp) = prev;
    SUCC(bp) = next;
    if (prev != NULL)
        SUCC(prev) = bp;
    else
        free_listp = bp;

    if (next != NULL)
        PRED(next) = bp;
}

/*
 * remove_free - 从空闲链表中移除指定块。
 */
static void remove_free(void *bp)
{
    void *prev = PRED(bp);
    void *next = SUCC(bp);

    if (prev != NULL)
        SUCC(prev) = next;
    else
        free_listp = next;

    if (next != NULL)
        PRED(next) = prev;
}

/*
 * adjust_block_size - 加上元数据开销，并将请求向上对齐为合法块大小。
 */
static size_t adjust_block_size(size_t size)
{
    size_t asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);

    return MAX(asize, MIN_BLOCK_SIZE);
}

/*
 * resize_block - 在 total_size 范围内调整已分配块的大小。
 */
static void resize_block(void *bp, size_t total_size, size_t asize)
{
    if (total_size - asize >= MIN_BLOCK_SIZE) {
        void *remainder;

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        remainder = NEXT_BLKP(bp);
        PUT(HDRP(remainder), PACK(total_size - asize, 0));
        PUT(FTRP(remainder), PACK(total_size - asize, 0));
        coalesce(remainder);
    } else {
        PUT(HDRP(bp), PACK(total_size, 1));
        PUT(FTRP(bp), PACK(total_size, 1));
    }
}

/*
 * mm_malloc - 分配一个对齐块，其有效载荷至少包含 size 字节。
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    asize = adjust_block_size(size);

    bp = find_fit(asize);
    if (bp == NULL) {
        extendsize = MAX(asize, CHUNKSIZE);
        bp = extend_heap(extendsize / WSIZE);
        if (bp == NULL)
            return NULL;
    }

    place(bp, asize);
#ifdef DEBUG
    assert(mm_check());
#endif
    return bp;
}

/*
 * mm_free - 将块标记为空闲，并立即尝试合并相邻空闲块。
 */
void mm_free(void *ptr)
{
    size_t size;

    if (ptr == NULL)
        return;

    size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
#ifdef DEBUG
    assert(mm_check());
#endif
}

/*
 * mm_realloc - 尽量原地调整块大小，否则分配新块并复制数据。
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *newptr;
    void *next;
    size_t asize;
    size_t old_size;
    size_t total_size;
    size_t copy_size;

    if (ptr == NULL)
        return mm_malloc(size);

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    asize = adjust_block_size(size);
    old_size = GET_SIZE(HDRP(ptr));

    if (asize <= old_size) {
        resize_block(ptr, old_size, asize);
#ifdef DEBUG
        assert(mm_check());
#endif
        return ptr;
    }

    next = NEXT_BLKP(ptr);
    total_size = old_size + GET_SIZE(HDRP(next));
    if (!GET_ALLOC(HDRP(next)) && total_size >= asize) {
        remove_free(next);
        resize_block(ptr, total_size, asize);
#ifdef DEBUG
        assert(mm_check());
#endif
        return ptr;
    }

    if (GET_SIZE(HDRP(next)) == 0) {
        size_t extendsize = MAX(asize - old_size, CHUNKSIZE);

        if (extend_heap(extendsize / WSIZE) != NULL) {
            next = NEXT_BLKP(ptr);
            total_size = old_size + GET_SIZE(HDRP(next));
            if (!GET_ALLOC(HDRP(next)) && total_size >= asize) {
                remove_free(next);
                resize_block(ptr, total_size, asize);
#ifdef DEBUG
                assert(mm_check());
#endif
                return ptr;
            }
        }
    }

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    copy_size = old_size - DSIZE;
    if (size < copy_size)
        copy_size = size;
    memcpy(newptr, ptr, copy_size);
    mm_free(ptr);
    return newptr;
}

/*
 * in_heap - 判断 p 是否指向模拟堆内部。
 */
static int in_heap(const void *p)
{
    return p >= mem_heap_lo() && p <= mem_heap_hi();
}

/*
 * aligned - 判断 p 是否满足规定的对齐要求。
 */
static int aligned(const void *p)
{
    return (uintptr_t)p % ALIGNMENT == 0;
}

/*
 * mm_check - 检查堆和显式空闲链表必须满足的不变量。
 */
int mm_check(void)
{
    char *bp;
    void *free_bp;
    size_t heap_free_count = 0;
    size_t list_free_count = 0;
    size_t max_blocks = mem_heapsize() / MIN_BLOCK_SIZE + 1;

    if (heap_listp == NULL || !in_heap(HDRP(heap_listp))) {
        fprintf(stderr, "invalid heap list pointer\n");
        return 0;
    }

    if (GET_SIZE(HDRP(heap_listp)) != DSIZE
        || !GET_ALLOC(HDRP(heap_listp))) {
        fprintf(stderr, "invalid prologue block\n");
        return 0;
    }

    for (bp = heap_listp; ; bp = NEXT_BLKP(bp)) {
        size_t size;
        int alloc;

        if (!in_heap(HDRP(bp))) {
            fprintf(stderr, "block header is outside the heap\n");
            return 0;
        }

        size = GET_SIZE(HDRP(bp));
        alloc = GET_ALLOC(HDRP(bp));
        if (size == 0) {
            if (!alloc) {
                fprintf(stderr, "invalid epilogue block\n");
                return 0;
            }
            break;
        }

        if (!aligned(bp) || size % ALIGNMENT != 0) {
            fprintf(stderr, "misaligned block at %p\n", (void *)bp);
            return 0;
        }

        if (!in_heap(FTRP(bp)) || GET(HDRP(bp)) != GET(FTRP(bp))) {
            fprintf(stderr, "header/footer mismatch at %p\n", (void *)bp);
            return 0;
        }

        if (bp != heap_listp && size < MIN_BLOCK_SIZE) {
            fprintf(stderr, "block is smaller than the minimum size\n");
            return 0;
        }

        if (!alloc && GET_SIZE(HDRP(NEXT_BLKP(bp))) != 0
            && !GET_ALLOC(HDRP(NEXT_BLKP(bp)))) {
            fprintf(stderr, "adjacent free blocks were not coalesced\n");
            return 0;
        }

        if (!alloc)
            heap_free_count++;
    }

    for (free_bp = free_listp; free_bp != NULL; free_bp = SUCC(free_bp)) {
        void *prev;
        void *next;

        if (!in_heap(free_bp) || !aligned(free_bp)
            || GET_ALLOC(HDRP(free_bp))) {
            fprintf(stderr, "invalid block in free list\n");
            return 0;
        }

        prev = PRED(free_bp);
        next = SUCC(free_bp);
        if ((prev != NULL && (!in_heap(prev) || SUCC(prev) != free_bp))
            || (next != NULL && (!in_heap(next) || PRED(next) != free_bp))) {
            fprintf(stderr, "inconsistent free-list links\n");
            return 0;
        }

        if (++list_free_count > max_blocks) {
            fprintf(stderr, "cycle detected in free list\n");
            return 0;
        }
    }

    if (heap_free_count != list_free_count) {
        fprintf(stderr, "heap and free-list counts differ\n");
        return 0;
    }

    return 1;
}
