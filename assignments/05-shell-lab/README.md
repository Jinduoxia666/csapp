# 作业 05：Shell Lab

状态：**待做**

这是 CMU CS:APP3e / 15-213 Fall 2015 Shell Lab（tshlab）的自学版本，
在 f15 课程顺序里紧接 Cache Lab 之后。

## 目标

实现一个带作业控制的简易 shell `tsh.c`，练习异常控制流与进程管理：

- 解析命令行，`fork` + `execve` 运行前台/后台作业。
- 处理 `SIGCHLD`、`SIGINT`、`SIGTSTP` 信号，正确回收子进程、避免竞态。
- 实现内建命令 `quit`、`jobs`、`bg`、`fg` 和作业列表管理。

## 参考讲义

`lectures/` 下放了 f15 对应的两讲 PDF：

- `14-ecf-procs.pdf`：异常控制流、进程、`fork`/`exec`/`wait`。
- `15-ecf-signals.pdf`：信号、信号处理程序、非本地跳转。

来源：<https://www.cs.cmu.edu/afs/cs/academic/class/15213-f15/www/lectures/>

对应的中文笔记（按幻灯片顺序整理，末尾附与本 lab 的对照表）：

- `lectures/14-ecf-procs-zh.md`
- `lectures/15-ecf-signals-zh.md`

## 材料

代码 handout 待获取。官方入口：

- <https://csapp.cs.cmu.edu/3e/labs.html>
- <http://csapp.cs.cmu.edu/3e/shlab.pdf>
- <http://csapp.cs.cmu.edu/3e/shlab-handout.tar>

拿到 handout 后再补充本节，并开始做题。
