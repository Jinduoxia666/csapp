# Cache Lab 中文说明

> 本文是官方 `cachelab.pdf`（CMU 15-213/18-213, Fall 2012）的中文整理版，供自学使用。
> 函数名、命令、参数、输出格式保持原文不变。与英文原文有出入时以 `cachelab.pdf` 为准。
> 原文中与课程相关的内容（上交方式、截止时间、答疑）已略去。

## 1 概述

这个实验帮助你理解高速缓存对 C 程序性能的影响，分两部分：

- **Part A**：写一个约 200–300 行的 C 程序，模拟一个高速缓存的行为。
- **Part B**：优化一个矩阵转置函数，目标是尽量减少缓存不命中数。

必须在 64 位 x86-64 机器上做这个实验。

## 2 材料清单

需要你修改并提交的两个文件：

| 文件 | 说明 |
| --- | --- |
| `csim.c` | 你的缓存模拟器（初始几乎是空的，要从头写） |
| `trans.c` | 你的转置函数 |

评测与辅助工具：

| 文件 | 说明 |
| --- | --- |
| `Makefile` | 构建模拟器和工具 |
| `README` | 官方原始说明 |
| `driver.py` | 驱动程序，依次跑 `test-csim` 和 `test-trans` |
| `cachelab.c` / `cachelab.h` | 必需的辅助函数与头文件 |
| `csim-ref` | 参考缓存模拟器的可执行文件 |
| `test-csim` | 测试你的缓存模拟器 |
| `test-trans.c` | 测试你的转置函数 |
| `tracegen.c` | `test-trans` 使用的辅助程序 |
| `traces/` | `test-csim` 使用的 trace 文件 |

编译：

```sh
make clean
make
```

注意：官方文档提到可以 `#include "contracts.h"`（15-122 的 C0 风格契约），但这份 handout 里并没有该文件，忽略即可。

## 3 参考 trace 文件

`traces/` 下的 trace 文件用于检验 Part A 的正确性。它们由 Linux 程序 `valgrind` 生成，例如：

```sh
valgrind --log-fd=1 --tool=lackey -v --trace-mem=yes ls -l
```

这条命令运行 `ls -l`，按发生顺序捕获它的每一次内存访问并打印到标准输出。

trace 的格式如下：

```text
I 0400d7d4,8
 M 0421c7f0,4
 L 04f6b868,8
 S 7ff0005c8,8
```

每一行表示一到两次内存访问，格式为：

```text
[空格]操作 地址,大小
```

- 操作字段：`I` 表示取指令，`L` 表示数据读，`S` 表示数据写，`M` 表示数据修改（即一次读后紧跟一次对同一地址的写）。
- `I` 前面**没有**空格；`M`、`L`、`S` 前面**总是**有一个空格。
- 地址是 64 位十六进制内存地址。
- 大小是这次操作访问的字节数。

## 4 Part A：写一个缓存模拟器

在 `csim.c` 中写一个缓存模拟器：以一个 valgrind 内存 trace 作为输入，模拟缓存在这个 trace 上的命中/不命中行为，输出命中、不命中、逐出的总数。

handout 提供了参考模拟器的二进制 `csim-ref`，它能模拟任意大小和相联度的缓存，替换策略为 **LRU**（最近最少使用）。

### 4.1 命令行接口

```text
Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>
```

- `-h`：可选，打印帮助信息。
- `-v`：可选，详细模式，显示每次访问的 trace 信息。
- `-s <s>`：组索引位数（组数 S = 2^s）。
- `-E <E>`：相联度（每组的行数）。
- `-b <b>`：块偏移位数（块大小 B = 2^b）。
- `-t <tracefile>`：要重放的 valgrind trace 文件名。

参数记号 (s, E, b) 取自 CS:APP2e 教材第 597 页。示例：

```sh
./csim-ref -s 4 -E 1 -b 4 -t traces/yi.trace
hits:4 misses:5 evictions:3
```

同一个例子的详细模式：

```sh
./csim-ref -v -s 4 -E 1 -b 4 -t traces/yi.trace
L 10,1 miss
M 20,1 miss hit
L 22,1 hit
S 18,1 hit
L 110,1 miss eviction
L 210,1 miss eviction
M 12,1 miss eviction hit
hits:4 misses:5 evictions:3
```

你的任务就是把 `csim.c` 写成接受同样的命令行参数、产生**完全相同**的输出。

### 4.2 Part A 编程规则

- 在 `csim.c` 的头部注释里写上你的名字和 loginID。
- `csim.c` 必须**无警告**编译通过才能得分。
- 模拟器必须对任意的 s、E、b 都正确工作。这意味着数据结构要用 `malloc` 动态分配。
- 本实验只关心数据缓存的性能，因此**忽略所有取指访问**（以 `I` 开头的行）。valgrind 总是把 `I` 放在第一列（前面没有空格），把 `M`、`L`、`S` 放在第二列（前面有一个空格），可以据此解析 trace。
- 要得分，必须在 `main` 函数最后调用 `printSummary`，传入命中、不命中、逐出的总数：

  ```c
  printSummary(hit_count, miss_count, eviction_count);
  ```

