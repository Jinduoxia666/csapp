# 作业 06：Malloc Lab

状态：**待做**（handout 已就位，`mm.c` 是官方基线实现）

这是 CMU CS:APP3e / 15-213 Fall 2015 Malloc Lab 的自学版本，
在 f15 课程顺序里紧接 Shell Lab 之后。

## 目标

在 `mm.c` 中实现自己的动态内存分配器，兼顾吞吐率与空间利用率：

- `mm_malloc`、`mm_free`、`mm_realloc`、`mm_init`。
- 设计堆的组织方式：隐式/显式空闲链表、分离空闲链表等。
- 处理块的分割、合并（coalescing）、对齐与边界标记。

## 参考讲义

`lectures/` 下放了 f15 对应的两讲 PDF：

- `19-malloc-basic.pdf`：动态内存分配基础、隐式空闲链表、放置策略。
- `20-malloc-advanced.pdf`：显式/分离空闲链表、垃圾回收、常见错误。

来源：<https://www.cs.cmu.edu/afs/cs/academic/class/15213-f15/www/lectures/>

## 材料

`malloclab-handout.tar` 已解包到本目录，并按用途整理：

```text
06-malloc-lab/
├── README.md
├── Makefile           # 适配当前目录与 64 位环境
├── mm.c               # 唯一需要完成的实验代码
├── mm.h
├── docs/
│   ├── malloclab.pdf  # 官方作业说明
│   └── README         # 官方 handout 说明
├── lectures/          # 两讲动态内存分配课件
├── traces/
│   ├── short1-bal.rep
│   ├── short2-bal.rep
│   └── 11 个默认评测 trace
└── tools/
    ├── mdriver.c      # 正确性、利用率与吞吐率测试驱动
    ├── memlib.c/.h    # 模拟堆与 mem_sbrk
    └── config.h、计时器源码及头文件
```

官方 CS:APP3e handout 只附带两个入门 trace，但其中的 `config.h` 默认引用
CMU 内部 AFS 上的 11 个评测 trace。为便于自学，本目录从 CMU Fall 2010
公开 handout 补入了同名 `*-bal.rep` 文件；驱动、接口和 `mm.c` 仍使用
CS:APP3e 官方 handout 版本。

只有 `mm.c` 是实验实现文件。初始版本只通过 `mem_sbrk` 不断扩展堆，
`mm_free` 不做任何事情，尚未实现空闲块复用与合并。

来源与校验（2026-09-03 从 CMU 官方站点下载）：

- <https://csapp.cs.cmu.edu/3e/labs.html>
- <https://csapp.cs.cmu.edu/3e/malloclab.pdf>
- <https://csapp.cs.cmu.edu/3e/malloclab-handout.tar>
- <https://www.cs.cmu.edu/afs/cs/academic/class/15213-f10/www/labs/malloclab-handout.tar>

```text
e922e0ed09f474c2f402b7223690b5c5ca13feb08c8e0377a0c5bd260338c40b  malloclab.pdf
5f7aaeb98fb90e3b59f44627f5ede3eab1f99cf5047492ed40a489e22a859e2c  malloclab-handout.tar
1ba90774403d00b7184115cfe0affb93d5a72015dd194b78ee9494a2c5a9c251  CMU Fall 2010 malloclab-handout.tar
```

## 构建与测试

官方 Makefile 使用 `-m32`，无法在本机 macOS arm64 上链接；当前 Makefile
移除了该参数，并根据 `docs/`、`tools/`、`traces/` 的目录结构调整了路径。

```sh
make clean && make

# 先跑两个入门 trace
make test

# 跑 11 个默认评测 trace
make test-all

# 单独定位某个 trace
./mdriver -V -f traces/coalescing-bal.rep
```

当前官方基线的 `mm_free` 不回收空间，因此 `make test-all` 会在若干较长
trace 上耗尽 20 MB 模拟堆并报告 `mm_malloc failed`。这是实验尚未实现的
预期结果，不是 trace 缺失或驱动配置错误。

本机可以验证正确性、空间利用率和基本吞吐率，但历史评分阈值来自旧版
Linux x86 环境，因此本机吞吐率不宜直接换算成课程分数。
