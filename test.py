#!/usr/bin/env python3
"""
FP16 Multiplication Simulator - Reference Implementation
用于自查的Python版本，不限制任何库
"""

import sys
import math

# Constants
FP16_SIGN_MASK = 0x8000
FP16_EXP_MASK = 0x7C00
FP16_FRAC_MASK = 0x03FF
FP16_EXP_SHIFT = 10
FP16_BIAS = 15
FP16_MAX_EXP = 31

TYPE_NORMAL = 0
TYPE_SUBNORMAL = 1
TYPE_ZERO = 2
TYPE_INF = 3
TYPE_NAN = 4

def print_binary_22(val):
    """Print 22-bit binary number"""
    return ''.join(str((val >> i) & 1) for i in range(21, -1, -1))

def print_binary_10(val):
    """Print 10-bit binary number"""
    return ''.join(str((val >> i) & 1) for i in range(9, -1, -1))

def unpack_fp16(raw):
    """Unpack FP16 number"""
    sign = (raw & FP16_SIGN_MASK) >> 15
    exp_raw = (raw & FP16_EXP_MASK) >> FP16_EXP_SHIFT
    frac = raw & FP16_FRAC_MASK
    
    if exp_raw == FP16_MAX_EXP:
        exp_unbiased = 0
        if frac != 0:
            type_val = TYPE_NAN
            mantissa = 0x400 | frac
        else:
            type_val = TYPE_INF
            mantissa = 0x400
    elif exp_raw == 0:
        exp_unbiased = -14
        if frac == 0:
            type_val = TYPE_ZERO
            mantissa = 0
        else:
            type_val = TYPE_SUBNORMAL
            mantissa = frac
    else:
        exp_unbiased = exp_raw - FP16_BIAS
        type_val = TYPE_NORMAL
        mantissa = 0x400 | frac
    
    return {
        'sign': sign,
        'exp_unbiased': exp_unbiased,
        'mantissa': mantissa,
        'frac': frac,
        'type': type_val
    }

def get_type_str(type_val):
    """Convert type to string"""
    types = ['normal', 'subnormal', 'zero', 'inf', 'nan']
    return types[type_val]

def print_unpack(label, u):
    """Print unpacked information"""
    # Format fraction as 3 hex digits
    frac_hex = format(u['frac'] * 4, '03x')
    print(f"{label}: S={u['sign']} E={u['exp_unbiased']} "
          f"M={u['mantissa'] >> 10}.{frac_hex} {get_type_str(u['type'])}")

def get_grs_from_raw(Raw, target_pos):
    """
    Extract G, R, S bits from Raw product
    target_pos: position of the last bit we keep (0-indexed from LSB)
    """
    G = (Raw >> target_pos) & 1
    R = (Raw >> (target_pos - 1)) & 1 if target_pos > 0 else 0
    
    # S is OR of all bits below target_pos-1
    S = 0
    for i in range(target_pos - 2, -1, -1):
        if (Raw >> i) & 1:
            S = 1
            break
    
    return G, R, S

def round_fraction(fraction, G, R, S, sign):
    """
    Round according to Round-to-+∞ mode
    Returns (rounded_fraction, carry)
    """
    Inexact = G | R | S
    
    if not Inexact:
        return fraction, 0
    
    if sign == 0:  # Positive - round up
        fraction += 1
        if fraction >= 0x400:  # Carry to exponent
            return 0, 1
        return fraction, 0
    else:  # Negative - truncate
        return fraction, 0

