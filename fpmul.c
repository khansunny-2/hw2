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
    // printf("am i in?");
    uint16_t sign = u1.sign ^ u2.sign;
    int16_t exp = u1.exp_unbiased + u2.exp_unbiased;
    uint32_t Raw = (int32_t)u1.mantissa_11bit * u2.mantissa_11bit;
    printf("Raw: ");
    print_binary_22(Raw);
    // printf("\n");
    printf(" E_raw=%d\n", exp);

    int16_t shift;
    int16_t count = 0 ;
    for(int16_t i = 21 ; i >= 0 ; i--){
        uint16_t a = Raw >> i;
        if (a != 1){
            count++;
            // printf("count = %d", count);
        }else{
            break;
        }
    }
    //count=0 +1 count = 1 +0 ....
    shift = 1 - count;

    exp += shift;
    // printf("exp = %d", exp);

    if (exp<=-14){
        shift -= (-14-exp);
        shift +=2;
        exp = -14;

        uint16_t fractionforsubnormal = ((Raw & FP16_PRE_FRAC_MASK)>>(12-shift));
        uint16_t G = ((Raw >> (12 - shift - 1)) & 1)  ? 1:0;
        uint16_t R = ((Raw >> (12 - shift - 2)) & 1) ? 1:0;
        uint16_t S;
        uint32_t a =0;
        for(int16_t i = 12 - shift -3; i>=0; i--){
            uint16_t popped = (Raw>>i)&1 ;
            a = a*2 + popped;
        }
        if (a){
            S = 1;
        }else{
            S = 0;
        }
        uint16_t Inexact = G || R || S;

        printf("Norm: E_norm=-14");
        printf(" Fraction = ");
        print_binary_10(fractionforsubnormal);
        if((Inexact != 0)&&(sign == 0)){
            fractionforsubnormal += 1;
        }
        if(sign == 1){
            Inexact = 0;
        }

        printf(" G=%d R=%d S=%d Action=%s\n",
            G, R, S, Inexact ? "Up" : "Truncate");
        uint16_t Result = (sign << 15) + fractionforsubnormal;
        
        printf("Result: %04x\n", Result);
        return;
    }

    
    uint32_t pre_fraction = ((Raw << (count + 1)) & FP16_PRE_FRAC_MASK) >> 12;
    // printf("%d",((Raw << (count + 1)) & FP16_PRE_FRAC_MASK));
    // print_binary_22((Raw << (count + 1))& FP16_PRE_FRAC_MASK);
    printf("Norm: E_norm=%d", exp);
    printf(" Fraction = ");
    print_binary_10(((Raw << (count + 1)) & FP16_PRE_FRAC_MASK)>>12);

    uint16_t G = (pre_fraction & FP16_G_MASK) ? 1:0;
    uint16_t R = (pre_fraction & FP16_R_MASK) ? 1:0;
    uint16_t S;
    if (Raw & FP16_S_MASK){
        S = 1;
    }else{
        S = 0;
    }
    uint16_t Inexact = G || R || S;

    if((Inexact != 0)&&(sign == 0)){
        pre_fraction += 1;
    }

    if(sign == 1){
        Inexact = 0;
    }

    uint16_t Result = ((exp + 15) << 10) + (sign << 15) + pre_fraction;
    if((Result >= 0x7bff)&&(sign == 0)){
        printf(" G=0 R=0 S=0 Action=Truncate\n");
        printf("Result: 7c00");
        return;
    }
    if((Result >= 0xfbff)){
        printf(" G=0 R=0 S=0 Action=Truncate\n");
        printf("Result: fbff");
        return;
    }

    
    printf(" G=%d R=%d S=%d Action=%s\n",
        G, R, S, Inexact ? "Up" : "Truncate");

    printf("Result: %04x\n", Result);
    return;
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