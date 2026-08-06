# CSAPP 自学作业

这个仓库用于记录《深入理解计算机系统》（CSAPP）相关课程作业与练习。

## 作业目录

| 编号 | 作业 | 状态 |
| --- | --- | --- |
| 01 | [Data Lab](assignments/01-data-lab/README.md) | 已完成 |
| 02 | [Bomb Lab](assignments/02-bomb-lab/README.md) | 已完成 |
| 03 | [Attack Lab](assignments/03-attack-lab/README.md) | 已完成 |
| 04 | [Cache Lab](assignments/04-cache-lab/README.md) | 待做 |
| 05 | [Shell Lab](assignments/05-shell-lab/README.md) | 待做 |

## 运行测试

```sh
make test
```

清理编译产物：

```sh
make clean
```

## 环境假设

Data Lab 的整数题沿用原实验模型：32 位二进制补码 `int`，负数右移采用算术右移。Bomb Lab 和 Attack Lab 是 Linux x86-64 程序，需要在对应架构的 Linux 环境中运行；Attack Lab 自学版必须使用 `-q` 参数，避免连接评分服务器。
