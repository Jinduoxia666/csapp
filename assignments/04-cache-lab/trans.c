/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * 评测用的缓存：s=5, E=1, b=5 —— 32 组、直接映射、块 32 字节（= 8 个 int），
 * 总共 1KB。关键推论：
 *   - 32x32：一行 32 个 int = 4 个块，8 行正好填满缓存，
 *            所以 A[i][j] 与 A[i+8][j] 组号相同、互相冲突。
 *   - 64x64：一行 64 个 int = 8 个块，4 行就填满缓存，
 *            所以 A[i][j] 与 A[i+4][j] 冲突 —— 8x8 分块会自己打自己。
 * 另外 A 和 B 是两个独立数组，块地址错开，对角线上的块常常映射到同一组，
 * 于是"读 A 一格、写 B 一格"会来回互相驱逐。对策是先把一整行读进寄存器再写。
 */
static void trans_32(int M, int N, int A[N][M], int B[M][N]);
static void trans_64(int M, int N, int A[N][M], int B[M][N]);
static void trans_blocked(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    /*
     * 本函数自身不声明局部变量，三条分支各自的变量数都 <= 12，
     * 这样"栈上同时存在的局部变量不超过 12 个"这条规则就不会被违反。
     * PDF 明确允许按输入尺寸分派到各自优化的代码。
     */
    if (M == 32 && N == 32)
        trans_32(M, N, A, B);
    else if (M == 64 && N == 64)
        trans_64(M, N, A, B);
    else
        trans_blocked(M, N, A, B);
}

/*
 * trans_32 - 32x32：8x8 分块，每次把 A 的一整行（8 个 int，正好一个块）
 *            读进 8 个局部变量，再写到 B 的一列上。
 *
 * 为什么要先读进寄存器：对角线上的块，A[k][j..j+7] 和 B[j..j+7][k] 会落到
 * 同一组。如果边读边写，写 B 会把 A 那一行的块挤掉，下一次读 A 又得重新加载，
 * 一个块被反复驱逐 8 次。先一次性读完整块，A 的那一行就只需加载一次。
 *
 * 局部变量：i, j, k, a0..a7 = 11 个。
 */
static void trans_32(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    for (i = 0; i < N; i += 8) {
        for (j = 0; j < M; j += 8) {
            for (k = i; k < i + 8; k++) {
                a0 = A[k][j + 0];
                a1 = A[k][j + 1];
                a2 = A[k][j + 2];
                a3 = A[k][j + 3];
                a4 = A[k][j + 4];
                a5 = A[k][j + 5];
                a6 = A[k][j + 6];
                a7 = A[k][j + 7];

                B[j + 0][k] = a0;
                B[j + 1][k] = a1;
                B[j + 2][k] = a2;
                B[j + 3][k] = a3;
                B[j + 4][k] = a4;
                B[j + 5][k] = a5;
                B[j + 6][k] = a6;
                B[j + 7][k] = a7;
            }
        }
    }
}

/*
 * trans_64 - 64x64：仍按 8x8 分块，但块内拆成 4 个 4x4 象限处理。
 *
 * 难点：64 列时缓存只装得下 4 行，8x8 块的上半 4 行和下半 4 行组号完全重叠。
 * 直接做 8x8 会让 B 的上下半互相驱逐，miss 数飙到 4000 以上。
 *
 * 解法是借 B 块的右上角当暂存区（它反正马上要被写，先放着不亏）：
 *   第 1 步：读 A 上半 4 行（每次一整行 8 个 int）。左边 4 个直接转置到
 *            B 的左上角；右边 4 个"寄存"在 B 的右上角，位置暂时是错的。
 *   第 2 步：逐列把寄存的值取出来，同时读 A 的左下角 4x4，
 *            用 A 的左下角覆盖 B 的右上角（这才是它的正确位置），
 *            再把取出来的寄存值放到 B 的左下角。一读一写都在同一行上，
 *            每次只碰 B 的两行，不会触发上下半互相驱逐。
 *   第 3 步：A 的右下角 4x4 直接转置到 B 的右下角。
 *
 * 局部变量：i, j, x, y, a0..a7 = 12 个，正好卡在上限。
 */
