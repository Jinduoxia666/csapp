# 第 14 讲：异常控制流 —— 异常与进程（中文版）

> 原讲义：`14-ecf-procs.pdf`
> CMU 15-213 *Introduction to Computer Systems*，Fall 2015 第 14 讲（2015-10-15）
> 讲师：Randal E. Bryant、David R. O'Hallaron
> 教材：CS:APP 第三版，对应第 8 章 8.1–8.4
>
> 本文是按幻灯片顺序整理的中文笔记，供 Shell Lab 复习使用；图示改用文字描述，术语保留英文原词。

## 目录

1. [异常控制流（ECF）](#1-异常控制流ecf)
2. [异常（Exceptions）](#2-异常exceptions)
3. [进程（Processes）](#3-进程processes)
4. [进程控制（Process Control）](#4-进程控制process-control)
5. [小结](#5-小结)

---

## 1. 异常控制流（ECF）

### 控制流

处理器只做一件事：从开机到关机，CPU 不断读取并执行（解释）一条又一条指令。这个指令序列就是 CPU 的**控制流**（control flow / flow of control）：

```
<startup>
inst1
inst2      ↓ 时间
inst3
...
instn
<shutdown>
```

### 改变控制流的手段

到目前为止学过两类改变控制流的机制：

- **跳转与分支**（jumps / branches）
- **调用与返回**（call / return）

它们的共同点是：**对程序状态的变化做出反应**。

但仅有这两种机制不足以构建一个可用的系统 —— 难以对**系统状态**的变化做出反应，例如：

- 数据从磁盘或网卡到达
- 指令除以 0
- 用户在键盘上按下 Ctrl-C
- 系统定时器到期

因此系统需要"**异常控制流**"（Exceptional Control Flow, ECF）机制。

### ECF 存在于系统的各个层次

**底层机制：**

1. **异常（Exceptions）**
   - 响应系统事件（即系统状态变化）而改变控制流
   - 由**硬件 + 操作系统软件**共同实现

**高层机制：**

2. **进程上下文切换（Process context switch）** —— 由 OS 软件和硬件定时器实现
3. **信号（Signals）** —— 由 OS 软件实现（下一讲）
4. **非本地跳转（Nonlocal jumps）**：`setjmp()` / `longjmp()` —— 由 C 运行时库实现（下一讲）

---

## 2. 异常（Exceptions）

### 定义

**异常**是响应某个事件（即处理器状态发生变化）而**把控制权转移给 OS 内核**的动作。

- **内核**（kernel）是操作系统常驻内存的那部分
- 事件举例：除以 0、算术溢出、缺页（page fault）、I/O 请求完成、按下 Ctrl-C

控制流示意：

```
用户代码                       内核代码
--------                       --------
I_current  --事件/异常-->      异常处理程序（exception handler）
I_next                          处理完毕后三种去向：
                                 • 返回 I_current（重新执行当前指令）
                                 • 返回 I_next（执行下一条指令）
                                 • 终止程序（abort）
```

### 异常表（Exception Table）

- 每类事件都有唯一的**异常号 k**
- k 作为**异常表**（又称中断向量表，interrupt vector）的索引
- 每次发生异常 k，就调用对应的 handler k

```
异常表
 ┌─────┐
 │  0  │ ──→ 异常处理程序 0 的代码
 │  1  │ ──→ 异常处理程序 1 的代码
 │  2  │ ──→ 异常处理程序 2 的代码
 │ ... │        ...
 │ n-1 │ ──→ 异常处理程序 n-1 的代码
 └─────┘
```

### 异步异常（中断，Interrupts）

由**处理器外部**的事件引起：

- 通过设置处理器的中断引脚（interrupt pin）来指示
- handler 返回到"下一条"指令

例子：

- **定时器中断**
  - 每隔几毫秒，外部定时器芯片触发一次中断
  - 内核借此从用户程序手里**夺回控制权**
- **来自外部设备的 I/O 中断**
  - 键盘上按下 Ctrl-C
  - 网络数据包到达
  - 磁盘数据到达

### 同步异常

由**执行某条指令**所导致的事件引起，分三类：

| 类型 | 是否有意 | 是否可恢复 | 返回行为 | 例子 |
|------|----------|------------|----------|------|
| **陷阱（Trap）** | 有意 | — | 返回"下一条"指令 | 系统调用、断点陷阱、特殊指令 |
| **故障（Fault）** | 无意 | 可能可恢复 | **重新执行**出错的（当前）指令，或终止 | 缺页（可恢复）、保护故障（不可恢复）、浮点异常 |
| **终止（Abort）** | 无意 | 不可恢复 | 终止当前程序 | 非法指令、奇偶校验错、机器检查 |

### 系统调用（System Calls）

每个 x86-64 系统调用都有唯一的 ID 号：

| 号码 | 名称 | 说明 |
|------|--------|--------------|
| 0 | `read` | 读文件 |
| 1 | `write` | 写文件 |
| 2 | `open` | 打开文件 |
| 3 | `close` | 关闭文件 |
| 4 | `stat` | 获取文件信息 |
| 57 | `fork` | 创建进程 |
| 59 | `execve` | 执行一个程序 |
| 60 | `_exit` | 终止进程 |
| 62 | `kill` | 向进程发送信号 |

**系统调用示例：打开文件**

用户调用 `open(filename, options)`，实际调用库函数 `__open`，后者执行 `syscall` 指令：

```asm
00000000000e5d70 <__open>:
  ...
  e5d79:  b8 02 00 00 00        mov    $0x2,%eax   # open 是 2 号系统调用
  e5d7e:  0f 05                 syscall            # 返回值在 %rax
  e5d80:  48 3d 01 f0 ff ff     cmp    $0xfffffffffffff001,%rax
  ...
  e5dfa:  c3                    retq
```

调用约定：

- `%rax` 存放系统调用号
- 其余参数依次放在 `%rdi`、`%rsi`、`%rdx`、`%r10`、`%r8`、`%r9`
  （注意：不是普通函数调用的 `%rcx`，而是 `%r10`）
- 返回值在 `%rax`
- **负返回值表示出错**，其绝对值对应 `errno`

### 故障示例 1：缺页（可恢复）

```c
int a[1000];
main()
{
    a[500] = 13;
}
```

```asm
80483b7:  c7 05 10 9d 04 08 0d   movl $0xd,0x8049d10
```

用户写内存，但该页当前在磁盘上：

```
用户代码            内核代码
movl  --缺页异常-->  把该页从磁盘拷贝到内存
      <--返回并重新执行 movl--
```

### 故障示例 2：非法内存引用（不可恢复）

```c
int a[1000];
main()
{
    a[5000] = 13;
}
```

```asm
80483b7:  c7 05 60 e3 04 08 0d   movl $0xd,0x804e360
```

```
用户代码            内核代码
movl  --缺页异常-->  检测到非法地址 → 向进程发信号
```

- 内核向用户进程发送 **SIGSEGV** 信号
- 用户进程以 "segmentation fault"（段错误）退出

---

## 3. 进程（Processes）

### 定义

> **进程是一个正在运行的程序的实例。**

- 这是计算机科学中最深刻的思想之一
- 注意区分：进程 ≠ 程序（program），也 ≠ 处理器（processor）

进程为每个程序提供两个关键**抽象**：

1. **逻辑控制流（Logical control flow）**
   - 每个程序似乎都独占 CPU
   - 由内核的**上下文切换**（context switching）机制提供
2. **私有地址空间（Private address space）**
   - 每个程序似乎都独占主存
   - 由内核的**虚拟内存**（virtual memory）机制提供

每个进程看到的抽象机器：

```
Memory            CPU
 Stack           Registers
 Heap
 Data
 Code
```

### 多处理：假象

计算机同时运行很多进程，每个进程都以为自己有一整套 Memory（Stack/Heap/Data/Code）+ CPU（Registers）：

- 一个或多个用户的应用程序：浏览器、邮件客户端、编辑器……
- 后台任务：监控网络与 I/O 设备

用 `top` 命令（Mac 上）可以看到：系统有 123 个进程，其中 5 个处于活动状态；每个进程由 **PID（Process ID）** 标识。

### 多处理：（传统的）现实

单处理器**并发**执行多个进程：

- 进程的执行是**交错**进行的（多任务，multitasking）
- 地址空间由虚拟内存系统管理（课程后面讲）
- **未在执行的进程，其寄存器值保存在内存中**（Saved registers）

上下文切换三步：

1. 把当前寄存器保存到内存
2. 调度下一个进程执行
3. 加载被保存的寄存器并切换地址空间 —— 这整个过程叫**上下文切换（context switch）**

### 多处理：（现代的）现实

**多核处理器（Multicore processors）**：

- 单芯片上有多个 CPU
- 共享主存（以及部分 cache）
- 每个核可以执行一个独立的进程
- 由内核负责把进程调度到各个核上

### 并发进程

- 每个进程是一条**逻辑控制流**
- 如果两个进程的流在**时间上有重叠**，就称它们**并发**（concurrent）
- 否则称它们是**顺序的**（sequential）

单核上运行的例子（进程 A、B、C）：

- 并发：A & B，A & C
- 顺序：B & C

### 用户视角下的并发进程

- 并发进程的控制流在**物理时间上是不相交的**（单核上任一时刻只有一个在跑）
- 但我们**可以把并发进程看作彼此并行运行**

### 上下文切换

- 进程由一块**常驻内存的共享 OS 代码**（即内核）管理
- **重要**：内核不是一个独立的进程，而是作为**某个已有进程的一部分**运行

控制流通过**上下文切换**从一个进程转移到另一个进程：

```
进程 A                进程 B
用户代码
内核代码  ──上下文切换──→
                       用户代码
              ←──上下文切换── 内核代码
用户代码
```

---

## 4. 进程控制（Process Control）

### 系统调用的错误处理

- 出错时，Linux 系统级函数通常返回 **-1**，并把全局变量 `errno` 设为错误原因
- **铁律**：必须检查**每一个**系统级函数的返回状态
  - 唯一的例外是那少数几个返回 `void` 的函数

```c
if ((pid = fork()) < 0) {
    fprintf(stderr, "fork error: %s\n", strerror(errno));
    exit(0);
}
```

### 错误报告函数

用一个**错误报告函数**可以稍作简化：

```c
void unix_error(char *msg) /* Unix 风格的错误 */
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}
```

于是调用处变成：

```c
if ((pid = fork()) < 0)
    unix_error("fork error");
```

### 错误处理包装函数

课程用 **Stevens 风格的包装函数**（首字母大写）进一步简化代码：

```c
pid_t Fork(void)
{
    pid_t pid;

    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}
```

调用处直接写：

```c
pid = Fork();
```

### 获取进程 ID

- `pid_t getpid(void)` —— 返回当前进程的 PID
- `pid_t getppid(void)` —— 返回父进程的 PID

### 创建与终止进程

从程序员的角度看，进程处于三种状态之一：

- **运行（Running）** —— 进程正在执行，或者正在等待被执行、终将被内核调度（选中执行）
- **停止（Stopped）** —— 进程执行被挂起，在收到进一步通知前不会被调度（下一讲讲信号时详述）
- **终止（Terminated）** —— 进程被永久停止

### 终止进程

进程因以下三种原因之一而终止：

1. 收到一个**默认动作为终止**的信号（下一讲）
2. 从 `main` 例程**返回**
3. 调用 `exit` 函数

```c
void exit(int status)
```

- 以退出状态 `status` 终止
- 约定：正常返回状态为 0，出错为非 0
- 另一种显式设置退出状态的方式是从 `main` 返回一个整数值
- **`exit` 调用一次，永不返回**

### 创建进程

父进程通过调用 `fork` 创建一个新的运行中的子进程。

```c
int fork(void)
```

- **对子进程返回 0，对父进程返回子进程的 PID**
- 子进程与父进程几乎完全相同：
  - 子进程得到父进程虚拟地址空间的一份**相同但独立**的拷贝
  - 子进程得到父进程已打开文件描述符的相同拷贝
  - 子进程的 PID 与父进程不同
- `fork` 之所以有趣（也常常令人困惑），是因为它 **调用一次，返回两次**

### fork 示例

```c
int main()
{
    pid_t pid;
    int x = 1;

    pid = Fork();
    if (pid == 0) {  /* 子进程 */
        printf("child : x=%d\n", ++x);
        exit(0);
    }

    /* 父进程 */
    printf("parent: x=%d\n", --x);
    exit(0);
}
```

```
linux> ./fork
parent: x=0
child : x=2
```

要点：

- **调用一次，返回两次**
- **并发执行** —— 无法预测父子进程的执行顺序
- **地址空间是副本但相互独立**
  - `fork` 返回时，父子进程中 `x` 的值都是 1
  - 此后对 `x` 的修改互不影响
- **共享打开的文件**
  - `stdout` 在父子进程中是同一个

### 用进程图（Process Graph）为 fork 建模

进程图是刻画并发程序中语句**偏序关系**的有用工具：

- 每个顶点是一条语句的执行
- `a -> b` 表示 a 先于 b 发生
- 边可以用变量的当前值标注
- `printf` 顶点可以用输出内容标注
- 每张图从一个没有入边的顶点开始

**图的任何一个拓扑排序都对应一种可行的全序执行顺序** —— 即所有边都从左指向右的顶点全序。

**进程图示例**（对应上面的 `fork.c`）：

```
                        child: x=2
                          printf ── exit          （子进程）
                        ↗
main ── fork ──────────
   x==1                 parent: x=0
                          printf ── exit          （父进程）
```

**解读进程图**：把顶点重新标号为

```
                e ── f
              ↗
a ── b ── c ── d
```

（a=main, b=fork, c=parent printf, d=parent exit, e=child printf, f=child exit）

- 可行的全序：`a b e c f d`
- 不可行的全序：`a b f c e d`（f 在 e 之前，违反了 e→f 的边）

### fork 示例：连续两次 fork

```c
void fork2()
{
    printf("L0\n");
    fork();
    printf("L1\n");
    fork();
    printf("Bye\n");
}
```

进程图（4 个进程最终都打印 Bye）：

```
                                        Bye
                              ┌────── printf
                     L1       │        Bye
        ┌───────── printf ── fork ── printf
        │
 L0     │            L1              Bye
printf ─┴─ fork ── printf ── fork ── printf
```

| 可行输出 | 不可行输出 |
|----------|------------|
| L0 / L1 / Bye / Bye / L1 / Bye / Bye | L0 / **Bye** / L1 / Bye / L1 / Bye / Bye |

（不可行的原因：第一个 Bye 必须排在它所属分支的 L1 之后。）

### fork 示例：在父进程中嵌套 fork

```c
void fork4()
{
    printf("L0\n");
    if (fork() != 0) {
        printf("L1\n");
        if (fork() != 0) {
            printf("L2\n");
        }
    }
    printf("Bye\n");
}
```

只有父进程继续嵌套，共 3 个进程：

```
                          Bye        Bye
                        printf     printf
  L0            L1                    L2      Bye
printf ─ fork ─ printf ─ fork ─ printf ─ printf
```

| 可行输出 | 不可行输出 |
|----------|------------|
| L0 / L1 / Bye / Bye / L2 / Bye | L0 / **Bye** / L1 / Bye / Bye / L2 |

### fork 示例：在子进程中嵌套 fork

```c
void fork5()
{
    printf("L0\n");
    if (fork() == 0) {
        printf("L1\n");
        if (fork() == 0) {
            printf("L2\n");
        }
    }
    printf("Bye\n");
}
```

只有子进程继续嵌套：

```
                                        L2      Bye
                                      printf ─ printf
                            L1                  Bye
                        ┌─ printf ─ fork ───── printf
  L0                    │
printf ─ fork ──────────┴──────────  Bye
                                    printf
```

| 可行输出 | 不可行输出 |
|----------|------------|
| L0 / Bye / L1 / L2 / Bye / Bye | L0 / Bye / L1 / **Bye** / Bye / **L2** |

### 回收（Reaping）子进程

**为什么要回收：**

- 进程终止后**仍然占用系统资源**（退出状态、各种 OS 表项等）
- 这种进程称为**僵尸（zombie）** —— "活死人"，半死不活

**回收（Reaping）：**

- 由父进程对已终止的子进程执行（使用 `wait` 或 `waitpid`）
- 父进程得到子进程的退出状态信息
- 内核随后**删除**这个僵尸子进程

**如果父进程不回收会怎样？**

- 如果父进程在回收某个子进程之前就终止了，这个**孤儿**子进程会被 `init` 进程（pid == 1）回收
- 所以只有**长期运行的进程**才需要显式回收 —— 例如 **shell 和服务器**

### 僵尸进程示例

```c
void fork7() {
    if (fork() == 0) {
        /* 子进程 */
        printf("Terminating Child, PID = %d\n", getpid());
        exit(0);
    } else {
        printf("Running Parent, PID = %d\n", getpid());
        while (1)
            ;  /* 无限循环 */
    }
}
```

```
linux> ./forks 7 &
[1] 6639
Running Parent, PID = 6639
Terminating Child, PID = 6640
linux> ps
  PID TTY          TIME CMD
 6585 ttyp9    00:00:00 tcsh
 6639 ttyp9    00:00:03 forks
 6640 ttyp9    00:00:00 forks <defunct>      ← 僵尸
 6641 ttyp9    00:00:00 ps
linux> kill 6639
[1]    Terminated
linux> ps
  PID TTY          TIME CMD
 6585 ttyp9    00:00:00 tcsh
 6642 ttyp9    00:00:00 ps
```

- `ps` 把子进程显示为 **"defunct"**（即僵尸）
- **杀掉父进程后，子进程被 `init` 回收**

### 子进程不终止的示例

```c
void fork8()
{
    if (fork() == 0) {
        /* 子进程 */
        printf("Running Child, PID = %d\n", getpid());
        while (1)
            ;  /* 无限循环 */
    } else {
        printf("Terminating Parent, PID = %d\n", getpid());
        exit(0);
    }
}
```

```
linux> ./forks 8
Terminating Parent, PID = 6675
Running Child, PID = 6676
linux> ps
  PID TTY          TIME CMD
 6585 ttyp9    00:00:00 tcsh
 6676 ttyp9    00:00:06 forks     ← 父进程已死，子进程还在跑
 6677 ttyp9    00:00:00 ps
linux> kill 6676
```

- **父进程已终止，子进程仍然活跃**
- 必须显式 kill 子进程，否则它会一直运行下去

### wait：与子进程同步

父进程通过调用 `wait` 回收一个子进程。

```c
int wait(int *child_status)
```

- 挂起当前进程，直到它的某个子进程终止
- 返回值是终止的那个子进程的 pid
- 如果 `child_status != NULL`，则它指向的整数会被设为一个值，表示子进程终止的原因和退出状态
  - 用 `wait.h` 中定义的宏来检查：
    `WIFEXITED`、`WEXITSTATUS`、`WIFSIGNALED`、`WTERMSIG`、`WIFSTOPPED`、`WSTOPSIG`、`WIFCONTINUED`
  - 细节见教材

**wait 示例：**

```c
void fork9() {
    int child_status;

    if (fork() == 0) {
        printf("HC: hello from child\n");
        exit(0);
    } else {
        printf("HP: hello from parent\n");
        wait(&child_status);
        printf("CT: child has terminated\n");
    }
    printf("Bye\n");
}
```

进程图（注意 `wait` 引入了子进程 exit → 父进程 wait 的**同步边**）：

```
          HC ─ exit
        printf     ╲
                    ╲（同步边）
  HP                 ╲       CT     Bye
fork ─ printf ────── wait ─ printf ─ printf
```

| 可行输出 | 不可行输出 |
|----------|------------|
| HC / HP / CT / Bye | HP / CT / Bye / **HC** |

（不可行的原因：CT 必须在 HC 之后，因为 `wait` 要等子进程 exit。）

### 另一个 wait 示例

- 如果**多个子进程都已完成**，`wait` 会以**任意顺序**取走其中一个
- 可以用宏 `WIFEXITED` 和 `WEXITSTATUS` 获取退出状态信息

```c
void fork10() {
    pid_t pid[N];
    int i, child_status;

    for (i = 0; i < N; i++)
        if ((pid[i] = fork()) == 0) {
            exit(100+i);  /* 子进程 */
        }
    for (i = 0; i < N; i++) {  /* 父进程 */
        pid_t wpid = wait(&child_status);
        if (WIFEXITED(child_status))
            printf("Child %d terminated with exit status %d\n",
                   wpid, WEXITSTATUS(child_status));
        else
            printf("Child %d terminate abnormally\n", wpid);
    }
}
```

### waitpid：等待特定的进程

```c
pid_t waitpid(pid_t pid, int *status, int options)
```

- 挂起当前进程，直到**指定的**进程终止
- 有多种 options（见教材）

```c
void fork11() {
    pid_t pid[N];
    int i;
    int child_status;

    for (i = 0; i < N; i++)
        if ((pid[i] = fork()) == 0)
            exit(100+i);  /* 子进程 */
    for (i = N-1; i >= 0; i--) {   /* 按逆序逐个等待 */
        pid_t wpid = waitpid(pid[i], &child_status, 0);
        if (WIFEXITED(child_status))
            printf("Child %d terminated with exit status %d\n",
                   wpid, WEXITSTATUS(child_status));
        else
            printf("Child %d terminate abnormally\n", wpid);
    }
}
```

### execve：加载并运行程序

```c
int execve(char *filename, char *argv[], char *envp[])
```

在**当前进程中**加载并运行：

- 可执行文件 `filename`
  - 可以是目标文件，也可以是以 `#!interpreter` 开头的脚本文件（例如 `#!/bin/bash`）
- 参数列表 `argv`
  - 按约定 `argv[0] == filename`
- 环境变量列表 `envp`
  - 形如 `"name=value"` 的字符串（例如 `USER=droh`）
  - 相关函数：`getenv`、`putenv`、`printenv`

行为：

- **覆盖**代码、数据和栈
- **保留** PID、已打开的文件和信号上下文
- **调用一次，永不返回** —— 除非出错

### 新程序启动时的栈结构

```
栈底（高地址）
 ┌──────────────────────────────┐
 │ 以 null 结尾的环境变量字符串 │
 │ 以 null 结尾的命令行参数字符串│
 ├──────────────────────────────┤
 │ envp[n] == NULL              │
 │ envp[n-1]                    │   ← environ（全局变量）
 │ ...                          │
 │ envp[0]                      │   ← envp（在 %rdx 中）
 ├──────────────────────────────┤
 │ argv[argc] = NULL            │
 │ argv[argc-1]                 │
 │ ...                          │
 │ argv[0]                      │   ← argv（在 %rsi 中）
 ├──────────────────────────────┤
 │ argc                         │   ← （在 %rdi 中）
 ├──────────────────────────────┤
 │ libc_start_main 的栈帧       │
 ├──────────────────────────────┤
 │ main 的未来栈帧              │
 └──────────────────────────────┘
栈顶（低地址）
```

### execve 示例

用当前环境在子进程中执行 `/bin/ls -lt /usr/include`：

```
myargv[3] = NULL
myargv[2] ──→ "/usr/include"      (argc == 3)
myargv[1] ──→ "-lt"
myargv[0] ──→ "/bin/ls"

envp[n] = NULL
envp[n-1] ──→ "PWD=/usr/droh"
...
envp[0]   ──→ "USER=droh"         ← environ
```

```c
if ((pid = Fork()) == 0) {   /* 子进程运行新程序 */
    if (execve(myargv[0], myargv, environ) < 0) {
        printf("%s: Command not found.\n", myargv[0]);
        exit(1);
    }
}
```

---

## 5. 小结

**异常**

- 需要非标准控制流的事件
- 外部产生（中断）或内部产生（陷阱和故障）

**进程**

- 任一时刻，系统中都有多个活跃进程
- 但在单核上，同一时刻只有一个能执行
- 每个进程看起来都完全掌控着处理器 + 私有内存空间

**衍生进程**

- 调用 `fork` —— **一次调用，两次返回**

**进程结束**

- 调用 `exit` —— **一次调用，零次返回**

**回收与等待进程**

- 调用 `wait` 或 `waitpid`

**加载并运行程序**

- 调用 `execve`（或其变体）—— **一次调用，（正常情况下）零次返回**

---

## 与 Shell Lab 的关联

本讲是 tshlab 的直接基础：

| 讲义内容 | 在 tsh.c 中的用途 |
|----------|-------------------|
| `fork` + `execve` | `eval()` 中创建子进程并执行用户命令 |
| `waitpid` + `WIFEXITED`/`WIFSIGNALED`/`WIFSTOPPED` | `waitfg()` 与 `sigchld_handler()` 中回收作业、判断结束原因 |
| 僵尸进程与回收 | shell 是长期运行进程，**必须**显式回收后台作业 |
| 错误处理包装函数 | `csapp.c` 提供的 `Fork()`、`Execve()` 等 |
| 进程图与偏序 | 分析父子进程竞态（下一讲的 `addjob`/`deletejob` 竞态） |