- 假设内存访问都是对齐的，即单次访问不会跨越块边界。有了这个假设，trace 里的**大小字段可以忽略**。

### 4.3 Part A 评分（27 分）

用不同的缓存参数和 trace 跑你的模拟器，共 8 个测试用例，每个 3 分，最后一个 6 分：

```sh
./csim -s 1 -E 1 -b 1 -t traces/yi2.trace
./csim -s 4 -E 2 -b 4 -t traces/yi.trace
./csim -s 2 -E 1 -b 4 -t traces/dave.trace
./csim -s 2 -E 1 -b 3 -t traces/trans.trace
./csim -s 2 -E 2 -b 3 -t traces/trans.trace
./csim -s 2 -E 4 -b 3 -t traces/trans.trace
./csim -s 5 -E 1 -b 5 -t traces/trans.trace
./csim -s 5 -E 1 -b 5 -t traces/long.trace
```

可以用 `csim-ref` 得到每个用例的正确答案；调试时加 `-v` 看每次命中/不命中的详细记录。

每个用例中，命中数、不命中数、逐出数各占该用例 1/3 的分数。例如一个 3 分的用例，命中数和不命中数对、逐出数错，得 2 分。

## 5 Part B：优化矩阵转置

在 `trans.c` 中写一个转置函数，使缓存不命中数尽可能少。

设 A 是矩阵，A_ij 是第 i 行第 j 列的元素。A 的转置 A^T 满足 A_ij = A^T_ji。

`trans.c` 里已经给了一个示例函数，把 N×M 的矩阵 A 转置后存入 M×N 的矩阵 B：

```c
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
```

它结果正确，但访问模式导致不命中很多。你的任务是写一个类似的函数 `transpose_submit`，在不同尺寸的矩阵上都让不命中数尽量少：

```c
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N]);
```

**不要修改描述字符串 `"Transpose submission"`**，评分程序靠它来识别要评分的那个函数。

### 5.1 Part B 编程规则

- 在 `trans.c` 的头部注释里写上你的名字和 loginID。
- `trans.c` 必须无警告编译通过才能得分。
- 每个转置函数最多定义 **12 个 `int` 类型的局部变量**。
- 不允许用 `long` 类型或位技巧把多个值塞进一个变量来绕过上一条规则。
- 转置函数**不能用递归**。
- 如果使用辅助函数，从顶层转置函数到各辅助函数，栈上同时存在的局部变量总数不能超过 12 个。例如转置函数声明了 8 个变量，调用的函数用了 4 个，那个函数又调用了用 2 个变量的函数，栈上就有 14 个，违规。
- 转置函数**不能修改数组 A**；对数组 B 可以随意操作。
- **不允许定义任何数组，也不允许使用任何形式的 `malloc`**。

> 局部变量数量受限的原因：评测代码无法统计对栈的引用，所以要求你把栈引用压到最低，把注意力放在源数组和目标数组的访问模式上。

### 5.2 Part B 评分（26 分）

在三种输出矩阵尺寸上评测 `transpose_submit` 的正确性和性能：

- 32 × 32（M = 32, N = 32）
- 64 × 64（M = 64, N = 64）
- 61 × 67（M = 61, N = 67）

评测方式：用 `valgrind` 抽取你的函数的地址 trace，再用参考模拟器在参数为 **(s = 5, E = 1, b = 5)** 的缓存上重放这个 trace。也就是 32 组、直接映射、32 字节块，共 1KB。

每种尺寸的性能分随不命中数 m 线性变化，直到某个阈值：

| 尺寸 | 满分 | 满分条件 | 0 分条件 |
| --- | --- | --- | --- |
| 32 × 32 | 8 分 | m < 300 | m > 600 |
| 64 × 64 | 8 分 | m < 1300 | m > 2000 |
| 61 × 67 | 10 分 | m < 2000 | m > 3000 |

结果必须正确才能拿到该尺寸的性能分。代码只需要对这三种尺寸正确，完全可以显式判断输入尺寸、为每种情况写各自优化的代码。

### 5.3 代码风格（7 分）

7 分由人工评定。课程助教会检查 Part B 中是否有违规的数组和过多的局部变量。

总分为 60 分：Part A 27 分 + Part B 26 分 + 风格 7 分。

## 6 做题流程

### 6.1 Part A 的流程与提示

编译后用自动评分程序 `test-csim` 检查模拟器正确性：

```sh
make
./test-csim
```

输出形如：

```text
                        Your simulator     Reference simulator
Points (s,E,b)    Hits  Misses  Evicts    Hits  Misses  Evicts
     3 (1,1,1)       9       8       6       9       8       6  traces/yi2.trace
     3 (4,2,4)       4       5       2       4       5       2  traces/yi.trace
     3 (2,1,4)       2       3       1       2       3       1  traces/dave.trace
     3 (2,1,3)     167      71      67     167      71      67  traces/trans.trace
     3 (2,2,3)     201      37      29     201      37      29  traces/trans.trace
     3 (2,4,3)     212      26      10     212      26      10  traces/trans.trace
     3 (5,1,5)     231       7       0     231       7       0  traces/trans.trace
     6 (5,1,5)  265189   21775   21743  265189   21775   21743  traces/long.trace
    27
```

