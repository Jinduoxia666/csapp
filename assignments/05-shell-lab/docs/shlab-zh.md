# Shell Lab 中文说明

> 本文是官方 `shlab.pdf`（CMU CS 213, Fall 2002 / CS:APP3e 自学版 Lab L5）的中文整理版，供自学使用。
> 函数名、命令、参数、输出格式保持原文不变。与英文原文有出入时以 `shlab.pdf` 为准。
> 原文中与课程相关的内容（分组、上交方式、截止时间、AFS 目录）已略去。

## 1 概述

这个实验的目的是熟悉**进程控制**与**信号**：写一个支持作业控制（job control）的简易 Unix shell。

`tsh.c`（tiny shell）里已经给出了一个能跑的骨架，比较无聊的函数官方已经实现好了，你要补完下面这些空函数。括号里是参考答案的大致行数（含大量注释）：

| 函数 | 职责 | 参考行数 |
| --- | --- | --- |
| `eval` | 解析并解释命令行的主例程 | 70 |
| `builtin_cmd` | 识别并执行内建命令 `quit`、`fg`、`bg`、`jobs` | 25 |
| `do_bgfg` | 实现 `bg` 和 `fg` 内建命令 | 50 |
| `waitfg` | 等待前台作业结束 | 20 |
| `sigchld_handler` | 捕获 `SIGCHLD` | 80 |
| `sigint_handler` | 捕获 `SIGINT`（ctrl-c） | 15 |
| `sigtstp_handler` | 捕获 `SIGTSTP`（ctrl-z） | 15 |

每次改完 `tsh.c` 都执行 `make` 重新编译。运行自己的 shell：

```sh
unix> ./tsh
tsh> [在这里输入命令]
```

## 2 材料清单

> 官方 handout 的文件在本目录里按用途分到了 `docs/`、`traces/`、`tools/` 三个子目录，
> `Makefile` 里的路径已经跟着改好，`make`、`make test01` 这类命令照常可用。
> 下表第二列是本仓库里的实际位置。

| 文件 | 位置 | 说明 |
| --- | --- | --- |
| `tsh.c` | `tsh.c` | **要写的** shell 程序 |
| `Makefile` | `Makefile` | 编译 shell 并运行测试（路径已按新目录改过） |
| `README` | `docs/README` | 官方原始说明 |
| `tshref` | `tools/tshref` | 参考 shell 的可执行文件（Linux x86-64 二进制） |
| `sdriver.pl` | `tools/sdriver.pl` | trace 驱动的 shell 驱动程序 |
| `trace01.txt` … `trace16.txt` | `traces/` | 驱动 shell 的 16 个 trace 文件 |
| `tshref.out` | `traces/tshref.out` | 参考 shell 在全部 trace 上的输出样例 |

trace 文件会调用下面这几个小 C 程序（源码在 `tools/`，`make` 会把它们编译到当前目录，
因为 trace 里是按 `./myspin` 这样调用的）：

| 文件 | 说明 |
| --- | --- |
| `tools/myspin.c` | 接受参数 `<n>`，空转 `<n>` 秒 |
| `tools/mysplit.c` | fork 出一个子进程，子进程空转 `<n>` 秒 |
| `tools/mystop.c` | 空转 `<n>` 秒后给自己发 `SIGTSTP` |
| `tools/myint.c` | 空转 `<n>` 秒后给自己发 `SIGINT` |

## 3 Unix shell 概述

shell 是一个交互式的命令行解释器，代表用户运行程序。它反复地：打印提示符 → 在 `stdin` 上等一行命令 → 按命令内容做事。

命令行是一串用空白分隔的 ASCII 单词。第一个单词要么是**内建命令**的名字，要么是**可执行文件的路径名**，其余单词是命令行参数。如果第一个单词是内建命令，shell 就在当前进程里立刻执行它；否则把它当成可执行程序的路径，fork 一个子进程，在子进程的上下文里加载并运行这个程序。解释一行命令所创建的这些子进程合起来叫做一个**作业**（job）。一般来说一个作业可以由多个用管道连接的子进程组成。

