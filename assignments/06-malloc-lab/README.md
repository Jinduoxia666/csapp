# 作业 06：Malloc Lab

状态：**待做**

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

代码 handout 待获取。官方入口：

- <https://csapp.cs.cmu.edu/3e/labs.html>
- <http://csapp.cs.cmu.edu/3e/malloclab.pdf>
- <http://csapp.cs.cmu.edu/3e/malloclab-handout.tar>

拿到 handout 后再补充本节，并开始做题。
