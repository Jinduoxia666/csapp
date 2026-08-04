#include "bits.h"

/*
 * 这是基于 CMU 15-213 Fall 2015 Data Lab 的自学练习骨架。
 *
 * 整数题规则：
 * - 只能写直线代码：不能使用 if、for、while、switch、?:。
 * - 通常允许使用的运算符是：! ~ & ^ | + << >>。
 * - 有些函数会进一步限制可用运算符。
 * - 整数常量只能使用 8 位以内的值：0x00 到 0xff。
 *
 * 浮点题规则：
 * - 可以使用控制流。
 * - 不能使用 float/double 类型、浮点常量、union、struct、数组。
 * - 把 unsigned 参数当成单精度浮点数的 32 位二进制表示来处理。
 */

/* bitXor - 只使用 ~ 和 & 计算 x^y。难度：1，最多 14 个运算符。 */
int bitXor(int x, int y)
{
    return ~(~(x & ~y) & ~(~x & y));
}

/* allOddBits - x 的所有奇数编号 bit 均为 1 时返回 1。 */
int allOddBits(int x)
{
    int mask = 0xAA;
    mask = mask | (mask << 8);
    mask = mask | (mask << 16);
    return !((x & mask) ^ mask);
}

/* isAsciiDigit - 0x30 <= x <= 0x39 时返回 1。 */
int isAsciiDigit(int x)
{
    int lower = x + (~0x30 + 1);
    int upper = 0x39 + (~x + 1);
    return !((lower >> 31) | (upper >> 31));
}

/* conditional - 实现与 x ? y : z 相同的效果。 */
int conditional(int x, int y, int z)
{
    int mask = !!x;
    mask = ~mask + 1;
    return (mask & y) | (~mask & z);
}

/* logicalNeg - 不使用 ! 计算 !x。 */
int logicalNeg(int x)
{
    return ((x | (~x + 1)) >> 31) + 1;
}

/* tmin - 返回最小的补码整数。 */
int tmin(void)
{
    return 1 << 31;
}

/* isTmax - x 是最大的补码整数时返回 1。 */
int isTmax(int x)
{
    int a = x + 1;
    int b = a + a;
    return !b & !!a;
}

/* negate - 不使用一元负号返回 -x。 */
int negate(int x)
{
    return ~x + 1;
}

/* isLessOrEqual - x <= y 时返回 1。 */
int isLessOrEqual(int x, int y)
{
    int sx = (x >> 31) & 1;
    int sy = (y >> 31) & 1;
    int signDiff = sx ^ sy;
    int sub = y + (~x + 1);
    int subSign = (sub >> 31) & 1;
    return (signDiff & sx) | (!signDiff & !subSign);
}

/* howManyBits - 返回以补码形式表示 x 所需的最少 bit 数。 */
int howManyBits(int x)
{
    int sign = x >> 31;
    int b16, b8, b4, b2, b1, b0;

    x = (sign & ~x) | (~sign & x);
    b16 = (!!(x >> 16)) << 4;
    x = x >> b16;
    b8 = (!!(x >> 8)) << 3;
    x = x >> b8;
    b4 = (!!(x >> 4)) << 2;
    x = x >> b4;
    b2 = (!!(x >> 2)) << 1;
    x = x >> b2;
    b1 = !!(x >> 1);
    x = x >> b1;
    b0 = x;

    return b16 + b8 + b4 + b2 + b1 + b0 + 1;
}

/* float_twice - 返回 2.0*f 的 bit 级等价结果，NaN 原样返回。 */
unsigned float_twice(unsigned uf)
{
    unsigned exp = (uf >> 23) & 0xFF;
    unsigned sign = uf & 0x80000000;

    if (exp == 0xFF)
        return uf;
    if (exp == 0)
        return sign | (uf << 1);

    uf = uf + (1 << 23);
    if (((uf >> 23) & 0xFF) == 0xFF)
        uf = uf & 0xFF800000;
    return uf;
}

/* float_f2i - 返回 (int)f；越界或 NaN 时返回 0x80000000u。 */
int float_f2i(unsigned uf)
{
    unsigned sign = uf >> 31;
    int E = ((uf >> 23) & 0xFF) - 127;
    unsigned M = (uf & 0x7FFFFF) | 0x800000;
    unsigned result;

    if (E < 0)
        return 0;
    if (E >= 31)
        return 0x80000000u;

    if (E >= 23)
        result = M << (E - 23);
    else
        result = M >> (23 - E);

    return sign ? -result : result;
}

/* float_i2f - 返回 (float)x 的 bit 级等价结果。 */
unsigned float_i2f(int x)
{
    unsigned sign = 0;
    unsigned ax;
    int n = 31;
    unsigned exp;
    unsigned frac;
    int s;
    unsigned dropped;
    unsigned half;
    unsigned round_up = 0;
    unsigned result;

    if (x == 0)
        return 0;

    if (x < 0) {
        sign = 0x80000000;
        ax = -(unsigned)x;
    } else {
        ax = x;
    }

    while (!(ax & (1u << n)))
        n--;

    exp = n + 127;
    if (n <= 23) {
        frac = (ax << (23 - n)) & 0x7FFFFF;
        return sign | (exp << 23) | frac;
    }

    s = n - 23;
    frac = (ax >> s) & 0x7FFFFF;
    dropped = ax & ((1u << s) - 1);
    half = 1u << (s - 1);

    if (dropped > half)
        round_up = 1;
    else if (dropped == half && (frac & 1))
        round_up = 1;

    result = sign | (exp << 23) | frac;
    return result + round_up;
}