如果命令行以 `&` 结尾，作业就在**后台**运行：shell 不等它结束就打印提示符、等下一条命令。否则作业在**前台**运行：shell 等它结束才继续。所以任何时刻最多有一个作业在前台运行，但后台作业可以有任意多个。

例如：

```text
tsh> jobs
```

让 shell 执行内建的 `jobs` 命令。而

```text
tsh> /bin/ls -l -d
```

在前台运行 `ls`。按惯例，当程序开始执行 `int main(int argc, char *argv[])` 时，参数是：

- `argc == 3`
- `argv[0] == "/bin/ls"`
- `argv[1] == "-l"`
- `argv[2] == "-d"`

写成

```text
tsh> /bin/ls -l -d &
```

则在后台运行 `ls`。

Unix shell 支持**作业控制**：用户可以把作业在前台和后台之间搬来搬去，也可以改变作业中进程的状态（运行、停止、终止）。

- 敲 ctrl-c 会给前台作业里的每个进程发 `SIGINT`，其默认行为是终止进程。
- 敲 ctrl-z 会给前台作业里的每个进程发 `SIGTSTP`，其默认行为是把进程置为停止（stopped）状态，直到收到 `SIGCONT` 才被唤醒。

shell 通常还提供这些支持作业控制的内建命令：

- `jobs`：列出正在运行和已停止的后台作业。
- `bg <job>`：把一个已停止的后台作业变成正在运行的后台作业。
- `fg <job>`：把一个已停止或正在运行的后台作业变成前台运行。
- `kill <job>`：终止一个作业。

## 4 tsh 规范

你的 `tsh` 要满足下面这些要求：

- 提示符是字符串 `"tsh> "`。
- 用户输入的命令行由一个名字和零个或多个参数组成，之间用一个或多个空格分隔。如果名字是内建命令，`tsh` 立即处理它并等待下一行命令；否则把名字当作可执行文件的路径，在一个初始子进程的上下文中加载并运行它（这个初始子进程就是这里说的**作业**）。
- `tsh` **不需要**支持管道（`|`）和 I/O 重定向（`<`、`>`）。
- 敲 ctrl-c（ctrl-z）应当把 `SIGINT`（`SIGTSTP`）发给当前的前台作业**以及它的所有后代进程**（例如它 fork 出来的子进程）。如果没有前台作业，这个信号应当没有任何效果。
- 命令行以 `&` 结尾时在后台运行作业，否则在前台运行。
- 每个作业既可以用进程 ID（PID）标识，也可以用 `tsh` 分配的正整数作业 ID（JID）标识。命令行上的 JID 用前缀 `%` 表示：`%5` 表示 JID 5，`5` 表示 PID 5。（操作作业列表需要的例程官方都已经写好了。）
- `tsh` 要支持下面的内建命令：
  - `quit`：终止 shell。
  - `jobs`：列出所有后台作业。
  - `bg <job>`：给 `<job>` 发 `SIGCONT` 让它重新运行，然后放到后台跑。`<job>` 可以是 PID 或 JID。
  - `fg <job>`：给 `<job>` 发 `SIGCONT` 让它重新运行，然后放到前台跑。`<job>` 可以是 PID 或 JID。
- `tsh` 要回收（reap）自己所有的僵死子进程。如果某个作业因为收到一个它没有捕获的信号而终止，`tsh` 要识别这个事件，并打印一条包含该作业 PID 和信号说明的消息。

## 5 检查你的工作

### 5.1 参考 shell

Linux 可执行文件 `tshref` 是本实验的参考答案。对 shell 该有什么行为拿不准时，直接运行它看看。你的 shell 的输出应当和参考答案**逐字节一致**（PID 当然除外，它每次运行都不一样）。

### 5.2 shell 驱动程序

`sdriver.pl` 把一个 shell 当作子进程运行，按 trace 文件的指示给它发命令和信号，并捕获、显示这个 shell 的输出。用 `-h` 看用法：

