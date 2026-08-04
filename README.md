# CSAPP 自学作业

这个仓库用于记录《深入理解计算机系统》（CSAPP）相关课程作业与练习。

## 作业目录

| 编号 | 作业 | 状态 |
| --- | --- | --- |
| 01 | [Data Lab](assignments/01-data-lab/README.md) | 已完成 |

## 运行测试

```sh
make test
```

清理编译产物：

```sh
make clean
```

## 环境假设

Data Lab 的整数题沿用原实验模型：32 位二进制补码 `int`，负数右移采用算术右移。