每行显示该测试得到的分数、缓存参数、输入 trace 文件，以及你的模拟器与参考模拟器的结果对比。

提示：

- 先用小 trace 调试，比如 `traces/dave.trace`。
- 参考模拟器的 `-v` 参数会输出每次内存访问导致的命中、不命中和逐出。你的 `csim.c` 不要求实现这个功能，但**强烈建议实现**——这样才能逐行对比你的模拟器和参考模拟器的行为。
- 建议用 `getopt` 解析命令行参数，需要这些头文件：

  ```c
  #include <getopt.h>
  #include <stdlib.h>
  #include <unistd.h>
  ```

  详见 `man 3 getopt`。
- 每个数据读（L）或数据写（S）操作最多导致一次不命中。数据修改（M）视为一次读后紧跟一次对同一地址的写，因此 M 可能产生两次命中，或者一次不命中加一次命中（外加可能的一次逐出）。

### 6.2 Part B 的流程与提示

`test-trans.c` 会测试你在评分程序中注册过的每个转置函数的正确性和性能。

`trans.c` 里最多可以注册 100 个版本的转置函数，每个形如：

```c
/* Header comment */
char trans_simple_desc[] = "A simple transpose";
void trans_simple(int M, int N, int A[N][M], int B[M][N])
{
    /* your transpose code here */
}
```

在 `trans.c` 的 `registerFunctions` 例程里注册：

```c
registerTransFunction(trans_simple, trans_simple_desc);
```

运行时评分程序会评测每个注册过的函数并打印结果。其中必须有一个是你要提交评分的 `transpose_submit`：

```c
registerTransFunction(transpose_submit, transpose_submit_desc);
```

评分程序以矩阵尺寸为输入，用 `valgrind` 为每个注册的转置函数生成 trace，再用参考模拟器在 (s = 5, E = 1, b = 5) 的缓存上评测这些 trace。例如在 32 × 32 矩阵上测试：

```sh
make
./test-trans -M 32 -N 32
```

```text
Step 1: Evaluating registered transpose funcs for correctness:
func 0 (Transpose submission): correctness: 1
func 1 (Simple row-wise scan transpose): correctness: 1
func 2 (column-wise scan transpose): correctness: 1
func 3 (using a zig-zag access pattern): correctness: 1

Step 2: Generating memory traces for registered transpose funcs.

Step 3: Evaluating performance of registered transpose funcs (s=5, E=1, b=5)
func 0 (Transpose submission): hits:1766, misses:287, evictions:255
func 1 (Simple row-wise scan transpose): hits:870, misses:1183, evictions:1151
func 2 (column-wise scan transpose): hits:870, misses:1183, evictions:1151
func 3 (using a zig-zag access pattern): hits:1076, misses:977, evictions:945

Summary for official submission (func 0): correctness=1 misses=287
```

三种尺寸分别测：

```sh
./test-trans -M 32 -N 32
./test-trans -M 64 -N 64
./test-trans -M 61 -N 67
```

提示：

- `test-trans` 会把第 i 个函数的 trace 存到 `trace.fi`。这些 trace 是极有价值的调试工具，能让你精确看到命中和不命中来自哪里。把某个 trace 用详细模式喂给参考模拟器即可：

  ```sh
  ./csim-ref -v -s 5 -E 1 -b 5 -t trace.f0
  S 68312c,1 miss
  L 683140,8 miss
  L 683124,4 hit
  L 683120,4 hit
  L 603124,4 miss eviction
  S 6431a0,4 miss
  ...
  ```

  （因为 valgrind 会引入大量与你代码无关的栈访问，trace 里的栈访问已被过滤掉。这也是禁用局部数组、限制局部变量个数的原因。）
- 评测用的是**直接映射**缓存，冲突不命中是主要问题。想清楚你的代码在哪里会产生冲突不命中，**尤其是对角线上**，然后设法减少。
- **分块（blocking）** 是减少缓存不命中的有效手段，参考：
  <http://csapp.cs.cmu.edu/public/waside/waside-blocking.pdf>

### 6.3 一次跑完全部测试

```sh
./driver.py
```

`driver.py` 依次运行 `test-csim` 和 `test-trans`。注意它是 Python 2 脚本（首行为 `#!/usr//bin/python`），现在的系统上多半没有 `python2`，直接分别运行 `./test-csim` 和 `./test-trans` 即可。

## 7 运行环境

`csim-ref` 和 `test-csim` 是 Linux x86-64 的 ELF 可执行文件，`test-trans` 依赖 `valgrind`，因此实验必须在 64 位 x86-64 Linux 上做。本机是 macOS arm64，实测：

- `make csim` 可以编译通过，能在本地写和编译 `csim.c`。
- `make` 编译 `test-trans` 会失败（macOS 的 `WEXITSTATUS` 宏在 `test-trans.c:72` 上报错）。
- `./csim-ref` 直接报 `exec format error`。

所以和 Attack Lab 一样，把目录同步到远端 Linux 机器上跑测试。