```text
unix> ./sdriver.pl -h
Usage: sdriver.pl [-hv] -t <trace> -s <shellprog> -a <args>
Options:
  -h            Print this message
  -v            Be more verbose
  -t <trace>    Trace file
  -s <shell>    Shell program to test
  -a <args>     Shell arguments
  -g            Generate output for autograder
```

用 `trace01.txt` 测自己的 shell（原文写法在左，本目录分完目录后的实际路径在右）：

```sh
unix> ./sdriver.pl -t trace01.txt -s ./tsh -a "-p"              # 原文
unix> ./tools/sdriver.pl -t traces/trace01.txt -s ./tsh -a "-p" # 本目录
```

（`-a "-p"` 告诉你的 shell 不要打印提示符），等价于：

```sh
unix> make test01
```

同样地，用参考 shell 跑同一个 trace 作对比：

```sh
unix> ./tools/sdriver.pl -t traces/trace01.txt -s ./tools/tshref -a "-p"
# 或
unix> make rtest01
```

`traces/tshref.out` 里已经给出了参考答案在全部 trace 上的输出，比自己逐个手动跑一遍方便。

trace 文件的好处是：它生成的输出，和你交互式地敲这些命令得到的输出是一样的（只多了开头一段标识 trace 的注释）。例如：

```text
$ make test15
./tools/sdriver.pl -t traces/trace15.txt -s ./tsh -a "-p"
#
# trace15.txt - Putting it all together
#
tsh> ./bogus
./bogus: Command not found
tsh> ./myspin 10
Job [1] (26343) terminated by signal 2
tsh> ./myspin 3 &
[1] (26345) ./myspin 3 &
tsh> ./myspin 4 &
[2] (26347) ./myspin 4 &
tsh> jobs
[1] (26345) Running ./myspin 3 &
[2] (26347) Running ./myspin 4 &
tsh> fg %1
Job [1] (26345) stopped by signal 20
tsh> jobs
[1] (26345) Stopped ./myspin 3 &
[2] (26347) Running ./myspin 4 &
tsh> bg %3
%3: No such job
tsh> bg %1
[1] (26345) ./myspin 3 &
tsh> jobs
[1] (26345) Running ./myspin 3 &
[2] (26347) Running ./myspin 4 &
tsh> fg %1
tsh> quit
```

> `shlab.pdf` 里印的这段样例出自更早的版本（`Command not found.` 带句点、
> `Job (9721)` 不带 JID、`Running` 后面补空格对齐），和本 handout 附带的
> `tshref` 实际输出对不上。**以 `traces/tshref.out` 为准**，上面这段就是从 `traces/tshref.out` 抄来的
> （只有开头那行驱动命令按本目录的新路径改过）。

### 5.3 16 个 trace 一览

编号越小的 trace 测得越简单，按顺序做即可：

| trace | 考点 |
| --- | --- |
| 01 | 遇到 EOF 时正确退出 |
| 02 | 内建命令 `quit` |
| 03 | 运行一个前台作业 |
| 04 | 运行一个后台作业 |
| 05 | 内建命令 `jobs` |
| 06 | 把 `SIGINT` 转发给前台作业 |
| 07 | `SIGINT` **只**转发给前台作业 |
| 08 | `SIGTSTP` **只**转发给前台作业 |
| 09 | 内建命令 `bg` |
| 10 | 内建命令 `fg` |
| 11 | 把 `SIGINT` 转发给前台**进程组**里的每个进程 |
| 12 | 把 `SIGTSTP` 转发给前台进程组里的每个进程 |
| 13 | 重启进程组里所有被停止的进程 |
| 14 | 简单的错误处理 |
| 15 | 综合测试 |
| 16 | 处理来自其它进程（而非终端）的 `SIGTSTP` 和 `SIGINT` |

### 5.4 参考 shell 的输出格式

抄错一个字都会导致 diff 不一致，这几条消息照抄（来自 `tools/tshref` 与 `traces/tshref.out`）：

