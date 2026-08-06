# 作业 04：Cache Lab

状态：**待做**

这是 CMU CS:APP3e / 15-213 Fall 2015 Cache Lab 的自学版本，
在 f15 课程顺序里紧接 Attack Lab 之后（f15 不含 Y86 架构实验）。

## 目标

围绕高速缓存的组织与局部性展开，分两部分：

- **Part A**：实现一个缓存模拟器 `csim.c`，按 `(s, E, b)` 参数统计
  hit / miss / eviction。
- **Part B**：优化矩阵转置 `trans.c`，通过分块等手段尽量减少缓存未命中数。

## 参考讲义

`lectures/` 下放了 f15 对应的两讲 PDF：

- `11-memory-hierarchy.pdf`：存储器层次结构、局部性。
- `12-cache-memories.pdf`：高速缓存的组织与访问、缓存友好代码。

来源：<https://www.cs.cmu.edu/afs/cs/academic/class/15213-f15/www/lectures/>

## 材料

代码 handout 待获取。官方入口：

- <https://csapp.cs.cmu.edu/3e/labs.html>
- <http://csapp.cs.cmu.edu/3e/cachelab.pdf>
- <http://csapp.cs.cmu.edu/3e/cachelab-handout.tar>

拿到 handout 后再补充本节，并开始做题。
