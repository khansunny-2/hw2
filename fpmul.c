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

static void print_binary_22(uint32_t val) {
    for (int16_t i = 21; i >= 0; i--) {
        printf("%d", (val >> i) & 1);
    }
}

static void print_binary_10(uint32_t val) {
    for (int i = 9; i >= 0; i--) {
        printf("%d", (val >> i) & 1);
    }
}

static void print_unpack(const char *label, FP16Unpacked *u) {
    const char *type_str;
    switch (u->type) {
        case TYPE_NORMAL: type_str = "normal"; break;
        case TYPE_SUBNORMAL: type_str = "subnormal"; break;
        case TYPE_ZERO: type_str = "zero"; break;
        case TYPE_INF: type_str = "inf"; break;
        case TYPE_NAN: type_str = "nan"; break;
        default: type_str = "unknown"; break;
    }
    uint16_t hx[3];
    uint16_t frac = u->frac_10bit * 4;
    for(uint16_t i = 0 ; i<= 2 ; i++){
        uint16_t popped = frac & 15;
        frac >>= 4;
        hx[i] = popped;
    }


    printf("%s: S=%d E=%d M=%d.%x%x%x %s\n",
           label,
           u->sign,
           u->exp_unbiased,
           u->mantissa_11bit >> 10,
            // (u->mantissa_11bit & 0x3FF) * 256 / 1024,
            // (u->mantissa_11bit & 0x3FF),
            hx[2] ,
            hx[1] ,
            hx[0] ,
           type_str);
}

static FP16Unpacked unpack_fp16(uint16_t raw) {
    FP16Unpacked u;
    uint16_t exp_raw;

    u.sign = (raw & FP16_SIGN_MASK) >> 15;
    exp_raw = (raw & FP16_EXP_MASK) >> FP16_EXP_SHIFT;
    u.frac_10bit = raw & FP16_FRAC_MASK;

    if (exp_raw == FP16_MAX_EXP) {
        u.exp_unbiased = 0;
        if (u.frac_10bit != 0) {
            u.type = TYPE_NAN;
            u.mantissa_11bit = 0x400 | u.frac_10bit;
        } else {
            u.type = TYPE_INF;
            u.mantissa_11bit = 0x400;
        }
    } else if (exp_raw == 0) {
        u.exp_unbiased = -14;
        if (u.frac_10bit == 0) {
            u.type = TYPE_ZERO;
            u.mantissa_11bit = 0;
        } else {
            u.type = TYPE_SUBNORMAL;
            u.mantissa_11bit = u.frac_10bit;
        }
    } else {
        u.exp_unbiased = (int16_t)exp_raw - FP16_BIAS;
        u.type = TYPE_NORMAL;
        u.mantissa_11bit = 0x400 | u.frac_10bit;
    }

    return u;
}

// static void normalization(uint16_t Raw , int16_t *exp){

    


// }


