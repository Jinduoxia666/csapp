# 作业 05：Shell Lab

状态：**已完成**（16 个 trace 全部通过，正确性 80/80；风格分 10 分需人工评定）

| 项目 | 结果 |
| --- | --- |
| trace01–10 | 输出与 `traces/tshref.out` 逐字节一致 |
| trace11–13 | 非 `ps` 部分逐字节一致；`mysplit` 进程状态与参考一致 |
| trace14–16 | 输出与 `traces/tshref.out` 逐字节一致 |

评测在远端 CentOS 8 x86-64 上做，PID 规格化后与参考实现 `tshref` 的输出对比。
`tsh.c` 里的注释已全部改成中文（只动注释，剥掉注释后代码与官方 handout 一致）。

这是 CMU CS:APP3e Shell Lab（原 CS 213 Fall 2002 Lab L5）的自学版本，
在 f15 课程顺序里紧接 Cache Lab 之后。

## 目标

实现一个带作业控制的简易 shell `tsh.c`，练习异常控制流与进程管理：

- 解析命令行，`fork` + `execve` 运行前台/后台作业。
- 处理 `SIGCHLD`、`SIGINT`、`SIGTSTP` 信号，正确回收子进程、避免竞态。
- 实现内建命令 `quit`、`jobs`、`bg`、`fg` 和作业列表管理。

要补完的 7 个函数：`eval`、`builtin_cmd`、`do_bgfg`、`waitfg`、
`sigchld_handler`、`sigint_handler`、`sigtstp_handler`。

满分 90 分 = 16 个 trace × 5 分 + 风格 10 分（注释 5 分，检查**每个**系统调用
的返回值 5 分）。

先读 `docs/shlab-zh.md`，遇到细节问题对照 `docs/shlab.pdf`。

## 材料

`shlab-handout.tar` 已解包到本目录，并按用途分成了几个子目录：

```text
05-shell-lab/
├── README.md
├── Makefile        # 官方构建脚本，路径已按新目录改过
├── tsh.c           # 要写的 shell（骨架已给，7 个函数体是空的）
├── docs/
│   ├── shlab.pdf       # 官方作业说明
│   ├── shlab-zh.md     # 中文说明，按官方 PDF 整理
│   └── README          # 官方原始说明
├── lectures/           # f15 对应两讲的 PDF 与中文笔记
├── traces/
│   ├── trace01.txt … trace16.txt   # 16 个测试 trace，编号越大越复杂
│   └── tshref.out                  # 参考 shell 在全部 trace 上的输出
└── tools/
    ├── sdriver.pl      # trace 驱动的 shell 驱动程序（Perl）
    ├── tshref          # 参考 shell（Linux x86-64 二进制）
    └── myspin.c mysplit.c mystop.c myint.c   # trace 调用的小测试程序
```

只有 `tsh.c` 需要动。`make` 会把 `tools/` 下的四个小程序编译到当前目录
（`./myspin` 等），因为 trace 文件里就是这么调用它们的。

`Makefile` 相对官方版的改动：路径跟着新目录走；`test01`…`test16` /
`rtest01`…`rtest16` 用模式规则代替了 32 条重复目标；另加 `test-all` /
`rtest-all` 一次跑完 16 个 trace。

来源与校验（2026-08-21 从 CMU 官方站点下载）：

- <http://csapp.cs.cmu.edu/3e/shlab.pdf>
- <http://csapp.cs.cmu.edu/3e/shlab-handout.tar>

```text
ebabdfa2a3147996246b6950303b601c1e38b772fa34b191d4bc33383b0ed156  shlab.pdf
3aaad75abb6654eb5073690a3af2bc82f3ac031daccdb85866095566b4b0b8cc  shlab-handout.tar
```

## 参考讲义

`lectures/` 下放了 f15 对应的两讲 PDF 和中文笔记：

- `14-ecf-procs.pdf` / `14-ecf-procs-zh.md`：异常控制流、进程、`fork`/`exec`/`wait`。
- `15-ecf-signals.pdf` / `15-ecf-signals-zh.md`：信号、信号处理程序、非本地跳转。