```text
%s: Command not found
%s command requires PID or %%jobid argument
%s: argument must be a PID or %%jobid
(%d): No such process
%s: No such job
Job [%d] (%d) terminated by signal %d
Job [%d] (%d) stopped by signal %d
```

作业列表一行的格式是 `[jid] (pid) 状态 命令行`，状态字段为 `Running `、`Stopped ` 或 `Foreground `（都带一个尾随空格，`listjobs` 已经写好，照用即可）。后台作业启动时打印 `[jid] (pid) 命令行`。

## 6 提示

- 把教材第 8 章（异常控制流）**逐字**读一遍。本目录 `lectures/` 下有对应两讲的中文笔记。
- 用 trace 文件驱动开发：从 `trace01.txt` 开始，确认你的 shell 输出和参考 shell 完全一致，再做 `trace02.txt`，依此类推。
- `waitpid`、`kill`、`fork`、`execve`、`setpgid`、`sigprocmask` 这几个函数会非常有用。`waitpid` 的 `WUNTRACED` 和 `WNOHANG` 选项也用得上。
- 写信号处理程序时，记得把 `SIGINT`、`SIGTSTP` 发给**整个前台进程组**：`kill` 的参数用 `-pid` 而不是 `pid`。`sdriver.pl` 会专门测这个错误。
- 本实验比较难拿捏的一点是 `waitfg` 和 `sigchld_handler` 之间怎么分工。推荐做法：
  - `waitfg` 里用一个围绕 `sleep` 的忙等待循环；
  - `sigchld_handler` 里**只调用一次** `waitpid`。

  其它方案（比如在 `waitfg` 和 `sigchld_handler` 里都调 `waitpid`）也能做出来，但很容易把自己绕晕。把回收工作全部放在处理程序里更简单。
- 在 `eval` 里，父进程必须在 `fork` 之前用 `sigprocmask` **阻塞** `SIGCHLD`，在调用 `addjob` 把子进程加进作业列表之后再用 `sigprocmask` 解除阻塞。由于子进程会继承父进程的阻塞集合，子进程必须在 `execve` 新程序之前先解除对 `SIGCHLD` 的阻塞。

  父进程这样阻塞 `SIGCHLD`，是为了避免这样的竞态：子进程在父进程调用 `addjob` 之前就被 `sigchld_handler` 回收（因而从作业列表里删掉）了。
- `more`、`less`、`vi`、`emacs` 这类程序会对终端设置做奇怪的事情，别从你的 shell 里运行它们。用 `/bin/ls`、`/bin/ps`、`/bin/echo` 这类简单的文本程序。
- 从标准 Unix shell 里启动你的 shell 时，你的 shell 处在前台进程组里。它 fork 出的子进程默认也属于这个前台进程组。而敲 ctrl-c 会给前台进程组里的每个进程发 `SIGINT`，那就会把 `SIGINT` 同时发给你的 shell 和它创建的每个进程——显然不对。

  解决办法：在 `fork` 之后、`execve` 之前，子进程调用 `setpgid(0, 0)`，把自己放进一个新的进程组，组 ID 就等于子进程的 PID。这样前台进程组里就只剩你的 shell 一个进程。敲 ctrl-c 时，shell 捕获到 `SIGINT`，再把它转发给相应的前台作业（更准确地说，是包含前台作业的那个进程组）。

## 7 评分

满分 90 分：

- **正确性 80 分**：16 个 trace，每个 5 分。
- **风格 10 分**：注释写好（5 分）；**检查每一个**系统调用的返回值（5 分）。

评测在 Linux 机器上进行，用的就是本目录里的 shell 驱动和 trace 文件。你的 shell 在这些 trace 上的输出应当和参考 shell 一致，只有两处例外：

- PID 会（也必然会）不同。
- `traces/trace11.txt`、`traces/trace12.txt`、`traces/trace13.txt` 里 `/bin/ps` 的输出每次运行都不一样。不过输出里 `mysplit` 进程的运行状态应当是一致的。
