# 作业 01：Data Lab

这是基于 CMU 15-213 Fall 2015 Data Lab 的自学练习。

## 题目

`bits.c` 包含以下函数：

- 整数题：`bitXor`、`allOddBits`、`isAsciiDigit`、`conditional`、`logicalNeg`、`tmin`、`isTmax`、`negate`、`isLessOrEqual`、`howManyBits`
- 浮点题：`float_twice`、`float_f2i`、`float_i2f`

整数题遵循 Data Lab 的限制：只能写直线代码，且只使用题目允许的位运算符和小常量。浮点题只操作 IEEE 754 单精度浮点数的 32 位二进制表示。

## 测试

在仓库根目录运行：

```sh
make test
```

测试包含典型边界值和 20 万组确定性随机输入。