static void normal_and_subnormal(FP16Unpacked u1, FP16Unpacked u2){
    uint16_t sign = u1.sign ^ u2.sign;
    int16_t exp = u1.exp_unbiased + u2.exp_unbiased;
    uint32_t Raw = (uint32_t)u1.mantissa_11bit * u2.mantissa_11bit;
    printf("Raw: ");
    print_binary_22(Raw);
    printf(" E_raw=%d\n", exp);

    // count = number of leading zeros in 22-bit representation
    int16_t count = 0;
    for(int16_t i = 21; i >= 0; i--){
        if ((Raw >> i) & 1) break;
        count++;
    }
    // shift to normalize: MSB goes to bit 22 (hidden bit above bit 21)
    // left shift by (count + 1) puts MSB at bit 22
    // exp adjustment: shift = 1 - count
    int16_t shift = 1 - count;
    exp += shift;

    if (exp < -14){
        // Subnormal result: need to align Raw so that E_norm = -14
        // Normal path would left-shift by (count+1). For subnormal at E=-14,
        // we need additional right-shift of (-14 - exp) from the normalized position.
        // Net left shift = (count + 1) - (-14 - exp) = (count + 1) + 14 + exp
        int16_t net_left = (count + 1) + 14 + exp;

        exp = -14;

        uint32_t shifted;
        uint16_t S = 0;
        if (net_left >= 0) {
            shifted = Raw << net_left;
        } else {
            int16_t rshift = -net_left;
            uint32_t mask = (rshift < 32) ? ((1u << rshift) - 1) : 0xFFFFFFFF;
            if (Raw & mask) S = 1;
            shifted = Raw >> rshift;
        }

        uint32_t fractionforsubnormal = (shifted >> 12) & 0x3FF;
        uint16_t G = (shifted >> 11) & 1;
        uint16_t R = (shifted >> 10) & 1;
        if (!S) {
            if (shifted & 0x3FF) S = 1;
        }

        uint16_t Inexact = G | R | S;

        printf("Norm: E_norm=-14");
        printf(" Fraction=");
        print_binary_10(fractionforsubnormal);

        printf(" G=%d R=%d S=%d Action=%s\n",
            G, R, S, (Inexact && sign == 0) ? "Up" : "Truncate");

        if (Inexact && sign == 0) {
            fractionforsubnormal += 1;
        }

        uint16_t Result = (sign << 15) + fractionforsubnormal;
        printf("Result: %04x\n", Result);
        return;
    }

    // Normal path: left-shift Raw by (count+1) to put MSB at bit 22
    uint32_t shifted_raw = Raw << (count + 1);
    uint32_t pre_fraction = (shifted_raw & FP16_PRE_FRAC_MASK) >> 12;

    printf("Norm: E_norm=%d", exp);
    printf(" Fraction=");
    print_binary_10(pre_fraction);

    uint16_t G = (shifted_raw >> 11) & 1;
    uint16_t R = (shifted_raw >> 10) & 1;
    uint16_t S = (shifted_raw & 0x3FF) ? 1 : 0;

    uint16_t Inexact = G | R | S;

    // Print GRS and Action before rounding
    printf(" G=%d R=%d S=%d Action=%s\n",
        G, R, S, (Inexact && sign == 0) ? "Up" : "Truncate");

    if (Inexact && sign == 0) {
        pre_fraction += 1;

        if (pre_fraction >= 0x400) {
            pre_fraction = 0;
            exp += 1;
        }
    }

    // Overflow check
    if (exp > 15) {
        if (sign == 0) {
            printf("Result: 7c00\n");
        } else {
            printf("Result: fbff\n");
        }
        return;
    }

    uint16_t Result = ((exp + 15) << 10) + (sign << 15) + pre_fraction;

    // Final overflow guard
    if (sign == 0 && Result >= 0x7c00) {
        printf("Result: 7c00\n");
        return;
    }
    if (sign == 1 && Result > 0xfbff) {
        printf("Result: fbff\n");
        return;
    }

    printf("Result: %04x\n", Result);
}

static void multiply_fp16(uint16_t op1_raw, uint16_t op2_raw) {
    FP16Unpacked u1 = unpack_fp16(op1_raw);
    FP16Unpacked u2 = unpack_fp16(op2_raw);

    print_unpack("Op1", &u1);
    print_unpack("Op2", &u2);

    if (u1.type == TYPE_NAN || u2.type == TYPE_NAN) {
        printf("Raw: N/A\n");
        printf("Norm: N/A\n");
        printf("Result: N/A\n");
        return;
    }

    if ((u1.type == TYPE_ZERO && u2.type == TYPE_INF) ||
        (u1.type == TYPE_INF && u2.type == TYPE_ZERO)) {
        printf("Raw: N/A\n");
        printf("Norm: N/A\n");
        printf("Result: N/A\n");
        return;
    }

    if ((u1.type == TYPE_INF && u2.type != TYPE_ZERO) ||
        (u2.type == TYPE_INF && u1.type != TYPE_ZERO)) {
        printf("Raw: N/A\n");
        printf("Norm: N/A\n");
        uint16_t sign = u1.sign ^ u2.sign;
        printf("Result: %04x\n", (sign << 15) | FP16_EXP_MASK);
        return;
    }

    if (u1.type == TYPE_ZERO || u2.type == TYPE_ZERO) {
        int raw_exp = u1.exp_unbiased + u2.exp_unbiased;
        printf("Raw: ");
        print_binary_22(0);
        printf(" E_raw=%d\n", raw_exp);
        printf("Norm: E_norm=%d Fraction=0000000000 G=0 R=0 S=0 Action=Truncate\n",
               -14);
        uint16_t sign = u1.sign ^ u2.sign;
        printf("Result: %04x\n", sign << 15);
        return;
    }

    normal_and_subnormal(u1,u2);
    return;

}


int main(void) {
    char line[256];
    uint16_t op1, op2;

    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        if (line[0] == '\0') continue;

        if (sscanf(line, "%hx %hx", &op1, &op2) == 2) {
            multiply_fp16(op1, op2);
        }
    }

    return 0;
}