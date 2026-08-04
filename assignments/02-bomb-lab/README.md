# 作业 02：Bomb Lab

这是 CMU CS:APP3e Bomb Lab 的公开自学版本。目标是使用反汇编器和调试器分析 `bomb`，依次找出六个阶段要求的输入。仓库不包含答案或现成反汇编结果。

## 材料

- `bomb`：Linux x86-64 ELF 可执行文件，包含调试信息且未剥离符号。
- `bomb.c`：程序入口和六个阶段的调用骨架，不包含阶段实现。
- `bomblab.pdf`：官方作业说明。

材料的官方入口：

- <https://csapp.cs.cmu.edu/3e/labs.html>
- <http://csapp.cs.cmu.edu/3e/bomb.tar>
- <https://csapp.cs.cmu.edu/3e/bomblab.pdf>

本次获取时 CMU 下载主机拒绝连接，因此从公开的 [CSAPP Labs 镜像](https://github.com/Zhenye-Na/CSAPP-Labs)提取了同版 `bomb`、`bomb.c` 和 `bomblab.pdf`；未复制镜像中的答案、题解或反汇编文件。`bomb` 又与另一个独立的 [15-213 课程仓库](https://github.com/JasonQSY/CMU-15-213-ICS)逐字节交叉核对，结果一致。

文件的 SHA-256：

```text
8849e033691d51426c0c91a76eeb0c346eddd37e8fdf21cd93acd16669f1b461  bomb
43b696e7a69ec9d7ef021aa65406a874f8ac8891edf989e3b3d2f0f0c29ad5ab  bomb.c
be4991aa83e89fe9ffb1acd60816d41530edfca1d47093dd74effa38e3d9d31e  bomblab.pdf
```

## 调试环境

本机是 macOS arm64，不能直接运行该 Linux x86-64 程序。材料已同步到以下 Linux x86-64 环境：

```sh
ssh order
cd /root/code/jinduoxia/bomblab
```

启动调试器：

```sh
gdb ./bomb
```

建议进入 GDB 后先阻止炸弹真正执行爆炸路径：

```gdb
break explode_bomb
run
```

常用命令：

```gdb
disassemble phase_1
break phase_1
info registers
x/i $pc
stepi
nexti
```

也可以先生成 Intel 语法的完整反汇编：

```sh
objdump -d -M intel bomb > bomb.asm
```

## 保存进度

在当前目录创建 `answers.txt`，每行保存一个已经确认的阶段输入：

```sh
./bomb answers.txt
```

程序读取完文件中的答案后会继续从标准输入读取。`answers.txt` 已被 Git 忽略，避免误提交答案。
