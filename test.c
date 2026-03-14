#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define FP16_SIGN_MASK    0x8000
#define FP16_EXP_MASK     0x7C00
#define FP16_FRAC_MASK    0x03FF
#define FP16_PRE_FRAC_MASK 0X3FF000
#define FP16_G_MASK 0x0800
#define FP16_R_MASK 0x0400
#define FP16_S_MASK 0x03FF

#define FP16_EXP_SHIFT  10
#define FP16_BIAS       15
#define FP16_MAX_EXP    31

typedef struct {
    uint16_t sign;
    int16_t  exp_unbiased;
    uint16_t mantissa_11bit;
    uint16_t frac_10bit;
    uint16_t      type;
} FP16Unpacked;

#define TYPE_NORMAL    0
#define TYPE_SUBNORMAL 1
#define TYPE_ZERO      2
#define TYPE_INF       3
#define TYPE_NAN       4