def multiply_fp16(op1_raw, op2_raw):
    """Main multiplication function"""
    u1 = unpack_fp16(op1_raw)
    u2 = unpack_fp16(op2_raw)
    
    print_unpack("Op1", u1)
    print_unpack("Op2", u2)
    
    # Handle special cases
    if u1['type'] == TYPE_NAN or u2['type'] == TYPE_NAN:
        print("Raw: N/A")
        print("Norm: N/A")
        print("Result: N/A")
        return
    
    if (u1['type'] == TYPE_ZERO and u2['type'] == TYPE_INF) or \
       (u1['type'] == TYPE_INF and u2['type'] == TYPE_ZERO):
        print("Raw: N/A")
        print("Norm: N/A")
        print("Result: N/A")
        return
    
    if (u1['type'] == TYPE_INF and u2['type'] != TYPE_ZERO) or \
       (u2['type'] == TYPE_INF and u1['type'] != TYPE_ZERO):
        print("Raw: N/A")
        print("Norm: N/A")
        sign = u1['sign'] ^ u2['sign']
        print(f"Result: {sign << 15 | FP16_EXP_MASK:04x}")
        return
    
    if u1['type'] == TYPE_ZERO or u2['type'] == TYPE_ZERO:
        raw_exp = u1['exp_unbiased'] + u2['exp_unbiased']
        print(f"Raw: {print_binary_22(0)} E_raw={raw_exp}")
        print(f"Norm: E_norm=-14 Fraction=0000000000 G=0 R=0 S=0 Action=Truncate")
        sign = u1['sign'] ^ u2['sign']
        print(f"Result: {sign << 15:04x}")
        return
    
    # Normal multiplication
    sign = u1['sign'] ^ u2['sign']
    exp = u1['exp_unbiased'] + u2['exp_unbiased']
    Raw = u1['mantissa'] * u2['mantissa']
    
    print(f"Raw: {print_binary_22(Raw)} E_raw={exp}")
    
    # Find leading 1 position
    if Raw == 0:
        # Should not happen with normal numbers
        print("Norm: E_norm=-14 Fraction=0000000000 G=0 R=0 S=0 Action=Truncate")
        print(f"Result: {sign << 15:04x}")
        return
    
    # Count leading zeros
    lz = 0
    for i in range(21, -1, -1):
        if (Raw >> i) & 1:
            break
        lz += 1
    
    # Normalize: shift so that bit21 becomes 1
    # After multiplication, Raw is 22 bits: [21:20] integer part, [19:0] fraction
    # We want it in format: 01.xxx (bit21=0, bit20=1) or 1x.xxx
    if lz == 0:  # First bit is 1 (value >= 2)
        # Need to shift right by 1 to get format 01.xxx
        shift = -1  # Actually right shift
        exp += 1
        # Extract fraction after right shift
        # After right shift by 1, fraction is bits [20:11]
        fraction = (Raw >> 11) & 0x3FF
        G, R, S = get_grs_from_raw(Raw, 10)  # G is bit10 after right shift
    else:
        # Need to shift left to get bit20=1
        shift = lz - 1
        exp -= shift
        # After left shift, fraction is bits [20:11]
        shifted = Raw << shift
        fraction = (shifted >> 11) & 0x3FF
        # For GRS, we need to look at the bits that were shifted out
        # The bits we keep are [21:11] after shift
        # G is bit10, R is bit9, S is bits [8:0]
        G = (shifted >> 10) & 1
        R = (shifted >> 9) & 1
        S = (shifted & 0x1FF) != 0
    
    Inexact = G | R | S
    
    # Check for underflow to subnormal
    if exp < -14:
        # Need to right shift to get exponent to -14
        underflow_shift = -14 - exp
        
        # Right shift the Raw product
        shifted_raw = Raw >> underflow_shift
        
        # Extract fraction (now subnormal, so no leading 1)
        fraction = shifted_raw & 0x3FF
        
        # Get GRS from the shifted out bits
        if underflow_shift > 0:
            G = (Raw >> (underflow_shift - 1)) & 1
            R = (Raw >> (underflow_shift - 2)) & 1 if underflow_shift > 1 else 0
            
            # S is OR of all bits below
            S = 0
            for i in range(underflow_shift - 3, -1, -1):
                if (Raw >> i) & 1:
                    S = 1
                    break
        else:
            G, R, S = 0, 0, 0
        
        Inexact = G | R | S
        exp = -14
        
        # Round
        action = "Up" if (Inexact and sign == 0) else "Truncate"
        if Inexact and sign == 0:
            fraction += 1
            if fraction >= 0x400:
                # Rounded up to normal
                fraction = 0
                exp = -13
        
        print(f"Norm: E_norm={exp} Fraction={print_binary_10(fraction)} "
              f"G={G} R={R} S={S} Action={action}")
        
        if exp == -13:  # Became normal
            result = (sign << 15) | ((exp + 15) << 10) | fraction
        else:  # Still subnormal
            result = (sign << 15) | fraction
        
        print(f"Result: {result:04x}")
        return
    
    # Check for overflow
    if exp > 15:
        if sign == 0:
            print("Norm: E_norm=16 Fraction=0000000000 G=0 R=0 S=0 Action=Truncate")
            print("Result: 7c00")  # +inf
        else:
            print("Norm: E_norm=16 Fraction=1111111111 G=0 R=0 S=0 Action=Truncate")
            print("Result: fbff")  # -maxnormal
        return
    
    # Normal case
    action = "Up" if (Inexact and sign == 0) else "Truncate"
    
    # Apply rounding
    if Inexact and sign == 0:
        fraction += 1
        if fraction >= 0x400:
            fraction = 0
            exp += 1
    
    print(f"Norm: E_norm={exp} Fraction={print_binary_10(fraction)} "
          f"G={G} R={R} S={S} Action={action}")
    
    # Construct result
    result = (sign << 15) | ((exp + 15) << 10) | fraction
    print(f"Result: {result:04x}")

def main():
    """Main function"""
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        
        parts = line.split()
        if len(parts) != 2:
            continue
        
        try:
            op1 = int(parts[0], 16)
            op2 = int(parts[1], 16)
            multiply_fp16(op1, op2)
        except ValueError:
            continue

if __name__ == "__main__":
    main()