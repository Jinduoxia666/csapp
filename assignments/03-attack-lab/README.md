# 作业 03：Attack Lab

这是 CMU CS:APP3e Attack Lab 的公开自学版本。目标是在授权的实验目标上完成 3 个代码注入攻击和 2 个返回导向编程（ROP）攻击，理解 x86-64 栈、调用约定、指令编码以及缓冲区溢出的防护机制。仓库不包含题解或现成反汇编结果。

## 材料

- `ctarget`：用于 Phase 1～3 的代码注入目标。
- `rtarget`：用于 Phase 4～5 的 ROP 目标。
- `hex2raw`：将以十六进制文本表示的攻击字符串转换成原始字节。
- `cookie.txt`：当前 target 实例的 4 字节签名。
- `farm.c`：`rtarget` 中 gadget farm 的源代码。
- `README.txt`：自学包原始文件说明。
- `attacklab.pdf`：官方作业说明。

| 阶段 | 目标 | 任务 |
| --- | --- | --- |
| 1 | `ctarget` | 将控制流重定向到 `touch1` |
| 2 | `ctarget` | 注入代码并以 cookie 调用 `touch2` |
| 3 | `ctarget` | 注入代码并以 cookie 字符串调用 `touch3` |
| 4 | `rtarget` | 使用 ROP gadget 调用 `touch2` |
| 5 | `rtarget` | 使用 ROP gadget 调用 `touch3` |

官方入口：

- <https://csapp.cs.cmu.edu/3e/labs.html>
- <http://csapp.cs.cmu.edu/3e/target1.tar>
- <https://csapp.cs.cmu.edu/3e/attacklab.pdf>

本次获取时 CMU 自学包下载服务不可用，因此从公开的 [CSAPP Labs 镜像](https://github.com/Zhenye-Na/CSAPP-Labs)提取原始材料，并与另一个独立的 [15-213 课程仓库](https://github.com/JasonQSY/CMU-15-213-ICS)逐字节交叉核对。镜像中的题解、攻击字符串和现成反汇编均未复制。

核心文件的 SHA-256：

```text
1c71d90dd20a28ffdd32b8bd7986ec440c1fde1f16427ed35d4cb4617611659d  ctarget
be8394b826199bddcdcddb79bf3a3a4233dc70e5a9a9637dff79ba2edb24b4b0  rtarget
9b5beb6ee5c13c229f5ec04732e92cd09d3e7232959396ddd461c4e22d78ea31  hex2raw
14d6b7418b827e31ff9087a9451c7ca34fec395dc9ea2947612dc719f18a350a  farm.c
4d0a08538d98a35cbff5c3a56bd3f9c7f6435dd7104644faa700653f04774ed8  cookie.txt
61d020b7240c15ed3bb7c711cca0ac87baeb48c418d59c6bc3513d0209a21d62  attacklab.pdf
```

## 调试环境

本机是 macOS arm64，不能直接运行这些 Linux x86-64 程序。材料同步到远端后使用：

```sh
ssh order
cd /root/code/jinduoxia/attacklab
```

## 安全运行

官方要求自学版始终使用 `-q`，否则 target 会尝试连接不存在的评分服务器：

```sh
./ctarget -q
./rtarget -q
```

通过 GDB 调试时也要保留该参数：

```sh
gdb --args ./ctarget -q
gdb --args ./rtarget -q
```

本仓库中的目标程序只应用于这份授权实验，不应用于其他程序或系统。

## 基本工作流

先生成反汇编文件：

```sh
objdump -d -M intel ctarget > ctarget.asm
objdump -d -M intel rtarget > rtarget.asm
```

攻击字符串使用空白分隔的两位十六进制字节表示。例如，`48 89 e0` 表示三个原始字节，不要写 `0x` 前缀。执行时通过 `hex2raw` 转换，并始终带 `-q`：

```sh
./hex2raw < exploit.txt | ./ctarget -q
```

建议先阅读 `attacklab.pdf`，然后从 Phase 1 开始。每个阶段单独保存攻击字符串和分析过程，确认成功后再进入下一阶段。
