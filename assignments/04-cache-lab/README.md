# 作业 04：Cache Lab

状态：**已完成**（自动评分 53/53，风格分 7 分需人工评定）

| 部分 | 结果 | 满分条件 |
| --- | --- | --- |
| Part A | 27/27，8 个用例全对 | — |
| Part B 32×32 | 287 misses，8/8 | m < 300 |
| Part B 64×64 | 1227 misses，8/8 | m < 1300 |
| Part B 61×67 | 1928 misses，10/10 | m < 2000 |

`csim -v` 的输出与 `csim-ref` 逐字节一致，已在 6 组参数（含 `-E 3`、`-E 8`、
`-s 6` 等评分范围外的配置）上 `diff` 验证。

这是 CMU CS:APP3e / 15-213 Fall 2015 Cache Lab 的自学版本，
在 f15 课程顺序里紧接 Attack Lab 之后（f15 不含 Y86 架构实验）。

## 目标

围绕高速缓存的组织与局部性展开，分两部分：

- **Part A**：实现一个缓存模拟器 `csim.c`，按 `(s, E, b)` 参数统计
  hit / miss / eviction，行为要和 `csim-ref` 完全一致（LRU 替换）。满分 27 分。
- **Part B**：优化矩阵转置 `trans.c`，在 (s=5, E=1, b=5) 的 1KB 直接映射缓存上，
  让 32×32、64×64、61×67 三种尺寸的 miss 数尽量少。满分 26 分。

先读 `cachelab-zh.md`，遇到细节问题对照 `cachelab.pdf`。

## 材料

`cachelab-handout.tar` 已解包到本目录：

- `csim.c`：**要写的**缓存模拟器（初始几乎为空）。
- `trans.c`：**要写的**转置函数，含示例 `trans` 和待实现的 `transpose_submit`。
- `csim-ref`：参考缓存模拟器（Linux x86-64 二进制）。
- `test-csim`：Part A 的自动评分程序（Linux x86-64 二进制）。
- `test-trans.c`、`tracegen.c`：Part B 的评测程序。
- `cachelab.c`、`cachelab.h`：`printSummary`、`registerTransFunction` 等辅助函数。
- `driver.py`：一次跑完两部分（Python 2 脚本，见下）。
- `traces/`：`test-csim` 用的 5 个 valgrind trace。
- `Makefile`、`README`：官方构建脚本与原始说明。
- `cachelab.pdf`：官方作业说明。
- `cachelab-zh.md`：中文说明，按官方 PDF 整理。

来源与校验（2026-08-10 从 CMU 官方站点下载）：

- <http://csapp.cs.cmu.edu/3e/cachelab.pdf>
- <http://csapp.cs.cmu.edu/3e/cachelab-handout.tar>

```text
2521ccdd867f23d607745a429570d7d863566d28c27711324f8fa879c1b4b8cd  cachelab.pdf
de37f176d21338c65fc5bd5eb163c06df97fda2a3f1d21f577e25247f80ba64b  cachelab-handout.tar
```

## 参考讲义

`lectures/` 下放了 f15 对应的两讲 PDF：

- `11-memory-hierarchy.pdf`：存储器层次结构、局部性。
- `12-cache-memories.pdf`：高速缓存的组织与访问、缓存友好代码。

来源：<https://www.cs.cmu.edu/afs/cs/academic/class/15213-f15/www/lectures/>

## 运行环境

本机是 macOS arm64，跑不了完整评测：

- `make csim` 能编译通过，可以在本地写、编译、手工测 `csim.c`。
- `make` 编译 `test-trans` 失败：`test-trans.c:72` 的 `WEXITSTATUS(system(cmd))`
  在 macOS 头文件下报 `cannot take the address of an rvalue`。
- `csim-ref`、`test-csim` 是 Linux x86-64 ELF，直接执行报 `exec format error`。
- Part B 的评测依赖 `valgrind`。

所以和 Attack Lab 一样，评测放到远端 Linux 机器上做：

```sh
ssh order
cd /root/code/jinduoxia/cachelab
```

远端是 CentOS 8 x86-64，已装 `valgrind 3.17.0`（`dnf install valgrind`），
Part A、Part B 的评测都验证过可以跑。

本地改完代码后同步过去（`--delete` 会清掉远端多余文件，但排除项里的
编译产物不受影响）：

```sh
rsync -a --delete --exclude 'lectures/' --exclude '.DS_Store' \
  --exclude 'csim' --exclude '*.dSYM/' --exclude '.csim_results' \
  --exclude 'trace.f*' --exclude '*-handin.tar' --exclude '*.o' \
  ./ order:/root/code/jinduoxia/cachelab/
```

## 工作流

```sh
make clean && make

# Part A
./csim -s 4 -E 1 -b 4 -t traces/yi.trace   # 对照 ./csim-ref 同参数的输出
./test-csim                                # 8 个用例，满分 27

# Part B
./test-trans -M 32 -N 32
./test-trans -M 64 -N 64
./test-trans -M 61 -N 67

# 用 trace.fi 定位 miss 来源
./csim-ref -v -s 5 -E 1 -b 5 -t trace.f0
```

`driver.py` 是 Python 2 脚本，现在的系统上一般跑不起来，分别运行
`./test-csim` 和 `./test-trans` 即可。

`make` 每次会顺带生成 `${USER}-handin.tar`，连同 `csim`、`test-trans`、
`tracegen`、`trace.f*` 等编译产物一起已在 `.gitignore` 里忽略。
