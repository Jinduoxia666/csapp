#include "bits.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK_EQ(label, actual, expected)                                      \
    do {                                                                       \
        unsigned got_ = (unsigned)(actual);                                    \
        unsigned want_ = (unsigned)(expected);                                 \
        if (got_ != want_) {                                                   \
            if (failures < 12)                                                 \
                fprintf(stderr, "%s: got 0x%08x, expected 0x%08x\n",          \
                        (label), got_, want_);                                 \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static uint32_t next_random(void)
{
    static uint32_t state = 0x15u * 213u + 2015u;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static int reference_how_many_bits(int x)
{
    unsigned value = x < 0 ? (unsigned)~x : (unsigned)x;
    int bits = 1;

    while (value) {
        bits++;
        value >>= 1;
    }
    return bits;
}

static unsigned float_to_bits(float value)
{
    unsigned bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float bits_to_float(unsigned bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static unsigned reference_float_twice(unsigned uf)
{
    unsigned exp = (uf >> 23) & 0xff;
    if (exp == 0xff && (uf & 0x7fffff))
        return uf;
    return float_to_bits(bits_to_float(uf) * 2.0f);
}

static int reference_float_f2i(unsigned uf)
{
    unsigned exp = (uf >> 23) & 0xff;
    double value;

    if (exp == 0xff)
        return (int)0x80000000u;
    value = bits_to_float(uf);
    if (value < -2147483648.0 || value >= 2147483648.0)
        return (int)0x80000000u;
    return (int)value;
}

static unsigned reference_float_i2f(int x)
{
    return float_to_bits((float)x);
}

static void test_boundaries(void)
{
    static const int values[] = {
        INT_MIN, INT_MIN + 1, -1024, -255, -1, 0, 1, 47, 48, 57, 58,
        127, 255, 1024, INT_MAX - 1, INT_MAX
    };
    static const unsigned floats[] = {
        0x00000000u, 0x80000000u, 0x00000001u, 0x007fffffu,
        0x00800000u, 0x3f000000u, 0x3f800000u, 0x40000000u,
        0x4effffffu, 0x4f000000u, 0x7f000000u, 0x7f7fffffu,
        0x7f800000u, 0xff800000u, 0x7fc00001u
    };
    size_t i;
    size_t j;

    CHECK_EQ("tmin", tmin(), INT_MIN);
    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        int x = values[i];
        CHECK_EQ("allOddBits", allOddBits(x),
                 (((unsigned)x & 0xaaaaaaaau) == 0xaaaaaaaau));
        CHECK_EQ("isAsciiDigit", isAsciiDigit(x), x >= 0x30 && x <= 0x39);
        CHECK_EQ("logicalNeg", logicalNeg(x), !x);
        CHECK_EQ("isTmax", isTmax(x), x == INT_MAX);
        CHECK_EQ("negate", negate(x), 0u - (unsigned)x);
        CHECK_EQ("howManyBits", howManyBits(x), reference_how_many_bits(x));

        for (j = 0; j < sizeof(values) / sizeof(values[0]); j++) {
            int y = values[j];
            int z = values[(i + j + 1) % (sizeof(values) / sizeof(values[0]))];
            CHECK_EQ("bitXor", bitXor(x, y), x ^ y);
            CHECK_EQ("conditional", conditional(x, y, z), x ? y : z);
            CHECK_EQ("isLessOrEqual", isLessOrEqual(x, y), x <= y);
        }
    }

    for (i = 0; i < sizeof(floats) / sizeof(floats[0]); i++) {
        unsigned uf = floats[i];
        CHECK_EQ("float_twice", float_twice(uf), reference_float_twice(uf));
        CHECK_EQ("float_f2i", float_f2i(uf), reference_float_f2i(uf));
    }
}

static void test_random_inputs(void)
{
    int i;

    for (i = 0; i < 200000; i++) {
        int x = (int)next_random();
        int y = (int)next_random();
        int z = (int)next_random();
        unsigned uf = next_random();

        CHECK_EQ("bitXor/random", bitXor(x, y), x ^ y);
        CHECK_EQ("allOddBits/random", allOddBits(x),
                 (((unsigned)x & 0xaaaaaaaau) == 0xaaaaaaaau));
        CHECK_EQ("isAsciiDigit/random", isAsciiDigit(x), x >= 0x30 && x <= 0x39);
        CHECK_EQ("conditional/random", conditional(x, y, z), x ? y : z);
        CHECK_EQ("logicalNeg/random", logicalNeg(x), !x);
        CHECK_EQ("isTmax/random", isTmax(x), x == INT_MAX);
        CHECK_EQ("negate/random", negate(x), 0u - (unsigned)x);
        CHECK_EQ("isLessOrEqual/random", isLessOrEqual(x, y), x <= y);
        CHECK_EQ("howManyBits/random", howManyBits(x), reference_how_many_bits(x));
        CHECK_EQ("float_twice/random", float_twice(uf), reference_float_twice(uf));
        CHECK_EQ("float_f2i/random", float_f2i(uf), reference_float_f2i(uf));
        CHECK_EQ("float_i2f/random", float_i2f(x), reference_float_i2f(x));
    }
}

int main(void)
{
    if (sizeof(int) != 4 || sizeof(unsigned) != 4) {
        fprintf(stderr, "该测试要求 32 位 int 和 unsigned。\n");
        return 2;
    }

    test_boundaries();
    test_random_inputs();
    if (failures) {
        fprintf(stderr, "测试失败：%d 个结果不匹配。\n", failures);
        return 1;
    }

    puts("Data Lab tests passed (boundaries + 200000 random cases). ");
    return 0;
}