来源：<https://www.cs.cmu.edu/afs/cs/academic/class/15213-f15/www/lectures/>

## 运行环境

本机是 macOS arm64，可以写、编译、交互式手测，但跑不了完整评测：

- `make` 能干净编译出 `tsh`、`myspin`、`mysplit`、`mystop`、`myint`（`-Wall -O2` 无警告）。
- `sdriver.pl` 在 macOS 自带的 Perl 上能跑，`make test01` 等可以用。
- `tools/tshref` 是 Linux x86-64 ELF，本地执行报 `exec format error`，所以 `make rtestNN`
  只能在 Linux 上跑；本地只能对着 `traces/tshref.out` 比。
- macOS 的 `/bin/echo` 不认 `-e`，也不解释 `\046`：trace 04–15 里
  `/bin/echo -e tsh> ./myspin 1 \046` 这类行，本地会原样打印
  `-e tsh> ./myspin 1 \046` 而不是 `tsh> ./myspin 1 &`。这只影响 trace 用来
  回显命令的那几行，shell 本身的行为照测，但输出没法直接和 `traces/tshref.out` diff。

所以和 Cache Lab 一样，评测放到远端 Linux 机器上做：

```sh
ssh order
cd /root/code/jinduoxia/shlab
```

远端是 CentOS 8 x86-64（gcc 8.5.0，perl 5.26.3），已经把 handout 同步过去，
`make rtest04` 验证过可以跑。

本地改完代码后同步过去：

```sh
rsync -a --delete --exclude 'lectures/' --exclude '.DS_Store' \
  --exclude 'tsh' --exclude 'myspin' --exclude 'mysplit' \
  --exclude 'mystop' --exclude 'myint' --exclude '*.dSYM/' \
  ./ order:/root/code/jinduoxia/shlab/
```

## 工作流

按 trace 编号从小到大做，每做完一个就和参考实现对齐：

```sh
make clean && make

make test01     # 跑自己的 tsh
make rtest01    # 跑参考 tshref（仅 Linux），逐行 diff

# 交互式手测
./tsh
tsh> /bin/ls -l
tsh> ./myspin 10 &
tsh> jobs
```

在远端一次跑完 16 个 trace 并和参考输出对比：

```sh
norm() { grep -vE 'sdriver\.pl|make\[' | sed -E 's/\([0-9]+\)/(PID)/g'; }
make test-all 2>&1 | norm > mine.out
norm < traces/tshref.out > ref.out
diff ref.out mine.out
```

PID 每次都不一样，先规格化成 `(PID)`；`make` 回显的驱动命令行（`./tools/sdriver.pl …`）
和 `tshref.out` 里记的路径不同，连同 CMU 当年录制时混进去的 `make[1]:` 一行一起滤掉。

剩下的差异应该只有 `trace11`–`trace13` 里 `/bin/ps a` 的输出——那是整台机器的进程表，
必然不同，只需保证里面 `mysplit` 进程的状态和参考输出一致。这套流程已经用
`make rtest-all`（拿参考 shell 自己跑一遍）在远端验证过：除 `ps` 的输出外与
`traces/tshref.out` 完全一致。

### 坑：trace11–13 要在 tty 下跑

`/bin/ps a` 只列**有控制终端**的进程。用 `ssh order '…'` 这种非交互方式跑
trace11–13，`ps` 的输出里根本看不到自己的 `tsh` 和 `mysplit`，看着像是进程组
没建对。要用 `ssh -tt` 分配一个伪终端：

```sh
ssh -tt order 'cd /root/code/jinduoxia/shlab && make test11 test12 test13; exit'
```

这时才能看到该看的东西：trace12 停止后有**两个** `./mysplit 4` 处于 `T` 状态
（父进程和它 fork 出来的子进程都收到了 SIGTSTP），trace11 被 SIGINT 打断后
两个都消失，trace13 `fg %1` 之后两个都恢复运行并跑完。


编译产物（`tsh`、`myspin`、`mysplit`、`mystop`、`myint`、`mine.out`、`ref.out`）
已在 `.gitignore` 里忽略。
