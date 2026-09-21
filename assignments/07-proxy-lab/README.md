# 作业 07：Proxy Lab

状态：**已完成**（Part I、Part II、Part III 均已实现并通过本地测试）；Linux 官方评分尚未运行。

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
├── proxy.c             # 多线程 HTTP 代理，包含缓存查询与响应完整性检查
├── cache.c / cache.h   # 支持并行读取的近似 LRU 缓存
├── csapp.c / csapp.h   # 官方网络、RIO、线程等辅助函数
├── docs/
│   ├── proxylab.pdf    # 官方实验说明，先读这个
│   └── README          # 官方 handout 说明的中文译文
├── lectures/           # 四份课件
├── tools/              # 转发、并发、缓存与 URL 解析测试
├── driver.sh           # Basic / Concurrency / Cache 官方自动评测
├── nop-server.py       # 并发测试的阻塞服务器
├── free-port.sh
├── port-for-user.pl
└── tiny/               # 完整 Tiny 服务器、中文说明、静态测试文件和 CGI adder
```

源码、Makefile 和评测脚本中的注释已翻译为中文；教材定位标记中的原始标识符保留。
下载时已核对初始代码；现在由 proxy.c 和 cache.c/cache.h 实现三个部分；csapp 和 Tiny 辅助代码保持原有逻辑。
官方说明要求缓存支持多个线程同时读取，不能只用一把大互斥锁串行化所有缓存访问；
课堂上用全局锁复制缓存的例子仅用于解释生命周期，不是满足本实验全部要求的最终方案。

## 构建与测试

在本目录执行：

```sh
make
make -C tiny
make test

# 在 Linux 上执行官方 Basic / Concurrency / Cache 评测
./driver.sh
```

2026-09-15 已在本机 macOS arm64 验证：

- 代理编译通过，proxy.c 无新增编译警告。
- `make test` 共 23 项：22 项通过，1 项跳过；测试无需公网。
- 官方 Basic 使用的 5 个 Tiny 文件（包括图片和可执行文件）经代理转发后逐字节一致。
- 验证请求行与请求头改写、显式端口、空路径、查询串、256 KiB 二进制内容及分块响应原样转发。
- 验证非法请求、过长请求头、上游拒绝连接和双方连接中断后，代理仍可服务下一次请求。
- 并发测试覆盖上游不响应、客户端请求头不完整时的请求隔离，以及 8 个并发客户端共 128 次请求的正确返回。
- 缓存验证覆盖源站停止后命中、128 次并发命中、请求键隔离、完整分块响应缓存、超大和不完整响应不缓存。
- 独立缓存测试覆盖容量边界、重复插入、读取更新 LRU、读锁并行及 8 线程共 2400 轮读写/淘汰；AddressSanitizer 和 UndefinedBehaviorSanitizer 检查通过。
- 真实默认端口测试因无法绑定本机 80 端口跳过；13 个 URL 解析场景单独通过，包含默认端口 80。
- 尚未在 Linux 上运行官方 driver.sh，因此不宣称获得官方 Basic、Concurrency 或 Cache 分数。
- 官方 csapp.c 的 sigismember 比较与 sem_init 弃用警告仍保留。

## Part I 实现说明

处理顺序：`accept → handle_client → parse_url → open_clientfd → 转发 → close`。
这是单条连接内部的处理顺序；Part II 已将此流程交给独立工作线程；Part III 在回源前查询缓存，并在完整接收后保存响应。

- 启动方式为 `./proxy 15213`；测试时选择未占用的非特权端口。
- 接收 HTTP/1.0 或 HTTP/1.1 的绝对 HTTP URL GET 请求，向源服务器发送 HTTP/1.0。
- URL 没有端口时使用 80，没有路径时使用 `/`；保留查询字符串。
- 保留浏览器提供的 Host；缺失时根据 URL 补充。固定 User-Agent、Connection: close
  和 Proxy-Connection: close，其他有效请求头按原内容转发。
- 使用实际读到的字节数转发完整响应，不用 strlen 计算响应体大小，读到源服务器关闭为止。
- 单次请求错误不退出代理；忽略 SIGPIPE，检查连接和读写错误，释放连接描述符。

当前范围与边界：只支持无请求体的 GET，不支持 HTTPS CONNECT、POST、折叠请求头。
请求行和单行请求头最多 8191 字节（含 CRLF），全部请求头少于 64 KiB。
错误请求返回 400，不支持的方法返回 501，请求头总量超限返回 431，
连接源服务器失败返回 502。已经开始转发后发生响应读取错误，关闭连接，不追加第二份 HTTP 响应。
当前采用阻塞 I/O，无超时机制；慢连接会占用对应工作线程，但不再阻塞主线程接待其他客户端。

## Part II 实现说明

主线程只负责 `accept` 和创建工作线程；`serve_client` 负责处理及关闭单条连接。

- 每次连接分配独立的 `int` 参数内存，避免向多个线程传递同一个循环变量的地址。
- 工作线程先复制描述符并释放参数，再调用 `handle_client`，最后关闭描述符并返回。
- 通过 `pthread_attr_setdetachstate(..., PTHREAD_CREATE_DETACHED)` 在创建时设置分离状态，
  线程退出后自动回收线程资源；主线程不调用阻塞的 `pthread_join`。
- 创建成功后主线程不再访问参数，也不关闭该连接；内存分配或线程创建失败时，
  主线程清理本次连接并继续接待，不退出整个代理。
- 请求缓冲区和 RIO 状态属于各自的工作线程；地址解析沿用可重入的 `open_clientfd`。
- 没有加入线程池、连接数限制和超时；大量长期阻塞的连接仍会消耗线程资源。

## Part III 实现说明

`cache_get` 在读锁内查找并复制数据，允许多个读者同时执行；复制结束后立即解锁，
网络发送只使用工作线程自己的副本。`cache_put` 持有写锁插入和淘汰条目，
不会释放正在被读者复制的数据。所有网络读写都在缓存锁之外。

- 缓存大小沿用官方宏：总响应字节最多 1,049,000，单个响应最多 102,400 字节。
  保存状态行、响应头和响应体的完整原始字节；键、链表节点等元数据不计入容量。
- 每个处理中的连接至多分配一个 100 KiB 临时对象缓冲区；超限立即释放缓冲区，
  继续向客户端转发，但不缓存该响应。
- 命中和插入都更新原子访问序号；空间不足时淘汰访问序号最小的条目，实现近似 LRU。
- 同一键的并发未命中可能分别回源；写入时去重，缓存只保留一份，不重复计费。
- 键包含主机、端口、路径/查询串和实际保留的请求头，避免不同 Host、Range 等请求混用响应。
- 仅缓存完整的 200 响应；存在 Content-Length 时检查长度一致，chunked 响应检查
  完整数据块、结束块及尾部空行；其他响应继续正常转发。上游读取失败或客户端写入失败时不缓存。
- 按实验简化模型处理缓存，不实现完整的 HTTP 缓存过期、重新验证或 Cache-Control 语义。

新增文件后正常执行 `make` 即可。`make test` 也会编译独立缓存测试并开启地址及未定义行为检测。

## 官方评测与清理

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
