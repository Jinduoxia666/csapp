# 作业 07：Proxy Lab

状态：**待做**（官方 handout 已就位，`proxy.c` 保留未实现的初始代码）

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

已从 CMU 官方自学入口下载并解包。沿用前面实验的 `docs/`、`lectures/`
组织方式；评测脚本与 `tiny/` 保留官方相对位置，避免改变驱动依赖。

```text
07-proxy-lab/
├── README.md
├── Makefile
├── proxy.c             # 实验入口，当前仅打印 User-Agent 后退出
├── csapp.c / csapp.h   # 官方网络、RIO、线程等辅助函数
├── docs/
│   ├── proxylab.pdf    # 官方实验说明，先读这个
│   └── README          # 官方 handout 说明的中文译文
├── lectures/           # 四份课件
├── driver.sh           # Basic / Concurrency / Cache 自动评测
├── nop-server.py       # 并发测试的阻塞服务器
├── free-port.sh
├── port-for-user.pl
└── tiny/               # 完整 Tiny 服务器、中文说明、静态测试文件和 CGI adder
```

源码、Makefile 和评测脚本中的注释已翻译为中文；教材定位标记中的原始标识符保留。
已核对非注释代码与官方初始版本一致，尚未实现代理功能。
官方说明要求缓存支持多个线程同时读取，不能只用一把大互斥锁串行化所有缓存访问；
课堂上用全局锁复制缓存的例子仅用于解释生命周期，不是满足本实验全部要求的最终方案。

## 构建与测试

在本目录执行：

```sh
make
make -C tiny

# 代理完成后，在 Linux 上执行官方评测
./driver.sh
```

2026-09-14 已在本机 macOS arm64 验证：

- `make` 成功编译代理骨架；运行后仅打印 User-Agent 并退出，符合官方初始状态。
- `make -C tiny` 成功编译 Tiny 和 CGI adder。
- 官方 `csapp.c` 有 `sigismember` 返回值比较及 `sem_init` 弃用警告，保留原有辅助代码逻辑。
- 编译通过不代表代理功能通过；未运行完整自动评测。

官方 PDF 明确要求在 Linux 上运行 `driver.sh`。脚本依赖 Bash、curl、
Linux 风格的 netstat（通常由 net-tools 提供）、killall，以及
`nop-server.py` 的 `/usr/bin/python` 解释器路径；运行前检查这些依赖。
脚本会按进程名清理当前用户的 proxy、tiny 和 nop-server.py，适合在独立实验环境运行。

保留的 `make handin` 是官方课程打包规则，硬编码 `proxylab-handout` 目录名，
不适用于当前重命名后的自学目录；本次未改动该规则。

清理编译产物：

```sh
make clean
make -C tiny clean
```

## 来源与校验

2026-09-14 从 CMU 官方站点下载：

- <https://csapp.cs.cmu.edu/3e/labs.html>
- <https://csapp.cs.cmu.edu/3e/proxylab.pdf>
- <https://csapp.cs.cmu.edu/3e/proxylab-handout.tar>

SHA-256（压缩包已解包，仓库中不重复保存）：

```text
f43ce66288e8a99613e5bc66f3cecbf826c2b3b0144ff2c9acca1e5368c499f0  proxylab.pdf
9664e70733dc7eb467a15f622f0fcf7f110670e34430478ae4f580aafe964b39  proxylab-handout.tar
```