static void trans_64(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, x, y;
    int a0, a1, a2, a3, a4, a5, a6, a7;

    for (x = 0; x < N; x += 8) {
        for (y = 0; y < M; y += 8) {
            /* 第 1 步：A 的上半 4 行 -> B 左上角（就位）+ B 右上角（暂存） */
            for (i = 0; i < 4; i++) {
                a0 = A[x + i][y + 0];
                a1 = A[x + i][y + 1];
                a2 = A[x + i][y + 2];
                a3 = A[x + i][y + 3];
                a4 = A[x + i][y + 4];
                a5 = A[x + i][y + 5];
                a6 = A[x + i][y + 6];
                a7 = A[x + i][y + 7];

                B[y + 0][x + i] = a0;
                B[y + 1][x + i] = a1;
                B[y + 2][x + i] = a2;
                B[y + 3][x + i] = a3;
                /* a4..a7 的正确位置在 B 的左下角，先寄存到右上角 */
                B[y + 0][x + i + 4] = a4;
                B[y + 1][x + i + 4] = a5;
                B[y + 2][x + i + 4] = a6;
                B[y + 3][x + i + 4] = a7;
            }

            /* 第 2 步：把寄存值搬到左下角，同时用 A 左下角填上右上角 */
            for (j = 0; j < 4; j++) {
                /* 先读走寄存的 4 个值，马上要被覆盖 */
                a0 = B[y + j][x + 4];
                a1 = B[y + j][x + 5];
                a2 = B[y + j][x + 6];
                a3 = B[y + j][x + 7];
                /* A 的左下角一列，转置后正好是 B 右上角这一行 */
                a4 = A[x + 4][y + j];
                a5 = A[x + 5][y + j];
                a6 = A[x + 6][y + j];
                a7 = A[x + 7][y + j];

                B[y + j][x + 4] = a4;
                B[y + j][x + 5] = a5;
                B[y + j][x + 6] = a6;
                B[y + j][x + 7] = a7;

                B[y + j + 4][x + 0] = a0;
                B[y + j + 4][x + 1] = a1;
                B[y + j + 4][x + 2] = a2;
                B[y + j + 4][x + 3] = a3;
            }

            /* 第 3 步：A 右下角 4x4 -> B 右下角 */
            for (i = 4; i < 8; i++) {
                a4 = A[x + i][y + 4];
                a5 = A[x + i][y + 5];
                a6 = A[x + i][y + 6];
                a7 = A[x + i][y + 7];

                B[y + 4][x + i] = a4;
                B[y + 5][x + i] = a5;
                B[y + 6][x + i] = a6;
                B[y + 7][x + i] = a7;
            }
        }
    }
}

/*
 * trans_blocked - 通用分块，用于 61x67 这种不规整的尺寸。
 *
 * 61 不是 8 的倍数，行与行之间的组号本来就是错开的，天然打散了冲突，
 * 所以不需要 64x64 那套复杂手法，朴素分块就够。
 *
 * 块大小是实测出来的（61x67 的 miss 数）：
 *   BLK   4     8    12    16    17    18    20    22    23    24
 *   miss 2425  2118  2057  1992  1950  1961  2002  1959  1928  2015
 * 取 23 —— 曲线不单调，因为块宽和 61 的取余关系会改变边角块的形状。
 *
 * 局部变量：i, j, ii, jj, tmp = 5 个。
 */
#define BLK 23
static void trans_blocked(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, ii, jj, tmp;

    for (ii = 0; ii < N; ii += BLK) {
        for (jj = 0; jj < M; jj += BLK) {
            for (i = ii; i < ii + BLK && i < N; i++) {
                for (j = jj; j < jj + BLK && j < M; j++) {
                    tmp = A[i][j];
                    B[j][i] = tmp;
                }
            }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

