# 第 15 讲：异常控制流 —— 信号与非本地跳转（中文版）

> 原讲义：`15-ecf-signals.pdf`
> CMU 15-213 *Introduction to Computer Systems*，Fall 2015 第 15 讲（2015-10-20）
> 讲师：Randal E. Bryant、David R. O'Hallaron
> 教材：CS:APP 第三版，对应第 8 章 8.5–8.6
>
> 本文是按幻灯片顺序整理的中文笔记，供 Shell Lab 复习使用；图示改用文字描述，术语保留英文原词。

## 目录

1. [Shell 程序](#1-shell-程序)
2. [信号（Signals）](#2-信号signals)
3. [编写安全的信号处理程序](#3-编写安全的信号处理程序)
4. [同步流以避免竞态](#4-同步流以避免竞态)
5. [显式等待信号](#5-显式等待信号)
6. [非本地跳转（补充幻灯片）](#6-非本地跳转补充幻灯片)
7. [小结](#7-小结)

---

## 系统各层次上的 ECF

| 机制 | 实现层 | 讲授位置 |
|------|--------|----------|
| **异常（Exceptions）** | 硬件 + 操作系统内核软件 | 上一讲 |
| **进程上下文切换** | 硬件定时器 + 内核软件 | 上一讲 |
| **信号（Signals）** | 内核软件 + 应用软件 | **本讲** |
| **非本地跳转** | 应用代码 | 教材 + 补充幻灯片 |

---

## 1. Shell 程序

### Linux 进程层次结构

```
                     [0]
                      │
                  init [1]
        ┌─────────────┼─────────────┐
     Daemon      Login shell    Login shell   …
   (e.g. httpd)       │              │
                    Child          Child
                      │              │
                 Grandchild     Grandchild
```

> 提示：可以用 Linux 的 `pstree` 命令查看这个层次结构。

### Shell 程序

**shell 是一个代表用户运行程序的应用程序。**

- `sh` —— 最初的 Unix shell（Stephen Bourne，AT&T 贝尔实验室，1977）
- `csh` / `tcsh` —— BSD Unix C shell
- `bash` —— "Bourne-Again" Shell（Linux 默认 shell）

执行过程就是一连串的 **读取 / 求值（read / evaluate）** 步骤：

```c
int main()
{
    char cmdline[MAXLINE];  /* 命令行 */

    while (1) {
        /* 读取 */
        printf("> ");
        Fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin))
            exit(0);

        /* 求值 */
        eval(cmdline);
    }
}
```

### 简易 shell 的 eval 函数

```c
void eval(char *cmdline)
{
    char *argv[MAXARGS];  /* execve() 的参数列表 */
    char buf[MAXLINE];    /* 存放修改后的命令行 */
    int bg;               /* 作业应该在后台还是前台运行？ */
    pid_t pid;            /* 进程 id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;   /* 忽略空行 */

    if (!builtin_command(argv)) {
        if ((pid = Fork()) == 0) {   /* 子进程运行用户作业 */
            if (execve(argv[0], argv, environ) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }

        /* 父进程等待前台作业终止 */
        if (!bg) {
            int status;
            if (waitpid(pid, &status, 0) < 0)
                unix_error("waitfg: waitpid error");
        }
        else
            printf("%d %s", pid, cmdline);
    }
    return;
}
```

### 这个简易 shell 的问题

- 它能正确地**等待并回收前台作业**
- 但**后台作业**怎么办？
  - 后台作业终止时会变成**僵尸**
  - 由于 shell（通常）不会终止，这些僵尸**永远不会被回收**
  - 会造成**内存泄漏**，最终可能耗尽内核内存

### ECF 来救场

**解决方案：异常控制流**

- 内核会打断常规处理流程，在后台进程结束时**提醒**我们
- 在 Unix 中，这个提醒机制就叫做**信号（signal）**

---

## 2. 信号（Signals）

### 什么是信号

**信号是一条小消息，用于通知进程系统中发生了某种类型的事件。**

- 类似于异常和中断
- 由内核发送给进程（有时是应另一个进程的请求）
- 信号类型由小整数 ID 标识（1–30）
- 信号中携带的**全部信息**就是它的 ID 和"它到达了"这件事本身

常见信号：

| ID | 名称 | 默认动作 | 对应事件 |
|----|------|----------|----------|
| 2 | `SIGINT` | 终止 | 用户按下 ctrl-c |
| 9 | `SIGKILL` | 终止 | 杀死程序（**不能被覆盖或忽略**） |
| 11 | `SIGSEGV` | 终止并转储（dump core） | 段违例（segmentation violation） |
| 14 | `SIGALRM` | 终止 | 定时器信号 |
| 17 | `SIGCHLD` | 忽略 | 子进程停止或终止 |

### 信号概念：发送信号

内核通过**更新目标进程上下文中的某些状态**，向目标进程**发送（递送，deliver）**一个信号。

内核发送信号的原因有两种：

1. 内核检测到了某个系统事件，例如除零（`SIGFPE`）或子进程终止（`SIGCHLD`）
2. 另一个进程调用了 `kill` 系统调用，显式请求内核向目标进程发送信号

### 信号概念：接收信号

当目标进程被内核**强制以某种方式对信号的递送作出反应**时，就称它**接收（receive）**了这个信号。

可能的反应方式：

- **忽略**该信号（什么也不做）
- **终止**进程（可选地转储 core）
- 通过执行一个名为**信号处理程序（signal handler）** 的用户级函数来**捕获（catch）** 该信号
  - 类似于响应异步中断而调用硬件异常处理程序

```
(1) 进程接收信号                  (2) 控制转移到信号处理程序
        │
      I_curr  ──────────────────→  (3) 信号处理程序运行
      I_next  ←──────────────────  (4) 处理程序返回到下一条指令
```

### 信号概念：待处理（Pending）与阻塞（Blocked）信号

- 信号**已发送但尚未被接收**时，称它是 **pending（待处理）** 的
  - 对于任何**特定类型**的信号，最多只能有**一个** pending 信号
  - **重要：信号不排队（not queued）**
    - 如果一个进程有类型为 k 的 pending 信号，那么后续发送给该进程的类型 k 的信号会被**丢弃**
- 进程可以**阻塞（block）** 某些信号的接收
  - 被阻塞的信号可以被递送，但在解除阻塞之前不会被接收
- **一个 pending 信号最多被接收一次**

### 信号概念：pending / blocked 位向量

内核在每个进程的上下文中维护 **pending** 和 **blocked** 两个位向量：

- **`pending`**：表示 pending 信号的集合
  - 当类型 k 的信号被递送时，内核**设置** `pending` 中的第 k 位
  - 当类型 k 的信号被接收时，内核**清除** `pending` 中的第 k 位
- **`blocked`**：表示被阻塞信号的集合
  - 可用 `sigprocmask` 函数设置和清除
  - 也称为**信号掩码（signal mask）**

### 发送信号：进程组（Process Groups）

**每个进程恰好属于一个进程组。**

```
                    pid=10
                   pgid=10   Shell

  pid=20                        pid=32              pid=40
 pgid=20   前台作业             pgid=32  后台作业#1  pgid=40  后台作业#2
    │                        （后台进程组 32）    （后台进程组 40）
 ┌──┴──┐
Child  Child
pid=21 pid=22
pgid=20 pgid=20
（前台进程组 20）
```

- `getpgrp()` —— 返回当前进程的进程组
- `setpgid()` —— 改变某个进程的进程组（细节见教材）

### 用 /bin/kill 程序发送信号

`/bin/kill` 程序可以向**一个进程**或**一个进程组**发送任意信号。

例子：

- `/bin/kill -9 24818` —— 向进程 24818 发送 `SIGKILL`
- `/bin/kill -9 -24817` —— 向**进程组 24817 中的每一个进程**发送 `SIGKILL`（注意 pid 前的负号）

```
linux> ./forks 16
Child1: pid=24818 pgrp=24817
Child2: pid=24819 pgrp=24817

linux> ps
  PID TTY          TIME CMD
24788 pts/2    00:00:00 tcsh
24818 pts/2    00:00:02 forks
24819 pts/2    00:00:02 forks
24820 pts/2    00:00:00 ps
linux> /bin/kill -9 -24817
linux> ps
  PID TTY          TIME CMD
24788 pts/2    00:00:00 tcsh
24823 pts/2    00:00:00 ps
```

### 从键盘发送信号

键入 **ctrl-c**（**ctrl-z**）会让内核向**前台进程组中的每一个作业**发送 `SIGINT`（`SIGTSTP`）。

- `SIGINT` —— 默认动作是终止每个进程
- `SIGTSTP` —— 默认动作是停止（挂起）每个进程

### ctrl-c 和 ctrl-z 的例子

```
bluefish> ./forks 17
Child: pid=28108 pgrp=28107
Parent: pid=28107 pgrp=28107
<按下 ctrl-z>
Suspended
bluefish> ps w
  PID TTY      STAT   TIME COMMAND
27699 pts/8    Ss     0:00 -tcsh
28107 pts/8    T      0:01 ./forks 17
28108 pts/8    T      0:01 ./forks 17
28109 pts/8    R+     0:00 ps w
bluefish> fg
./forks 17
<按下 ctrl-c>
bluefish> ps w
  PID TTY      STAT   TIME COMMAND
27699 pts/8    Ss     0:00 -tcsh
28110 pts/8    R+     0:00 ps w
```

**STAT（进程状态）图例：**

第一个字母：

- `S` —— sleeping（睡眠）
- `T` —— stopped（已停止）
- `R` —— running（运行）

第二个字母：

- `s` —— session leader（会话首进程）
- `+` —— foreground proc group（前台进程组）

更多细节见 `man ps`。

### 用 kill 函数发送信号

```c
void fork12()
{
    pid_t pid[N];
    int i;
    int child_status;

    for (i = 0; i < N; i++)
        if ((pid[i] = fork()) == 0) {
            /* 子进程：无限循环 */
            while(1)
                ;
        }

    for (i = 0; i < N; i++) {
        printf("Killing process %d\n", pid[i]);
        kill(pid[i], SIGINT);
    }

    for (i = 0; i < N; i++) {
        pid_t wpid = wait(&child_status);
        if (WIFEXITED(child_status))
            printf("Child %d terminated with exit status %d\n",
                   wpid, WEXITSTATUS(child_status));
        else
            printf("Child %d terminated abnormally\n", wpid);
    }
}
```

### 接收信号

设想内核正要从某个异常处理程序返回，准备把控制权交给进程 p。

> **重要：所有的上下文切换都是通过调用某个异常处理程序发起的。**

内核的做法：

1. 计算 **`pnb = pending & ~blocked`**
   - 即进程 p 的**未被阻塞的 pending 信号集合**
2. 如果 `pnb == 0`
   - 把控制权交给 p 的逻辑流中的下一条指令
3. 否则
   - 选择 `pnb` 中**最小的非零位 k**，强制进程 p 接收信号 k
   - 信号的接收会触发 p 的某个**动作**
   - 对 `pnb` 中所有非零的 k 重复此过程
   - 最后把控制权交给 p 的逻辑流中的下一条指令

### 默认动作

每种信号类型都有一个预定义的**默认动作**，为下列之一：

- 进程终止
- 进程终止并转储 core
- 进程停止，直到被 `SIGCONT` 信号重启
- 进程忽略该信号

### 安装信号处理程序

`signal` 函数修改与接收信号 `signum` 相关联的默认动作：

```c
handler_t *signal(int signum, handler_t *handler)
```

`handler` 的不同取值：

- **`SIG_IGN`** —— 忽略 `signum` 类型的信号
- **`SIG_DFL`** —— 恢复为接收 `signum` 类型信号时的默认动作
- 否则，`handler` 是一个用户级信号处理程序的地址
  - 当进程接收到 `signum` 类型的信号时被调用
  - 这一过程称为**安装（installing）** 处理程序
  - 执行处理程序称为**捕获（catching）** 或**处理（handling）** 信号
  - 当处理程序执行 return 语句时，控制权回到进程控制流中被信号中断的那条指令

### 信号处理示例

```c
void sigint_handler(int sig) /* SIGINT 处理程序 */
{
    printf("So you think you can stop the bomb with ctrl-c, do you?\n");
    sleep(2);
    printf("Well...");
    fflush(stdout);
    sleep(1);
    printf("OK. :-)\n");
    exit(0);
}

int main()
{
    /* 安装 SIGINT 处理程序 */
    if (signal(SIGINT, sigint_handler) == SIG_ERR)
        unix_error("signal error");

    /* 等待接收一个信号 */
    pause();

    return 0;
}
```

（注意：这个例子在 handler 里用了 `printf`/`exit`，**并不安全** —— 后面会给出安全版本。）

### 信号处理程序是并发的流

**信号处理程序是一条独立的逻辑流（不是进程），它与主程序并发运行。**

```
进程 A            进程 A         进程 B
while (1)         handler(){
    ;                  …
                  }
     ← 时间 →
```

**另一种视角** —— 处理程序在内核返回用户态时被调度执行：

```
进程 A                                      进程 B
信号递送给 A →  I_curr    用户代码 (main)
                          内核代码      ──上下文切换──→
                                                        …
                          用户代码 (main)  ←──上下文切换──
信号被 A 接收 →           内核代码
                          用户代码 (handler)   ← 处理程序在这里运行
                          内核代码
                I_next    用户代码 (main)
```

### 嵌套的信号处理程序

**处理程序可以被其他处理程序中断。**

```
主程序              处理程序 S             处理程序 T
 I_curr  ─(1)捕获信号 s→
         ─(2)控制转移到 S→
                    ─(3)捕获信号 t→
                    ─(4)控制转移到 T→
                                        (5) 处理程序 T 返回到 S
                    (6) 处理程序 S 返回到主程序
 I_next  ←(7)主程序恢复
```

### 阻塞与解除阻塞信号

**隐式阻塞机制**

- 内核会阻塞**当前正在被处理的那种类型**的所有 pending 信号
- 例如：一个 `SIGINT` 处理程序不会被另一个 `SIGINT` 打断

**显式阻塞与解除阻塞机制**

- `sigprocmask` 函数

**辅助函数**

- `sigemptyset` —— 创建空集合
- `sigfillset` —— 把每一个信号号加入集合
- `sigaddset` —— 把一个信号号加入集合
- `sigdelset` —— 从集合中删除一个信号号

### 临时阻塞信号

```c
sigset_t mask, prev_mask;

Sigemptyset(&mask);
Sigaddset(&mask, SIGINT);

/* 阻塞 SIGINT，并保存之前的阻塞集合 */
Sigprocmask(SIG_BLOCK, &mask, &prev_mask);

    /* 不会被 SIGINT 中断的代码区域 */

/* 恢复之前的阻塞集合，解除对 SIGINT 的阻塞 */
Sigprocmask(SIG_SETMASK, &prev_mask, NULL);
```

---

## 3. 编写安全的信号处理程序

处理程序之所以棘手，是因为它们**与主程序并发**，并且**共享同样的全局数据结构** —— 共享数据结构可能被破坏。

并发问题会在学期后面深入讨论；现在先给出一些指导原则来避免麻烦。

### 编写安全处理程序的准则

- **G0：让你的处理程序尽可能简单**
  - 例如：设置一个全局标志然后返回
- **G1：在处理程序中只调用 async-signal-safe 的函数**
  - `printf`、`sprintf`、`malloc`、`exit` **都不安全！**
- **G2：进入和退出时保存并恢复 `errno`**
  - 以免其他处理程序覆盖了你的 `errno` 值
- **G3：通过临时阻塞所有信号来保护对共享数据结构的访问**
  - 防止可能的数据破坏
- **G4：把全局变量声明为 `volatile`**
  - 防止编译器把它们存放在寄存器里
- **G5：把全局标志声明为 `volatile sig_atomic_t`**
  - 标志（flag）：只被读或只被写的变量（例如 `flag = 1`，而不是 `flag++`）
  - 这样声明的标志不需要像其他全局变量那样加保护

### 异步信号安全（Async-Signal-Safety）

一个函数是 **async-signal-safe** 的，当且仅当它满足下列之一：

- **可重入（reentrant）** —— 例如所有变量都存放在栈帧中（CS:APP3e 12.7.2）
- 或者**不可被信号中断**

POSIX 保证有 **117 个函数**是 async-signal-safe 的（来源：`man 7 signal`）。

- 名单上常用的函数：`_exit`、`write`、`wait`、`waitpid`、`sleep`、`kill`
- 名单上**没有**的常用函数：`printf`、`sprintf`、`malloc`、`exit`
- **不幸的事实：`write` 是唯一 async-signal-safe 的输出函数**

### 安全地产生格式化输出

在处理程序中使用 `csapp.c` 提供的可重入 **SIO（Safe I/O）库**：

```c
ssize_t sio_puts(char s[])  /* 输出字符串 */
ssize_t sio_putl(long v)    /* 输出 long */
void    sio_error(char s[]) /* 输出消息并退出 */
```

安全版本的 SIGINT 处理程序：

```c
void sigint_handler(int sig) /* 安全的 SIGINT 处理程序 */
{
    Sio_puts("So you think you can stop the bomb with ctrl-c, do you?\n");
    sleep(2);
    Sio_puts("Well...");
    sleep(1);
    Sio_puts("OK. :-)\n");
    _exit(0);
}
```

### 正确的信号处理（一）：信号不排队

```c
int ccount = 0;

void child_handler(int sig) {
    int olderrno = errno;
    pid_t pid;
    if ((pid = wait(NULL)) < 0)
        Sio_error("wait error");
    ccount--;
    Sio_puts("Handler reaped child ");
    Sio_putl((long)pid);
    Sio_puts(" \n");
    sleep(1);
    errno = olderrno;
}

void fork14() {
    pid_t pid[N];
    int i;
    ccount = N;
    Signal(SIGCHLD, child_handler);

    for (i = 0; i < N; i++) {
        if ((pid[i] = Fork()) == 0) {
            Sleep(1);
            exit(0);  /* 子进程退出 */
        }
    }
    while (ccount > 0)  /* 父进程自旋 */
        ;
}
```

运行结果（N=5，却只回收了 2 个）：

```
whaleshark> ./forks 14
Handler reaped child 23240
Handler reaped child 23241
（然后卡住）
```

**原因：pending 信号不排队**

- 对每种信号类型，只有**一个 bit** 表示该信号是否 pending……
- ……因此任何特定类型的 pending 信号**最多只有一个**
- **不能用信号来计数事件**，例如子进程终止的个数

### 正确的信号处理（二）：必须回收所有子进程

**把 `wait` 放进循环里**，回收所有已终止的子进程：

```c
void child_handler2(int sig)
{
    int olderrno = errno;
    pid_t pid;
    while ((pid = wait(NULL)) > 0) {
        ccount--;
        Sio_puts("Handler reaped child ");
        Sio_putl((long)pid);
        Sio_puts(" \n");
    }
    if (errno != ECHILD)
        Sio_error("wait error");
    errno = olderrno;
}
```

```
whaleshark> ./forks 15
Handler reaped child 23246
Handler reaped child 23247
Handler reaped child 23248
Handler reaped child 23249
Handler reaped child 23250
whaleshark>
```

### 可移植的信号处理

**麻烦在于：不同版本的 Unix 可能有不同的信号处理语义**

- 有些老系统在捕获信号后会把动作**恢复为默认**
- 有些被中断的系统调用会带着 `errno == EINTR` 返回
- 有些系统**不会**阻塞正在被处理的那种类型的信号

**解决方案：`sigaction`** —— 课程把它封装成 `Signal`：

```c
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* 阻塞正在处理的这类信号 */
    action.sa_flags = SA_RESTART; /* 尽可能重启系统调用 */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}
```

---

## 4. 同步流以避免竞态

### 有竞态的版本

下面这个简易 shell 有一个**微妙的同步错误**，因为它**假定父进程先于子进程运行**：

```c
int main(int argc, char **argv)
{
    int pid;
    sigset_t mask_all, prev_all;

    Sigfillset(&mask_all);
    Signal(SIGCHLD, handler);
    initjobs();  /* 初始化作业列表 */

    while (1) {
        if ((pid = Fork()) == 0) {  /* 子进程 */
            Execve("/bin/date", argv, NULL);
        }
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);  /* 父进程 */
        addjob(pid);   /* 把子进程加入作业列表 */
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    exit(0);
}
```

对应的 SIGCHLD 处理程序：

```c
void handler(int sig)
{
    int olderrno = errno;
    sigset_t mask_all, prev_all;
    pid_t pid;

    Sigfillset(&mask_all);
    while ((pid = waitpid(-1, NULL, 0)) > 0) { /* 回收子进程 */
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        deletejob(pid);  /* 从作业列表中删除该子进程 */
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    if (errno != ECHILD)
        Sio_error("waitpid error");
    errno = olderrno;
}
```

**竞态在哪里？** 如果子进程在父进程执行 `addjob(pid)` **之前**就结束了，`SIGCHLD` 会先到达，handler 里的 `deletejob(pid)` 会去删一个**还没被加入**的作业 —— 之后 `addjob` 才把它加进去，于是这个作业**永远留在作业列表里**。

### 修正后的无竞态版本

关键：**在 `fork` 之前阻塞 `SIGCHLD`，在 `addjob` 之后再解除阻塞**；同时子进程要**恢复**自己的信号掩码（因为子进程继承父进程的掩码）。

```c
int main(int argc, char **argv)
{
    int pid;
    sigset_t mask_all, mask_one, prev_one;

    Sigfillset(&mask_all);
    Sigemptyset(&mask_one);
    Sigaddset(&mask_one, SIGCHLD);
    Signal(SIGCHLD, handler);
    initjobs();  /* 初始化作业列表 */

    while (1) {
        Sigprocmask(SIG_BLOCK, &mask_one, &prev_one);  /* 阻塞 SIGCHLD */
        if ((pid = Fork()) == 0) {  /* 子进程 */
            Sigprocmask(SIG_SETMASK, &prev_one, NULL); /* 解除阻塞 SIGCHLD */
            Execve("/bin/date", argv, NULL);
        }
        Sigprocmask(SIG_BLOCK, &mask_all, NULL);       /* 父进程 */
        addjob(pid);   /* 把子进程加入作业列表 */
        Sigprocmask(SIG_SETMASK, &prev_one, NULL);     /* 解除阻塞 SIGCHLD */
    }
    exit(0);
}
```

---

## 5. 显式等待信号

### 场景

有时程序需要**显式等待某个信号到达** —— 例如 shell 等待前台作业终止。

处理程序：

```c
volatile sig_atomic_t pid;

void sigchld_handler(int s)
{
    int olderrno = errno;
    pid = Waitpid(-1, NULL, 0);  /* main 在等待一个非零的 pid */
    errno = olderrno;
}

void sigint_handler(int s)
{
}
```

主程序（**类似 shell 等待前台作业终止**）：

```c
int main(int argc, char **argv) {
    sigset_t mask, prev;
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGCHLD);

    while (1) {
        Sigprocmask(SIG_BLOCK, &mask, &prev);   /* 阻塞 SIGCHLD */
        if (Fork() == 0)   /* 子进程 */
            exit(0);
        /* 父进程 */
        pid = 0;
        Sigprocmask(SIG_SETMASK, &prev, NULL);  /* 解除阻塞 SIGCHLD */

        /* 等待 SIGCHLD 被接收（很浪费！） */
        while (!pid)
            ;
        /* 收到 SIGCHLD 之后做点事情 */
        printf(".");
    }
    exit(0);
}
```

### 忙等待的几种替代方案都不好

程序是**正确的，但非常浪费 CPU**。其他选项：

```c
while (!pid)      /* 有竞态！ */
    pause();
```

竞态：如果信号在检查 `!pid` 之后、`pause()` 之前到达，`pause()` 会永远睡下去。

```c
while (!pid)      /* 太慢！ */
    sleep(1);
```

**解决方案：`sigsuspend`**

### 用 sigsuspend 等待信号

```c
int sigsuspend(const sigset_t *mask)
```

它等价于下面三步的**原子（不可中断）版本**：

```c
sigprocmask(SIG_BLOCK, &mask, &prev);
pause();
sigprocmask(SIG_SETMASK, &prev, NULL);
```

（注意：`sigsuspend` 的参数是**等待期间要生效的掩码**，返回时恢复原掩码。）

```c
int main(int argc, char **argv) {
    sigset_t mask, prev;
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);
    Sigemptyset(&mask);
    Sigaddset(&mask, SIGCHLD);

    while (1) {
        Sigprocmask(SIG_BLOCK, &mask, &prev);  /* 阻塞 SIGCHLD */
        if (Fork() == 0)  /* 子进程 */
            exit(0);

        /* 等待 SIGCHLD 被接收 */
        pid = 0;
        while (!pid)
            Sigsuspend(&prev);

        /* 可选地解除对 SIGCHLD 的阻塞 */
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        /* 收到 SIGCHLD 之后做点事情 */
        printf(".");
    }
    exit(0);
}
```

---

## 6. 非本地跳转（补充幻灯片）

> 课上这部分让大家参考教材和补充幻灯片。

### setjmp / longjmp

这是一种**强大（但危险）的用户级机制**，可以把控制转移到任意位置：

- 一种**受控地打破过程调用/返回规程**的方式
- 对**错误恢复**和**信号处理**很有用

```c
int setjmp(jmp_buf j)
```

- 必须在 `longjmp` 之前调用
- 标识一个供后续 `longjmp` 使用的**返回点**
- **调用一次，返回一次或多次**
- 实现：把当前的寄存器上下文、栈指针和 PC 值存进 `jmp_buf`，记住"你在哪儿"；然后返回 0

```c
void longjmp(jmp_buf j, int i)
```

- 含义：从 jump buffer `j` 所记住的那个 `setjmp` **再返回一次**……这次返回值是 `i` 而不是 0
- 在 `setjmp` **之后**调用
- **调用一次，永不返回**
- 实现：
  - 从 jump buffer `j` 恢复寄存器上下文（栈指针、基址指针、PC 值）
  - 把 `%eax`（返回值）设为 `i`
  - 跳转到 `j` 中存放的 PC 所指示的位置

### setjmp / longjmp 示例

**目标：从深层嵌套的函数直接返回到最初的调用者。**

```c
/* 深层嵌套的函数 foo */
void foo(void)
{
    if (error1)
        longjmp(buf, 1);
    bar();
}

void bar(void)
{
    if (error2)
        longjmp(buf, 2);
}
```

```c
jmp_buf buf;

int error1 = 0;
int error2 = 1;

void foo(void), bar(void);

int main()
{
    switch(setjmp(buf)) {
    case 0:
        foo();
        break;
    case 1:
        printf("Detected an error1 condition in foo\n");
        break;
    case 2:
        printf("Detected an error2 condition in foo\n");
        break;
    default:
        printf("Unknown error condition in foo\n");
    }
    exit(0);
}
```

### 非本地跳转的局限（一）

**必须遵守栈规程（stack discipline）** —— 只能 long jump 到一个**已被调用但尚未完成**的函数的环境中。

```c
jmp_buf env;

P1()
{
    if (setjmp(env)) {
        /* Long Jump 到这里 */
    } else {
        P2();
    }
}

P2()
{ . . . P2(); . . . P3(); }

P3()
{
    longjmp(env, 1);
}
```

```
longjmp 之前        longjmp 之后
 P1  ← env            P1  ← env
 P2
 P2                （P2/P2/P3 的栈帧被丢弃）
 P3
```

这是**合法**的：`env` 所属的 P1 仍在栈上。

### 非本地跳转的局限（二）

如果 `setjmp` 所在的函数**已经返回**，再 `longjmp` 到它就是**非法**的：

```c
jmp_buf env;

P1()
{
    P2(); P3();
}

P2()
{
    if (setjmp(env)) {
        /* Long Jump 到这里 */
    }
}

P3()
{
    longjmp(env, 1);
}
```

```
setjmp 时:      P2 返回后:       longjmp 时:
 P1              P1               P1
 P2 ← env        ✗（env 悬空）    P3     env ✗ 指向已失效的栈帧
```

`P2` 返回后它的栈帧已经失效，`env` 变成悬空引用 —— 行为未定义。

### 综合应用：一个被 ctrl-c 后自我重启的程序

```c
#include "csapp.h"

sigjmp_buf buf;

void handler(int sig)
{
    siglongjmp(buf, 1);
}

int main()
{
    if (!sigsetjmp(buf, 1)) {
        Signal(SIGINT, handler);
        Sio_puts("starting\n");
    }
    else
        Sio_puts("restarting\n");

    while(1) {
        Sleep(1);
        Sio_puts("processing...\n");
    }
    exit(0); /* 控制永远到不了这里 */
}
```

```
greatwhite> ./restart
starting
processing...
processing...
processing...
restarting          ← Ctrl-c
processing...
processing...
restarting          ← Ctrl-c
processing...
processing...
processing...
```

> 注意用的是 `sigsetjmp` / `siglongjmp`（而不是 `setjmp` / `longjmp`）—— 它们额外保存/恢复信号掩码，这是从信号处理程序中跳出所必需的。

---

## 7. 小结

**信号提供了进程级别的异常处理**

- 可以从用户程序产生
- 可以通过声明信号处理程序来定义其效果
- **编写信号处理程序时要非常小心**

**非本地跳转提供了进程内部的异常控制流**

- 但受限于栈规程

---

## 与 Shell Lab 的关联

这一讲基本就是 tshlab 的实现说明书：

| 讲义内容 | 在 tsh.c 中的用途 |
|----------|-------------------|
| `eval()` 骨架、`parseline`、`builtin_command` | 直接对应 tsh.c 要补全的 `eval` 与 `builtin_cmd` |
| 后台作业变僵尸的问题 | 为什么必须写 `sigchld_handler` |
| `SIGINT` / `SIGTSTP` / `SIGCHLD` | 三个要实现的 handler |
| 进程组、`setpgid` | 子进程必须 `setpgid(0, 0)`，否则 ctrl-c 会连 shell 一起杀掉 |
| `kill(-pid, sig)` 发给整个进程组 | `do_bgfg` 与 `sigint_handler` 中转发信号的正确写法 |
| **信号不排队** → `waitpid` 要放在 `while` 循环里 | `sigchld_handler` 必须用 `while (waitpid(-1, &status, WNOHANG\|WUNTRACED) > 0)` |
| `addjob`/`deletejob` 竞态与 `sigprocmask` | fork 前阻塞 SIGCHLD、addjob 后解除，子进程中恢复掩码 |
| `sigsuspend` | `waitfg` 的正确实现（不要忙等，也不要裸 `pause`） |
| G0–G5 安全准则、SIO 库 | handler 里不要用 `printf`；保存/恢复 `errno` |
