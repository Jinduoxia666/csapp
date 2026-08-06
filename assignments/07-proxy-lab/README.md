# 作业 07：Proxy Lab

状态：**待做**

这是 CMU CS:APP3e / 15-213 Fall 2015 Proxy Lab 的自学版本，
在 f15 课程顺序里紧接 Malloc Lab 之后，是本课程的最后一个实验。

## 目标

实现一个能缓存网页的并发 HTTP 代理，综合网络编程与并发：

- **Part I**：顺序代理，转发 GET 请求、解析 URL 和请求头。
- **Part II**：并发代理，用多线程同时处理多个连接。
- **Part III**：加入带同步保护的缓存，缓存最近访问过的对象。

## 参考讲义

`lectures/` 下放了 f15 对应的四讲 PDF：

- `21-netprog1.pdf`：网络编程基础、套接字、客户端-服务器模型。
- `22-netprog2.pdf`：HTTP、Web 服务器、动态内容。
- `23-concprog.pdf`：并发编程、基于进程/线程的并发。
- `24-sync-basic.pdf`：线程同步、信号量、互斥。

来源：<https://www.cs.cmu.edu/afs/cs/academic/class/15213-f15/www/lectures/>

## 材料

代码 handout 待获取。官方入口：

- <https://csapp.cs.cmu.edu/3e/labs.html>
- <http://csapp.cs.cmu.edu/3e/proxylab.pdf>
- <http://csapp.cs.cmu.edu/3e/proxylab-handout.tar>

拿到 handout 后再补充本节，并开始做题。
