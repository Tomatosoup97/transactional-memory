#include "common.h"

int pow2_exp(size_t x) { return (64 - __builtin_clzl(x - 1)); }

size_t pow2(int exp) { return 1 << exp; }

size_t next_pow2(size_t x) { return pow2(pow2_exp(x)); }
