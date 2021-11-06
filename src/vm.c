#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/vm.h"
#include "../include/debug.h"
#include "../include/display.h"

#define PORT_ADDR 0xFF00

/* Load 16 bit data into an R16 Register */
#define LOAD_RR_D16(vm, RR) set_reg16(vm, RR, READ_16BIT(vm))
/* Load contents of R8 Register into address at R16 register (dereferencing) */
#define LOAD_ARR_R(vm, RR, R) writeAddr(vm, get_reg16(vm, RR), vm->GPR[R])
/* Load 8 bit data into R8 Register */
#define LOAD_R_D8(vm, R) vm->GPR[R] = READ_BYTE(vm)
/* Dereference the address contained in the R16 register and set it's value 
 * to the R8 register */
#define LOAD_R_ARR(vm, R, RR) vm->GPR[R] = vm->MEM[get_reg16(vm, RR)]
/* Load 8 bit data into address at R16 register (dereferencing) */
#define LOAD_ARR_D8(vm, RR) vm->MEM[get_reg16(vm, RR)] = READ_BYTE(vm)
/* Load contents of R8 register into another R8 register */
#define LOAD_R_R(vm, R1, R2) vm->GPR[R1] = vm->GPR[R2]
/* Load contents of R16 register into another R16 register */
#define LOAD_RR_RR(vm, RR1, RR2) set_reg16(vm, RR1, get_reg16(vm, RR2))
/* Load instructions from reading into and writing into main memory */
#define LOAD_MEM_R(vm, R) writeAddr(vm, READ_16BIT(vm), vm->GPR[R])
/* Load what's at the address specified by the 16 bit data into the R8 register */
#define LOAD_R_MEM(vm, R) vm->GPR[R] = vm->MEM[READ_16BIT(vm)]
/* Load 'R' into '(PORT_ADDR + D8)' */
#define LOAD_R_D8PORT(vm, R) writeAddr(vm, PORT_ADDR + READ_BYTE(vm), vm->GPR[R])
/* Load '(PORT_ADDR + D8) into 'R' */
#define LOAD_D8PORT_R(vm, R) vm->GPR[R] = vm->MEM[PORT_ADDR + READ_BYTE(vm)]
/* Load 'R1' into '(PORT_ADDR + R2)' */
#define LOAD_RPORT_R(vm, R1, R2) writeAddr(vm, PORT_ADDR + vm->GPR[R2], vm->GPR[R1])
/* Load '(PORT_ADDR + R2)' into 'R1' */
#define LOAD_R_RPORT(vm, R1, R2) vm->GPR[R1] = vm->MEM[PORT_ADDR + vm->GPR[R2]]

/* Increment contents of R16 register */
#define INC_RR(vm, RR) set_reg16(vm, RR, (get_reg16(vm, RR) + 1))
/* Decrement contents of R16 register */
#define DEC_RR(vm, RR) set_reg16(vm, RR, (get_reg16(vm, RR) - 1)) 

/* Direct Jump */
#define JUMP(vm, a16) vm->PC = a16
/* Relative Jump */
#define JUMP_RL(vm, i8) vm->PC += (int8_t)i8

#define PUSH_R16(vm, RR) push16(vm, get_reg16(vm, RR))
#define POP_R16(vm, RR) set_reg16(vm, RR, pop16(vm))
#define RST(vm, a16) call(vm, a16)
#define INTERRUPT_MASTER_ENABLE(vm) vm->IME = true
#define INTERRUPT_MASTER_DISABLE(vm) vm->IME = false
#define CCF(vm) set_flag(vm, FLAG_C, get_flag(vm, FLAG_C) ^ 1)
/* Flag utility macros */
#define TEST_Z_FLAG(vm, r) set_flag(vm, FLAG_Z, r == 0 ? 1 : 0)
/* We check if a carry over for the last 4 bits happened 
 * ref : https://www.reddit.com/r/EmuDev/comments/4ycoix/a_guide_to_the_gameboys_halfcarry_flag*/
#define TEST_H_FLAG_ADD(vm, x, y) set_flag(vm, FLAG_H, \
                                        ((x & 0xf) + (y & 0xf) > 0xf) ? 1 : 0)
#define TEST_H_FLAG_SUB(vm, x, y) set_flag(vm, FLAG_H, \
                                        (((int)(x & 0xf) - (int)(y & 0xf) < 0) ? 1 : 0))

/* Test for integer overloads and set carry flags */
#define TEST_C_FLAG_ADD16(vm, x, y) set_flag(vm, FLAG_C, (uint32_t)(x) + (uint32_t)(y) > 0xFFFF ? 1 : 0)
#define TEST_C_FLAG_ADD8(vm, x, y) set_flag(vm, FLAG_C, (uint16_t)(x) + (uint16_t)(y) > 0xFF ? 1 : 0)
/* Works for both 8 bit and 16 bit */
#define TEST_C_FLAG_SUB(vm, x, y) set_flag(vm, FLAG_C, (int32_t)(x) - (int32_t)(y) < 0 ? 1 : 0)

static inline void writeAddr(VM* vm, uint16_t addr, uint8_t byte);

static inline void set_flag(VM* vm, FLAG flag, uint8_t bit) {
    /* Since the flags are enums and in order, their numeric value
     * can be used to decide which bit to modify
     * We set the flag's corresponding bit to 1 */
    if (bit) vm->GPR[R8_F] |= 1 << (flag + 4);
    /* Otherwise 0 */
    else vm->GPR[R8_F] &= ~(1 << (flag + 4));
}

static inline uint8_t get_flag(VM* vm, FLAG flag) {
    return (vm->GPR[R8_F] >> (flag + 4)) & 1;
}

/* Retrives the value of the 16 bit registers by combining the values
 * of its 8 bit registers */
static inline uint16_t get_reg16(VM* vm, GP_REG RR) {
    return ((vm->GPR[RR] << 8) | vm->GPR[RR + 1]);
}

/* Sets a 16 bit register by modifying the individual 8 bit registers
 * it consists of */
static inline uint16_t set_reg16(VM* vm, GP_REG RR, uint16_t v) {
    vm->GPR[RR] = v >> 8; 
    vm->GPR[RR + 1] = v & 0xFF;

    return v;
}

static void resetGBC(VM* vm) {
    vm->PC = 0x0100;
    set_reg16(vm, R16_SP, 0xFFFE);
    vm->GPR[R8_A] = 0x11;
    vm->GPR[R8_F] = 0b10000000;
    vm->GPR[R8_B] = 0x00;
    vm->GPR[R8_C] = 0x00;
    vm->GPR[R8_D] = 0xFF;
    vm->GPR[R8_E] = 0x56;
    vm->GPR[R8_H] = 0x00;
    vm->GPR[R8_L] = 0x0D;
}

void initVM(VM* vm) {
    vm->cartridge = NULL;
    vm->conditionFalse = false;
    /* Set registers & flags to GBC specifics */
    resetGBC(vm);
}

void freeVM(VM* vm) {
    if (vm->cartridge != NULL) {
        freeCartridge(vm->cartridge);
    }
    resetGBC(vm);
}

static void incrementR8(VM* vm, GP_REG R) {
    /* A utility function which does incrementing for 8 bit registers
     * it does all flag updates it needs to */
    uint8_t old = vm->GPR[R]; uint8_t new = ++vm->GPR[R];
    TEST_Z_FLAG(vm, new);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, new);
}

static void decrementR8(VM* vm, GP_REG R) {
    /* Decrementing for 8 bit registers */
    uint8_t old = vm->GPR[R]; uint8_t new = --vm->GPR[R];
    TEST_Z_FLAG(vm, new);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, new);
}

/* The folloing functions are used for all 8 bit rotation 
 * operations */ 

static void rotateLeft(VM* vm, GP_REG R, bool setZFlag) {
    uint8_t toModify = vm->GPR[R];
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= bit7;

    vm->GPR[R] = toModify;
    if (setZFlag) {
        TEST_Z_FLAG(vm, toModify);
    } else {
        set_flag(vm, FLAG_Z, 0);
    }
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit7);  
}

static void rotateLeftAR16(VM* vm, GP_REG R16, bool setZFlag) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t toModify = vm->MEM[addr];
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= bit7;

    writeAddr(vm, addr, toModify);
    if (setZFlag) {
        TEST_Z_FLAG(vm, toModify);
    } else {
        set_flag(vm, FLAG_Z, 0);
    }
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit7);  
}

static void rotateRight(VM* vm, GP_REG R, bool setZFlag) {
    uint8_t toModify = vm->GPR[R];
    uint8_t bit1 = toModify & 1;

    toModify >>= 1;
    toModify |= bit1 << 7;

    vm->GPR[R] = toModify;

    if (setZFlag) {
        TEST_Z_FLAG(vm, toModify);
    } else {
        set_flag(vm, FLAG_Z, 0);
    }
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit1); 
}

static void rotateRightAR16(VM* vm, GP_REG R16, bool setZFlag) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t toModify = vm->MEM[addr];
    uint8_t bit1 = toModify & 1;

    toModify >>= 1;
    toModify |= bit1 << 7;

    writeAddr(vm, addr, toModify);

    if (setZFlag) {
        TEST_Z_FLAG(vm, toModify);
    } else {
        set_flag(vm, FLAG_Z, 0);
    }
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit1); 
}

static void shiftLeftArithmeticR8(VM* vm, GP_REG R) {
    uint8_t value = vm->GPR[R];
    uint8_t bit7 = value >> 7;
    uint8_t result = value << 1;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit7);
}

static void shiftLeftArithmeticAR16(VM* vm, GP_REG R16) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t value = vm->MEM[addr];
    uint8_t bit7 = value >> 7;
    uint8_t result = value << 1;

    writeAddr(vm, addr, result);

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit7);
}

static void shiftRightLogicalR8(VM* vm, GP_REG R) {
    uint8_t value = vm->GPR[R];
    uint8_t bit1 = value & 0x1;
    uint8_t result = value >> 1;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit1);
}

static void shiftRightLogicalAR16(VM* vm, GP_REG R16) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t value = vm->MEM[addr];
    uint8_t bit1 = value & 0x1;
    uint8_t result = value >> 1;

    writeAddr(vm, addr, result);

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit1);
}

static void shiftRightArithmeticR8(VM* vm, GP_REG R) {
    uint8_t value = vm->GPR[R];
    uint8_t bit7 = value >> 7;
    uint8_t result = value >> 1;
    
    /* Copy the 7th bit to its original location after the shift */
    result |= bit7 << 7;
    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, 0);
}

static void shiftRightArithmeticAR16(VM* vm, GP_REG R16) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t value = vm->MEM[addr]; 
    uint8_t bit7 = value >> 7;
    uint8_t result = value >> 1;
    
    /* Copy the 7th bit to its original location after the shift */
    result |= bit7 << 7;
    writeAddr(vm, addr, result);

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, 0);
}

static void swapR8(VM* vm, GP_REG R8) {
     uint8_t value = vm->GPR[R8];
     uint8_t highNibble = value >> 4;
     uint8_t lowNibble = value & 0xF;

     uint8_t newValue = (lowNibble << 4) | highNibble;

     vm->GPR[R8] = newValue;

     TEST_Z_FLAG(vm, newValue);
     set_flag(vm, FLAG_H, 0);
     set_flag(vm, FLAG_N, 0);
     set_flag(vm, FLAG_C, 0);
}

static void swapAR16(VM* vm, GP_REG R16) {
     uint16_t addr = get_reg16(vm, R16);
     uint8_t value = vm->MEM[addr]; 
     uint8_t highNibble = value >> 4;
     uint8_t lowNibble = value & 0xF;

     uint8_t newValue = (lowNibble << 4) | highNibble;

     writeAddr(vm, addr, newValue);

     TEST_Z_FLAG(vm, newValue);
     set_flag(vm, FLAG_H, 0);
     set_flag(vm, FLAG_N, 0);
     set_flag(vm, FLAG_C, 0);
}

static void testBitR8(VM* vm, GP_REG R8, uint8_t bit) {
    uint8_t value = vm->GPR[R8];
    uint8_t bitValue = (value >> bit) & 0x1;

    TEST_Z_FLAG(vm, bitValue);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 1);
}

static void testBitAR16(VM* vm, GP_REG R16, uint8_t bit) {
    uint8_t value = vm->MEM[get_reg16(vm, R16)];
    uint8_t bitValue = (value >> bit) & 0x1;

    TEST_Z_FLAG(vm, bitValue);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 1);
}

static void setBitR8(VM* vm, GP_REG R8, uint8_t bit) {
    uint8_t value = vm->GPR[R8];
    uint8_t orValue = 0b10000000 >> bit;
    uint8_t result = value | orValue;

    vm->GPR[R8] = result;
}

static void setBitAR16(VM* vm, GP_REG R16, uint8_t bit) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t value = vm->MEM[addr];
    uint8_t orValue = 0b10000000 >> bit;
    uint8_t result = value | orValue;

    writeAddr(vm, addr, result);
}

static void resetBitR8(VM* vm, GP_REG R8, uint8_t bit) {
    uint8_t value = vm->GPR[R8];
    /* Converts 00010000 to 11101111 for the andValue when bit 3 has to be reset for ex */
    uint8_t andValue = ~(0b10000000 >> bit);
    uint8_t result = value & andValue;

    vm->GPR[R8] = result;
}

static void resetBitAR16(VM* vm, GP_REG R16, uint8_t bit) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t value = vm->MEM[addr];
    uint8_t andValue = ~(0b10000000 >> bit);
    uint8_t result = value & andValue;

    writeAddr(vm, addr, result);
}

/* The following functions form the most of the arithmetic and logical
 * operations of the CPU */

static void addR16(VM* vm, GP_REG RR1, GP_REG RR2) {
    uint16_t old = get_reg16(vm, RR1); 
    uint16_t toAdd = get_reg16(vm, RR2);
    uint16_t result = set_reg16(vm, RR1, old + toAdd);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, result);
    TEST_C_FLAG_ADD16(vm, old, toAdd);
}

/* Adding a signed 8 bit integer to a 16 bit register */

static void addR16I8(VM* vm, GP_REG RR1, GP_REG RR_STORE) {
    /* This function is a little bit special because it provides an extra parameter 
     * for a 16 bit register, which is where the final result is stored. 
     *
     * This comes in handy for another instruction */
    uint16_t old = get_reg16(vm, RR1); 
    int8_t toAdd = (int8_t)READ_BYTE(vm);
    uint16_t result = set_reg16(vm, RR_STORE, old + toAdd);
    set_flag(vm, FLAG_Z, 0);
    set_flag(vm, FLAG_N, 0);

    if (toAdd < 0) {
        /* Negative integer addition means, subtraction so we
         * do subtraction tests */
        TEST_H_FLAG_SUB(vm, old, result);
        TEST_C_FLAG_SUB(vm, old, -toAdd);
    } else {
        /* Normal addition with a number being 0 or greater */
        TEST_H_FLAG_ADD(vm, old, result);
        TEST_C_FLAG_ADD16(vm, old, toAdd);
    }
}

static void addR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t toAdd = vm->GPR[R2];
    uint8_t result = old + toAdd;

    vm->GPR[R1] = result;
    
    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, result);
    TEST_C_FLAG_ADD8(vm, old, toAdd);
}

static void addR8D8(VM* vm, GP_REG R) {
    uint8_t data = READ_BYTE(vm);
    uint8_t old = vm->GPR[R];
    uint8_t result = old + data;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, result);
    TEST_C_FLAG_ADD8(vm, old, data);
}

static void addR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t toAdd = vm->MEM[get_reg16(vm, R16)];
    uint8_t result = old + toAdd;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, result);
    TEST_C_FLAG_ADD8(vm, old, toAdd);
}

static void adcR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    /* To add is 16 bit so that in case if theres an integer overflow
     * when adding carry (when passing it to test C flag), it can be 
     * noticed as > 0xFF */
    uint16_t toAdd = vm->GPR[R2];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old + toAdd + carry;

    vm->GPR[R1] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, result);
    TEST_C_FLAG_ADD8(vm, old, toAdd + carry);
}

static void adcR8D8(VM* vm, GP_REG R) {
    uint16_t data = (uint16_t)READ_BYTE(vm);
    uint8_t old = vm->GPR[R];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old + data + carry;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, result);
    TEST_C_FLAG_ADD8(vm, old, data + carry);
}

static void adcR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t toAdd = vm->MEM[get_reg16(vm, R16)];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old + toAdd + carry;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, result);
    TEST_C_FLAG_ADD8(vm, old, toAdd + carry);
}

static void subR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t toSub = vm->GPR[R2];
    uint8_t result = old - toSub;

    vm->GPR[R1] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB(vm, old, toSub);
}

static void subR8D8(VM* vm, GP_REG R) {
    uint8_t data = READ_BYTE(vm);
    uint8_t old = vm->GPR[R];
    uint8_t result = old - data;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB(vm, old, data);
}

static void subR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t toSub = vm->MEM[get_reg16(vm, R16)];
    uint8_t result = old - toSub;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB(vm, old, toSub);
}

static void sbcR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    int32_t toSub = (int32_t)vm->GPR[R2];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old - toSub - carry;

    vm->GPR[R1] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB(vm, old, toSub - carry);
}

static void sbcR8D8(VM* vm, GP_REG R) {
    uint16_t data = (uint16_t)READ_BYTE(vm);
    uint8_t old = vm->GPR[R];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old - data - carry;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB(vm, old, data - carry);
}

static void sbcR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    int32_t toSub = vm->MEM[get_reg16(vm, R16)];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old - toSub - carry;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB(vm, old, toSub - carry);
}

static void andR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t operand = vm->GPR[R2];
    uint8_t result = old & operand;

    vm->GPR[R1] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 1);
    set_flag(vm, FLAG_C, 0);
}

static void andR8D8(VM* vm, GP_REG R) {
    uint8_t old = vm->GPR[R];
    uint8_t operand = READ_BYTE(vm);
    uint8_t result = old & operand;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 1);
    set_flag(vm, FLAG_C, 0);
}

static void andR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t operand = vm->MEM[get_reg16(vm, R16)];
    uint8_t result = old & operand;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 1);
    set_flag(vm, FLAG_C, 0);
}

static void xorR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t operand = vm->GPR[R2];
    uint8_t result = old ^ operand;

    vm->GPR[R1] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);
}

static void xorR8D8(VM* vm, GP_REG R) {
    uint8_t old = vm->GPR[R];
    uint8_t operand = READ_BYTE(vm);
    uint8_t result = old ^ operand;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);
}

static void xorR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t operand = vm->MEM[get_reg16(vm, R16)];
    uint8_t result = old ^ operand;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);
}

static void orR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t operand = vm->GPR[R2];
    uint8_t result = old | operand;

    vm->GPR[R1] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);   
}

static void orR8D8(VM* vm, GP_REG R) {
    uint8_t old = vm->GPR[R];
    uint8_t operand = READ_BYTE(vm);
    uint8_t result = old | operand;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);   
}

static void orR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t operand = vm->MEM[get_reg16(vm, R16)];
    uint8_t result = old | operand;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);
}

static void compareR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t operand1 = vm->GPR[R1];
    uint8_t operand2 = vm->GPR[R2];

    set_flag(vm, FLAG_Z, operand1 == operand2);
    set_flag(vm, FLAG_C, operand2 > operand1);
    set_flag(vm, FLAG_H, operand1 > operand2);
    set_flag(vm, FLAG_N, 1);
}

static void compareR8D8(VM* vm, GP_REG R) {
    uint8_t operand1 = vm->GPR[R];
    uint8_t operand2 = READ_BYTE(vm);

    set_flag(vm, FLAG_Z, operand1 == operand2);
    set_flag(vm, FLAG_C, operand2 > operand1);
    set_flag(vm, FLAG_H, operand1 > operand2);
    set_flag(vm, FLAG_N, 1);
}

static void compareR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t operand1 = vm->GPR[R8];
    uint8_t operand2 = vm->MEM[get_reg16(vm, R16)];

    set_flag(vm, FLAG_Z, operand1 == operand2);
    set_flag(vm, FLAG_C, operand2 > operand1);
    set_flag(vm, FLAG_H, operand1 > operand2);
    set_flag(vm, FLAG_N, 1);       
}

/* The following functions are responsible for returning, calling & stack manipulation*/

static inline void push16(VM* vm, uint16_t u16) {
    /* Pushes a 2 byte value to the stack */
    uint16_t stackPointer = get_reg16(vm, R16_SP);

    /* Write the high byte */
    writeAddr(vm, stackPointer - 1, u16 >> 8);
    /* Write the low byte */
    writeAddr(vm, stackPointer - 2, u16 & 0xFF);

    /* Update the stack pointer */
    set_reg16(vm, R16_SP, stackPointer - 2);
}

static inline uint16_t pop16(VM* vm) {
    uint16_t stackPointer = get_reg16(vm, R16_SP);
    uint8_t lowByte = vm->MEM[stackPointer];
    uint8_t highByte = vm->MEM[stackPointer + 1];

    set_reg16(vm, R16_SP, stackPointer + 2);

    return (uint16_t)((highByte << 8) | lowByte);
}

static void call(VM* vm) {
    uint16_t callAddress = READ_16BIT(vm);
    push16(vm, vm->PC);

    vm->PC = callAddress;
}

static void rst(VM* vm, uint16_t addr) {
    /* RST are calls to fixed addresses */
    push16(vm, vm->PC);
    vm->PC = addr;
}

static void callCondition(VM* vm, bool isTrue) {
    vm->conditionFalse = !isTrue;

    uint16_t callAddress = READ_16BIT(vm);

    if (isTrue) {
        push16(vm, vm->PC);
        vm->PC = callAddress;
    }
}

static inline void ret(VM* vm) {
    vm->PC = pop16(vm);
}

static void retCondition(VM* vm, bool isTrue) {
    vm->conditionFalse = !isTrue;

    if (isTrue) {
        vm->PC = pop16(vm);
    }
}

static void cpl(VM* vm) {
    vm->GPR[R8_A] = ~vm->GPR[R8_A];
    set_flag(vm, FLAG_N, 1);
    set_flag(vm, FLAG_H, 1);
}

/* Conditional Jumps (both relative and direct) */ 

#define CONDITION_NZ(vm) (get_flag(vm, FLAG_Z) != 0)
#define CONDITION_NC(vm) (get_flag(vm, FLAG_C) != 0)
#define CONDITION_Z(vm)  (get_flag(vm, FLAG_Z) == 0)
#define CONDITION_C(vm)  (get_flag(vm, FLAG_C) == 0)

static void jumpCondition(VM* vm, bool isTrue) {
    vm->conditionFalse = !isTrue;

    uint16_t address = READ_16BIT(vm);
    if (isTrue) vm->PC = address;
}

static void jumpRelativeCondition(VM* vm, bool isTrue) {
    vm->conditionFalse = !isTrue;
    
    int8_t jumpCount = (int8_t)READ_BYTE(vm);
    if (isTrue) vm->PC += jumpCount;
}

/* Procedure for the decimal adjust instruction (DAA) */

static void decimalAdjust(VM* vm) {
    uint8_t correction = 0;
    bool setFlagC = false;
    bool didHalfCarry = get_flag(vm, FLAG_H);
    bool wasSubtraction = get_flag(vm, FLAG_N);
    bool didCarry = get_flag(vm, FLAG_C);
    uint8_t currentValue = vm->GPR[R8_A];

    if (didHalfCarry || (!wasSubtraction && (currentValue & 0xF) > 0x9)) {
        correction |= 0x6;
    }

    if (didCarry || (!wasSubtraction && currentValue > 0x99)) {
        correction |= 0x66;
        setFlagC = true;
    }

    currentValue += wasSubtraction ? -correction : correction;
    currentValue &= 0xFF;

    vm->GPR[R8_A] = currentValue;

    set_flag(vm, FLAG_Z, currentValue == 0);
    set_flag(vm, FLAG_C, setFlagC);
    set_flag(vm, FLAG_H, 0);

    /* Refer to 'https://ehaskins.com/2018-01-30%20Z80%20DAA/' */
}

/* This function prevents any code from writing into 
 * read only areas */ 

static inline bool canWriteAddr(VM* vm, uint16_t addr) {
    return !((addr >= ROM_N0_16KB && addr <= ROM_NN_16KB_END) ||
           (addr >= ECHO_N0_8KB && addr <= UNUSABLE_N0_END));
}

/* This function is responsible for writing 1 byte to a memory address */

static inline void writeAddr(VM* vm, uint16_t addr, uint8_t byte) {
    if (!canWriteAddr(vm, addr)) {
        /* Pass over control to an MBC, maybe this is a call for 
         * bank switch 
         * TODO */
        printf("[WARNING] Attempt to write to address 0x%x (read only)\n", addr);
        return;
    }
    vm->MEM[addr] = byte; 
}

static void bootROM(VM* vm) {
    /* This is only a temporary boot rom function,
     * the original boot rom will be in binary and will
     * be mapped over correctly when the cpu is complete
     *
     *
     * The boot procedure is only minimal and is incomplete */

    uint8_t logo[0x30] = {0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D,
                          0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
                          0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08,
                          0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
                          0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD,
                          0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x64,
                          0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC,
                          0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E};
#ifndef DEBUG_NO_CARTRIDGE_VERIFICATION    
    bool logoVerified = memcmp(&vm->cartridge->logoChecksum, &logo, 0x18) == 0;
    
    if (!logoVerified) {
        log_fatal(vm, "Logo Verification Failed");
    }
    
    int checksum = 0;
    for (int i = 0x134; i <= 0x14C; i++) {
        checksum = checksum - vm->cartridge->allocated[i] - 1;
    }

    if ((checksum & 0xFF) != vm->cartridge->headerChecksum) {
        log_fatal(vm, "Header Checksum Doesn't Match, it is possibly corrupted");
    }
#endif   
    /* Map the cartridge rom to the GBC rom space 
     * occupying bank 0 and 1, a total of 32 KB*/
    memcpy(&vm->MEM[ROM_N0_16KB], vm->cartridge->allocated, 0x8000);   
}

static void prefixCB(VM* vm) {
    /* This function contains opcode interpretations for
     * all the instruction prefixed by opcode CB */
    uint8_t byte = READ_BYTE(vm);
    
#ifdef DEBUG_REALTIME_PRINTING
    print_
#endif

    switch (byte) {
        case 0x00: rotateLeft(vm, R8_B, true); break;
        case 0x01: rotateLeft(vm, R8_C, true); break;
        case 0x02: rotateLeft(vm, R8_D, true); break;
        case 0x03: rotateLeft(vm, R8_E, true); break;
        case 0x04: rotateLeft(vm, R8_H, true); break;
        case 0x05: rotateLeft(vm, R8_L, true); break;
        case 0x06: rotateLeftAR16(vm, R16_HL, true); break;
        case 0x07: rotateLeft(vm, R8_A, true); break;
        case 0x08: rotateRight(vm, R8_B, true); break;
        case 0x09: rotateRight(vm, R8_C, true); break;
        case 0x0A: rotateRight(vm, R8_D, true); break;
        case 0x0B: rotateRight(vm, R8_E, true); break;
        case 0x0C: rotateRight(vm, R8_H, true); break;
        case 0x0D: rotateRight(vm, R8_L, true); break;
        case 0x0E: rotateRightAR16(vm, R16_HL, true); break;
        case 0x0F: rotateRight(vm, R8_A, true); break;
        case 0x10: rotateLeft(vm, R8_B, true); break;
        case 0x11: rotateLeft(vm, R8_C, true); break;
        case 0x12: rotateLeft(vm, R8_D, true); break;
        case 0x13: rotateLeft(vm, R8_E, true); break;
        case 0x14: rotateLeft(vm, R8_H, true); break;
        case 0x15: rotateLeft(vm, R8_L, true); break;
        case 0x16: rotateLeftAR16(vm, R16_HL, true); break;
        case 0x17: rotateLeft(vm, R8_A, true); break;
        case 0x18: rotateRight(vm, R8_B, true); break;
        case 0x19: rotateRight(vm, R8_C, true); break;
        case 0x1A: rotateRight(vm, R8_D, true); break;
        case 0x1B: rotateRight(vm, R8_E, true); break;
        case 0x1C: rotateRight(vm, R8_H, true); break;
        case 0x1D: rotateRight(vm, R8_L, true); break;
        case 0x1E: rotateRightAR16(vm, R16_HL, true); break;
        case 0x1F: rotateRight(vm, R8_A, true); break;
        case 0x20: shiftLeftArithmeticR8(vm, R8_B); break;
        case 0x21: shiftLeftArithmeticR8(vm, R8_C); break;
        case 0x22: shiftLeftArithmeticR8(vm, R8_D); break;
        case 0x23: shiftLeftArithmeticR8(vm, R8_E); break;
        case 0x24: shiftLeftArithmeticR8(vm, R8_H); break;
        case 0x25: shiftLeftArithmeticR8(vm, R8_L); break;
        case 0x26: shiftLeftArithmeticAR16(vm, R16_HL); break;
        case 0x27: shiftLeftArithmeticR8(vm, R8_A); break;
        case 0x28: shiftRightArithmeticR8(vm, R8_B); break;
        case 0x29: shiftRightArithmeticR8(vm, R8_C); break;
        case 0x2A: shiftRightArithmeticR8(vm, R8_D); break;
        case 0x2B: shiftRightArithmeticR8(vm, R8_E); break;
        case 0x2C: shiftRightArithmeticR8(vm, R8_H); break;
        case 0x2D: shiftRightArithmeticR8(vm, R8_L); break;
        case 0x2E: shiftRightArithmeticAR16(vm, R16_HL); break;
        case 0x2F: shiftRightArithmeticR8(vm, R8_A); break;
        case 0x30: swapR8(vm, R8_B); break;
        case 0x31: swapR8(vm, R8_C); break;
        case 0x32: swapR8(vm, R8_D); break;
        case 0x33: swapR8(vm, R8_E); break;
        case 0x34: swapR8(vm, R8_H); break;
        case 0x35: swapR8(vm, R8_L); break;
        case 0x36: swapAR16(vm, R16_HL); break;
        case 0x37: swapR8(vm, R8_A); break;
        case 0x38: shiftRightLogicalR8(vm, R8_B); break;
        case 0x39: shiftRightLogicalR8(vm, R8_C); break;
        case 0x3A: shiftRightLogicalR8(vm, R8_D); break;
        case 0x3B: shiftRightLogicalR8(vm, R8_E); break;
        case 0x3C: shiftRightLogicalR8(vm, R8_H); break;
        case 0x3D: shiftRightLogicalR8(vm, R8_L); break;
        case 0x3E: shiftRightLogicalAR16(vm, R16_HL); break;
        case 0x3F: shiftRightLogicalR8(vm, R8_A); break;
        case 0x40: testBitR8(vm, R8_B, 0); break;
        case 0x41: testBitR8(vm, R8_C, 0); break;
        case 0x42: testBitR8(vm, R8_D, 0); break;
        case 0x43: testBitR8(vm, R8_E, 0); break;
        case 0x44: testBitR8(vm, R8_H, 0); break;
        case 0x45: testBitR8(vm, R8_L, 0); break;
        case 0x46: testBitAR16(vm, R16_HL, 0); break;
        case 0x47: testBitR8(vm, R8_A, 0); break;
        case 0x48: testBitR8(vm, R8_B, 1); break;
        case 0x49: testBitR8(vm, R8_C, 1); break;
        case 0x4A: testBitR8(vm, R8_D, 1); break;
        case 0x4B: testBitR8(vm, R8_E, 1); break;
        case 0x4C: testBitR8(vm, R8_H, 1); break;
        case 0x4D: testBitR8(vm, R8_L, 1); break;
        case 0x4E: testBitAR16(vm, R16_HL, 1); break;
        case 0x4F: testBitR8(vm, R8_A, 1); break;
        case 0x50: testBitR8(vm, R8_B, 2); break;
        case 0x51: testBitR8(vm, R8_C, 2); break;
        case 0x52: testBitR8(vm, R8_D, 2); break;
        case 0x53: testBitR8(vm, R8_E, 2); break;
        case 0x54: testBitR8(vm, R8_H, 2); break;
        case 0x55: testBitR8(vm, R8_L, 2); break;
        case 0x56: testBitAR16(vm, R16_HL, 2); break;
        case 0x57: testBitR8(vm, R8_A, 2); break;
        case 0x58: testBitR8(vm, R8_B, 3); break;
        case 0x59: testBitR8(vm, R8_C, 3); break;
        case 0x5A: testBitR8(vm, R8_D, 3); break;
        case 0x5B: testBitR8(vm, R8_E, 3); break;
        case 0x5C: testBitR8(vm, R8_H, 3); break;
        case 0x5D: testBitR8(vm, R8_L, 3); break;
        case 0x5E: testBitAR16(vm, R16_HL, 3); break;
        case 0x5F: testBitR8(vm, R8_A, 3); break;
        case 0x60: testBitR8(vm, R8_B, 4); break;
        case 0x61: testBitR8(vm, R8_C, 4); break;
        case 0x62: testBitR8(vm, R8_D, 4); break;
        case 0x63: testBitR8(vm, R8_E, 4); break;
        case 0x64: testBitR8(vm, R8_H, 4); break;
        case 0x65: testBitR8(vm, R8_L, 4); break;
        case 0x66: testBitAR16(vm, R16_HL, 4); break;
        case 0x67: testBitR8(vm, R8_A, 4); break;
        case 0x68: testBitR8(vm, R8_B, 5); break;
        case 0x69: testBitR8(vm, R8_C, 5); break;
        case 0x6A: testBitR8(vm, R8_D, 5); break;
        case 0x6B: testBitR8(vm, R8_E, 5); break;
        case 0x6C: testBitR8(vm, R8_H, 5); break;
        case 0x6D: testBitR8(vm, R8_L, 5); break;
        case 0x6E: testBitAR16(vm, R16_HL, 5); break;
        case 0x6F: testBitR8(vm, R8_A, 5); break;
        case 0x70: testBitR8(vm, R8_B, 6); break;
        case 0x71: testBitR8(vm, R8_C, 6); break;
        case 0x72: testBitR8(vm, R8_D, 6); break;
        case 0x73: testBitR8(vm, R8_E, 6); break;
        case 0x74: testBitR8(vm, R8_H, 6); break;
        case 0x75: testBitR8(vm, R8_L, 6); break;
        case 0x76: testBitAR16(vm, R16_HL, 6); break;
        case 0x77: testBitR8(vm, R8_A, 6); break;
        case 0x78: testBitR8(vm, R8_B, 7); break;
        case 0x79: testBitR8(vm, R8_C, 7); break;
        case 0x7A: testBitR8(vm, R8_D, 7); break;
        case 0x7B: testBitR8(vm, R8_E, 7); break;
        case 0x7C: testBitR8(vm, R8_H, 7); break;
        case 0x7D: testBitR8(vm, R8_L, 7); break;
        case 0x7E: testBitAR16(vm, R16_HL, 7); break;
        case 0x7F: testBitR8(vm, R8_A, 7); break;
        case 0x80: resetBitR8(vm, R8_B, 0); break;
        case 0x81: resetBitR8(vm, R8_C, 0); break;
        case 0x82: resetBitR8(vm, R8_D, 0); break;
        case 0x83: resetBitR8(vm, R8_E, 0); break;
        case 0x84: resetBitR8(vm, R8_H, 0); break;
        case 0x85: resetBitR8(vm, R8_L, 0); break;
        case 0x86: resetBitAR16(vm, R16_HL, 0); break;
        case 0x87: resetBitR8(vm, R8_A, 0); break;
        case 0x88: resetBitR8(vm, R8_B, 1); break;
        case 0x89: resetBitR8(vm, R8_C, 1); break;
        case 0x8A: resetBitR8(vm, R8_D, 1); break;
        case 0x8B: resetBitR8(vm, R8_E, 1); break;
        case 0x8C: resetBitR8(vm, R8_H, 1); break;
        case 0x8D: resetBitR8(vm, R8_L, 1); break;
        case 0x8E: resetBitAR16(vm, R16_HL, 1); break;
        case 0x8F: resetBitR8(vm, R8_A, 1); break;
        case 0x90: resetBitR8(vm, R8_B, 2); break;
        case 0x91: resetBitR8(vm, R8_C, 2); break;
        case 0x92: resetBitR8(vm, R8_D, 2); break;
        case 0x93: resetBitR8(vm, R8_E, 2); break;
        case 0x94: resetBitR8(vm, R8_H, 2); break;
        case 0x95: resetBitR8(vm, R8_L, 2); break;
        case 0x96: resetBitAR16(vm, R16_HL, 2); break;
        case 0x97: resetBitR8(vm, R8_A, 2); break;
        case 0x98: resetBitR8(vm, R8_B, 3); break;
        case 0x99: resetBitR8(vm, R8_C, 3); break;
        case 0x9A: resetBitR8(vm, R8_D, 3); break;
        case 0x9B: resetBitR8(vm, R8_E, 3); break;
        case 0x9C: resetBitR8(vm, R8_H, 3); break;
        case 0x9D: resetBitR8(vm, R8_L, 3); break;
        case 0x9E: resetBitAR16(vm, R16_HL, 3); break;
        case 0x9F: resetBitR8(vm, R8_A, 3); break;
        case 0xA0: resetBitR8(vm, R8_B, 4); break;
        case 0xA1: resetBitR8(vm, R8_C, 4); break;
        case 0xA2: resetBitR8(vm, R8_D, 4); break;
        case 0xA3: resetBitR8(vm, R8_E, 4); break;
        case 0xA4: resetBitR8(vm, R8_H, 4); break;
        case 0xA5: resetBitR8(vm, R8_L, 4); break;
        case 0xA6: resetBitAR16(vm, R16_HL, 4); break;
        case 0xA7: resetBitR8(vm, R8_A, 4); break;
        case 0xA8: resetBitR8(vm, R8_B, 5); break;
        case 0xA9: resetBitR8(vm, R8_C, 5); break;
        case 0xAA: resetBitR8(vm, R8_D, 5); break;
        case 0xAB: resetBitR8(vm, R8_E, 5); break;
        case 0xAC: resetBitR8(vm, R8_H, 5); break;
        case 0xAD: resetBitR8(vm, R8_L, 5); break;
        case 0xAE: resetBitAR16(vm, R16_HL, 5); break;
        case 0xAF: resetBitR8(vm, R8_A, 5); break;
        case 0xB0: resetBitR8(vm, R8_B, 6); break;
        case 0xB1: resetBitR8(vm, R8_C, 6); break;
        case 0xB2: resetBitR8(vm, R8_D, 6); break;
        case 0xB3: resetBitR8(vm, R8_E, 6); break;
        case 0xB4: resetBitR8(vm, R8_H, 6); break;
        case 0xB5: resetBitR8(vm, R8_L, 6); break;
        case 0xB6: resetBitAR16(vm, R16_HL, 6); break;
        case 0xB7: resetBitR8(vm, R8_A, 6); break;
        case 0xB8: resetBitR8(vm, R8_B, 7); break;
        case 0xB9: resetBitR8(vm, R8_C, 7); break;
        case 0xBA: resetBitR8(vm, R8_D, 7); break;
        case 0xBB: resetBitR8(vm, R8_E, 7); break;
        case 0xBC: resetBitR8(vm, R8_H, 7); break;
        case 0xBD: resetBitR8(vm, R8_L, 7); break;
        case 0xBE: resetBitAR16(vm, R16_HL, 7); break;
        case 0xBF: resetBitR8(vm, R8_A, 7); break;
        case 0xC0: setBitR8(vm, R8_B, 0); break;
        case 0xC1: setBitR8(vm, R8_C, 0); break;
        case 0xC2: setBitR8(vm, R8_D, 0); break;
        case 0xC3: setBitR8(vm, R8_E, 0); break;
        case 0xC4: setBitR8(vm, R8_H, 0); break;
        case 0xC5: setBitR8(vm, R8_L, 0); break;
        case 0xC6: setBitAR16(vm, R16_HL, 0); break;
        case 0xC7: setBitR8(vm, R8_A, 0); break;
        case 0xC8: setBitR8(vm, R8_B, 1); break;
        case 0xC9: setBitR8(vm, R8_C, 1); break;
        case 0xCA: setBitR8(vm, R8_D, 1); break;
        case 0xCB: setBitR8(vm, R8_E, 1); break;
        case 0xCC: setBitR8(vm, R8_H, 1); break;
        case 0xCD: setBitR8(vm, R8_L, 1); break;
        case 0xCE: setBitAR16(vm, R16_HL, 1); break;
        case 0xCF: setBitR8(vm, R8_A, 1); break;
        case 0xD0: setBitR8(vm, R8_B, 2); break;
        case 0xD1: setBitR8(vm, R8_C, 2); break;
        case 0xD2: setBitR8(vm, R8_D, 2); break;
        case 0xD3: setBitR8(vm, R8_E, 2); break;
        case 0xD4: setBitR8(vm, R8_H, 2); break;
        case 0xD5: setBitR8(vm, R8_L, 2); break;
        case 0xD6: setBitAR16(vm, R16_HL, 2); break;
        case 0xD7: setBitR8(vm, R8_A, 2); break;
        case 0xD8: setBitR8(vm, R8_B, 3); break;
        case 0xD9: setBitR8(vm, R8_C, 3); break;
        case 0xDA: setBitR8(vm, R8_D, 3); break;
        case 0xDB: setBitR8(vm, R8_E, 3); break;
        case 0xDC: setBitR8(vm, R8_H, 3); break;
        case 0xDD: setBitR8(vm, R8_L, 3); break;
        case 0xDE: setBitAR16(vm, R16_HL, 3); break;
        case 0xDF: setBitR8(vm, R8_A, 3); break;
        case 0xE0: setBitR8(vm, R8_B, 4); break;
        case 0xE1: setBitR8(vm, R8_C, 4); break;
        case 0xE2: setBitR8(vm, R8_D, 4); break;
        case 0xE3: setBitR8(vm, R8_E, 4); break;
        case 0xE4: setBitR8(vm, R8_H, 4); break;
        case 0xE5: setBitR8(vm, R8_L, 4); break;
        case 0xE6: setBitAR16(vm, R16_HL, 4); break;
        case 0xE7: setBitR8(vm, R8_A, 4); break;
        case 0xE8: setBitR8(vm, R8_B, 5); break;
        case 0xE9: setBitR8(vm, R8_C, 5); break;
        case 0xEA: setBitR8(vm, R8_D, 5); break;
        case 0xEB: setBitR8(vm, R8_E, 5); break;
        case 0xEC: setBitR8(vm, R8_H, 5); break;
        case 0xED: setBitR8(vm, R8_L, 5); break;
        case 0xEE: setBitAR16(vm, R16_HL, 5); break;
        case 0xEF: setBitR8(vm, R8_A, 5); break;
        case 0xF0: setBitR8(vm, R8_B, 6); break;
        case 0xF1: setBitR8(vm, R8_C, 6); break;
        case 0xF2: setBitR8(vm, R8_D, 6); break;
        case 0xF3: setBitR8(vm, R8_E, 6); break;
        case 0xF4: setBitR8(vm, R8_H, 6); break;
        case 0xF5: setBitR8(vm, R8_L, 6); break;
        case 0xF6: setBitAR16(vm, R16_HL, 6); break;
        case 0xF7: setBitR8(vm, R8_A, 6); break;
        case 0xF8: setBitR8(vm, R8_B, 7); break;
        case 0xF9: setBitR8(vm, R8_C, 7); break;
        case 0xFA: setBitR8(vm, R8_D, 7); break;
        case 0xFB: setBitR8(vm, R8_E, 7); break;
        case 0xFC: setBitR8(vm, R8_H, 7); break;
        case 0xFD: setBitR8(vm, R8_L, 7); break;
        case 0xFE: setBitAR16(vm, R16_HL, 7); break;
        case 0xFF: setBitR8(vm, R8_A, 7); break;
    }
}

void runVM(VM* vm) {
    for (;;) {
#ifdef DEBUG_REALTIME_PRINTING
        printInstruction(vm);
#endif
        uint8_t byte = READ_BYTE(vm);
        switch (byte) {
            // nop
            case 0x00: break;
            case 0x01: LOAD_RR_D16(vm, R16_BC); break;
            case 0x02: LOAD_ARR_R(vm, R16_BC, R8_A); break;
            case 0x03: INC_RR(vm, R16_BC); break;
            case 0x04: incrementR8(vm, R8_B); break;
            case 0x05: decrementR8(vm, R8_B); break;
            case 0x06: LOAD_R_D8(vm, R8_B); break;
            case 0x07: rotateLeft(vm, R8_A, false); break;
            case 0x08: {
                uint16_t a = READ_16BIT(vm);
                writeAddr(vm, a, vm->GPR[R16_SP] & 0xFF);
                writeAddr(vm, a+1, vm->GPR[R16_SP] >> 8);
                break;
            }
            case 0x09: addR16(vm, R16_HL, R16_BC); break;
            case 0x0A: LOAD_R_ARR(vm, R8_A, R16_BC); break;
            case 0x0B: DEC_RR(vm, R16_BC); break;
            case 0x0C: incrementR8(vm, R8_C); break;
            case 0x0D: decrementR8(vm, R8_C); break;
            case 0x0E: LOAD_R_D8(vm, R8_C); break;
            case 0x0F: rotateRight(vm, R8_A, false); break;
            /* OPCODE : STOP, for testing only */
            case 0x10: return;
            case 0x11: LOAD_RR_D16(vm, R16_DE); break;
            case 0x12: LOAD_ARR_R(vm, R16_DE, R8_A); break;
            case 0x13: INC_RR(vm, R16_DE); break;
            case 0x14: incrementR8(vm, R8_D); break;
            case 0x15: decrementR8(vm, R8_D); break;
            case 0x16: LOAD_R_D8(vm, R8_D); break;
            case 0x17: rotateLeft(vm, R8_A, false); break;
            case 0x18: JUMP_RL(vm, READ_BYTE(vm)); break;
            case 0x19: addR16(vm, R16_HL, R16_DE); break;
            case 0x1A: LOAD_R_ARR(vm, R8_A, R16_DE); break;
            case 0x1B: DEC_RR(vm, R16_DE); break;
            case 0x1C: incrementR8(vm, R8_E); break;
            case 0x1D: decrementR8(vm, R8_E); break;
            case 0x1E: LOAD_R_D8(vm, R8_E); break;
            case 0x1F: rotateRight(vm, R8_A, false); break;
            case 0x20: jumpRelativeCondition(vm, CONDITION_NZ(vm)); break;
            case 0x21: LOAD_RR_D16(vm, R16_HL); break;
            case 0x22: LOAD_ARR_R(vm, R16_HL, R8_A); 
                       INC_RR(vm, R16_HL); 
                       break;
            case 0x23: INC_RR(vm, R16_SP); break;
            case 0x24: incrementR8(vm, R8_H); break;
            case 0x25: decrementR8(vm, R8_H); break;
            case 0x26: LOAD_R_D8(vm, R8_H); break;
            case 0x27: decimalAdjust(vm); break;
            case 0x28: jumpRelativeCondition(vm, CONDITION_Z(vm)); break;
            case 0x29: addR16(vm, R16_HL, R16_HL); break;
            case 0x2A: LOAD_R_ARR(vm, R8_A, R16_HL); 
                       INC_RR(vm, R16_HL); 
                       break;
            case 0x2B: DEC_RR(vm, R16_HL); break;
            case 0x2C: incrementR8(vm, R8_L); break;
            case 0x2D: decrementR8(vm, R8_L); break;
            case 0x2E: LOAD_R_D8(vm, R8_L); break;
            case 0x2F: cpl(vm); break;
            case 0x30: jumpRelativeCondition(vm, CONDITION_NC(vm)); break;
            case 0x31: LOAD_RR_D16(vm, R16_SP); break;
            case 0x32: LOAD_ARR_R(vm, R16_HL, R8_A); 
                       DEC_RR(vm, R16_HL); 
                       break;
            case 0x33: INC_RR(vm, R16_SP); break;
            case 0x34: {
                /* Increment what is at the address in HL */
                uint16_t address = get_reg16(vm, R16_HL);
                uint8_t old = vm->MEM[address];
                uint8_t new = old + 1; 
                
                vm->MEM[address] += 1;

                TEST_Z_FLAG(vm, new);
                TEST_H_FLAG_ADD(vm, old, new);
                set_flag(vm, FLAG_N, 0);
                break;
            }
            case 0x35: {
                /* Decrement what is at the address in HL */
                uint16_t address = get_reg16(vm, R16_HL);
                uint8_t old = vm->MEM[address];
                uint8_t new = old - 1; 
                
                TEST_Z_FLAG(vm, new);
                TEST_H_FLAG_SUB(vm, old, new);
                set_flag(vm, FLAG_N, 1);
                break;
            }
            case 0x36: LOAD_ARR_D8(vm, R16_HL); break;
            case 0x37: {
                set_flag(vm, FLAG_C, 1);
                set_flag(vm, FLAG_N, 0);
                set_flag(vm, FLAG_H, 0);
                break;
            }
            case 0x38: jumpRelativeCondition(vm, CONDITION_C(vm)); break;
            case 0x39: addR16(vm, R16_HL, R16_SP); break;
            case 0x3A: {
                LOAD_R_ARR(vm, R8_A, R16_HL);
                DEC_RR(vm, R16_HL);
                break;
            }
            case 0x3B: DEC_RR(vm, R16_SP); break;
            case 0x3C: incrementR8(vm, R8_A); break;
            case 0x3D: decrementR8(vm, R8_A); break;
            case 0x3E: LOAD_R_D8(vm, R8_A); break;
            case 0x3F: CCF(vm); break;
            case 0x40: LOAD_R_R(vm, R8_B, R8_B); break;
            case 0x41: LOAD_R_R(vm, R8_B, R8_C); break;
            case 0x42: LOAD_R_R(vm, R8_B, R8_D); break;
            case 0x43: LOAD_R_R(vm, R8_B, R8_E); break;
            case 0x44: LOAD_R_R(vm, R8_B, R8_H); break;
            case 0x45: LOAD_R_R(vm, R8_B, R8_L); break;
            case 0x46: LOAD_R_ARR(vm, R8_B, R16_HL); break;
            case 0x47: LOAD_R_R(vm, R8_B, R8_A); break;
            case 0x48: LOAD_R_R(vm, R8_C, R8_B); break;
            case 0x49: LOAD_R_R(vm, R8_C, R8_C); break;
            case 0x4A: LOAD_R_R(vm, R8_C, R8_D); break;
            case 0x4B: LOAD_R_R(vm, R8_C, R8_E); break;
            case 0x4C: LOAD_R_R(vm, R8_C, R8_H); break;
            case 0x4D: LOAD_R_R(vm, R8_C, R8_L); break;
            case 0x4E: LOAD_R_ARR(vm, R8_C, R16_HL); break;
            case 0x4F: LOAD_R_R(vm, R8_C, R8_A); break;
            case 0x50: LOAD_R_R(vm, R8_D, R8_B); break;
            case 0x51: LOAD_R_R(vm, R8_D, R8_C); break;
            case 0x52: LOAD_R_R(vm, R8_D, R8_D); break;
            case 0x53: LOAD_R_R(vm, R8_D, R8_E); break;
            case 0x54: LOAD_R_R(vm, R8_D, R8_H); break;
            case 0x55: LOAD_R_R(vm, R8_D, R8_L); break;
            case 0x56: LOAD_R_ARR(vm, R8_D, R16_HL); break;
            case 0x57: LOAD_R_R(vm, R8_D, R8_A); break;
            case 0x58: LOAD_R_R(vm, R8_E, R8_B); break;
            case 0x59: LOAD_R_R(vm, R8_E, R8_C); break;
            case 0x5A: LOAD_R_R(vm, R8_E, R8_D); break;
            case 0x5B: LOAD_R_R(vm, R8_E, R8_E); break;
            case 0x5C: LOAD_R_R(vm, R8_E, R8_H); break;
            case 0x5D: LOAD_R_R(vm, R8_E, R8_L); break;
            case 0x5E: LOAD_R_ARR(vm, R8_E, R16_HL); break;
            case 0x5F: LOAD_R_R(vm, R8_E, R8_A); break;
            case 0x60: LOAD_R_R(vm, R8_H, R8_B); break;
            case 0x61: LOAD_R_R(vm, R8_H, R8_C); break;
            case 0x62: LOAD_R_R(vm, R8_H, R8_D); break;
            case 0x63: LOAD_R_R(vm, R8_H, R8_E); break;
            case 0x64: LOAD_R_R(vm, R8_H, R8_H); break;
            case 0x65: LOAD_R_R(vm, R8_H, R8_L); break;
            case 0x66: LOAD_R_ARR(vm, R8_H, R16_HL); break;
            case 0x67: LOAD_R_R(vm, R8_H, R8_A); break;
            case 0x68: LOAD_R_R(vm, R8_L, R8_B); break;
            case 0x69: LOAD_R_R(vm, R8_L, R8_C); break;
            case 0x6A: LOAD_R_R(vm, R8_L, R8_D); break;
            case 0x6B: LOAD_R_R(vm, R8_L, R8_E); break;
            case 0x6C: LOAD_R_R(vm, R8_L, R8_H); break;
            case 0x6D: LOAD_R_R(vm, R8_L, R8_L); break;
            case 0x6E: LOAD_R_ARR(vm, R8_L, R16_HL); break;
            case 0x6F: LOAD_R_R(vm, R8_L, R8_A); break;
            case 0x70: LOAD_ARR_R(vm, R16_HL, R8_B); break;
            case 0x71: LOAD_ARR_R(vm, R16_HL, R8_C); break;
            case 0x72: LOAD_ARR_R(vm, R16_HL, R8_D); break;
            case 0x73: LOAD_ARR_R(vm, R16_HL, R8_E); break;
            case 0x74: LOAD_ARR_R(vm, R16_HL, R8_H); break;
            case 0x75: LOAD_ARR_R(vm, R16_HL, R8_L); break;
            /* TODO: HALT */ 
            case 0x76: break;
            case 0x77: LOAD_ARR_R(vm, R16_HL, R8_A); break;
            case 0x78: LOAD_R_R(vm, R8_A, R8_B); break;
            case 0x79: LOAD_R_R(vm, R8_A, R8_C); break;
            case 0x7A: LOAD_R_R(vm, R8_A, R8_D); break;
            case 0x7B: LOAD_R_R(vm, R8_A, R8_E); break;
            case 0x7C: LOAD_R_R(vm, R8_A, R8_H); break;
            case 0x7D: LOAD_R_R(vm, R8_A, R8_L); break;
            case 0x7E: LOAD_R_ARR(vm, R8_A, R16_HL); break;
            case 0x7F: LOAD_R_R(vm, R8_A, R8_A); break;
            case 0x80: addR8(vm, R8_A, R8_B); break;
            case 0x81: addR8(vm, R8_A, R8_C); break;
            case 0x82: addR8(vm, R8_A, R8_D); break;
            case 0x83: addR8(vm, R8_A, R8_E); break;
            case 0x84: addR8(vm, R8_A, R8_H); break;
            case 0x85: addR8(vm, R8_A, R8_L); break;
            case 0x86: addR8_AR16(vm, R8_A, R16_HL); break;
            case 0x87: addR8(vm, R8_A, R8_A); break;
            case 0x88: adcR8(vm, R8_A, R8_B); break;
            case 0x89: adcR8(vm, R8_A, R8_C); break;
            case 0x8A: adcR8(vm, R8_A, R8_D); break;
            case 0x8B: adcR8(vm, R8_A, R8_E); break;
            case 0x8C: adcR8(vm, R8_A, R8_H); break;
            case 0x8D: adcR8(vm, R8_A, R8_L); break;
            case 0x8E: adcR8_AR16(vm, R8_A, R16_HL); break;
            case 0x8F: adcR8(vm, R8_A, R8_A); break;
            case 0x90: subR8(vm, R8_A, R8_B); break;
            case 0x91: subR8(vm, R8_A, R8_C); break;
            case 0x92: subR8(vm, R8_A, R8_D); break;
            case 0x93: subR8(vm, R8_A, R8_E); break;
            case 0x94: subR8(vm, R8_A, R8_H); break;
            case 0x95: subR8(vm, R8_A, R8_L); break;
            case 0x96: subR8_AR16(vm, R8_A, R16_HL); break;
            case 0x97: subR8(vm, R8_A, R8_A); break;
            case 0x98: sbcR8(vm, R8_A, R8_B); break;
            case 0x99: sbcR8(vm, R8_A, R8_C); break;
            case 0x9A: sbcR8(vm, R8_A, R8_D); break;
            case 0x9B: sbcR8(vm, R8_A, R8_E); break;
            case 0x9C: sbcR8(vm, R8_A, R8_H); break;
            case 0x9D: sbcR8(vm, R8_A, R8_L); break;
            case 0x9E: sbcR8_AR16(vm, R8_A, R16_HL); break;
            case 0x9F: sbcR8(vm, R8_A, R8_A); break;
            case 0xA0: andR8(vm, R8_A, R8_B); break;
            case 0xA1: andR8(vm, R8_A, R8_C); break;
            case 0xA2: andR8(vm, R8_A, R8_D); break;
            case 0xA3: andR8(vm, R8_A, R8_E); break;
            case 0xA4: andR8(vm, R8_A, R8_H); break;
            case 0xA5: andR8(vm, R8_A, R8_L); break;
            case 0xA6: andR8_AR16(vm, R8_A, R16_HL); break;
            case 0xA7: andR8(vm, R8_A, R8_A); break;
            case 0xA8: xorR8(vm, R8_A, R8_B); break;
            case 0xA9: xorR8(vm, R8_A, R8_C); break;
            case 0xAA: xorR8(vm, R8_A, R8_D); break;
            case 0xAB: xorR8(vm, R8_A, R8_E); break;
            case 0xAC: xorR8(vm, R8_A, R8_H); break;
            case 0xAD: xorR8(vm, R8_A, R8_L); break;
            case 0xAE: xorR8_AR16(vm, R8_A, R16_HL); break;
            case 0xAF: xorR8(vm, R8_A, R8_A); break;
            case 0xB0: orR8(vm, R8_A, R8_B); break;
            case 0xB1: orR8(vm, R8_A, R8_C); break;
            case 0xB2: orR8(vm, R8_A, R8_D); break;
            case 0xB3: orR8(vm, R8_A, R8_E); break;
            case 0xB4: orR8(vm, R8_A, R8_H); break;
            case 0xB5: orR8(vm, R8_A, R8_L); break;
            case 0xB6: orR8_AR16(vm, R8_A, R16_HL); break;
            case 0xB7: orR8(vm, R8_A, R8_A); break;
            case 0xB8: compareR8(vm, R8_A, R8_B); break;
            case 0xB9: compareR8(vm, R8_A, R8_C); break;
            case 0xBA: compareR8(vm, R8_A, R8_D); break;
            case 0xBB: compareR8(vm, R8_A, R8_E); break;
            case 0xBC: compareR8(vm, R8_A, R8_H); break;
            case 0xBD: compareR8(vm, R8_A, R8_L); break;
            case 0xBE: compareR8_AR16(vm, R8_A, R16_HL); break;
            case 0xBF: compareR8(vm, R8_A, R8_A); break;
            case 0xC0: retCondition(vm, CONDITION_NZ(vm)); break;
            case 0xC1: POP_R16(vm, R16_BC); break;
            case 0xC2: jumpCondition(vm, CONDITION_NZ(vm)); break;
            case 0xC3: JUMP(vm, READ_16BIT(vm)); break;
            case 0xC4: callCondition(vm, CONDITION_NZ(vm)); break;
            case 0xC5: PUSH_R16(vm, R16_BC); break;
            case 0xC6: addR8D8(vm, R8_A); break;
            case 0xC7: rst(vm, 0x00); break;
            case 0xC8: retCondition(vm, CONDITION_Z(vm)); break;
            case 0xC9: ret(vm); break;
            case 0xCA: jumpCondition(vm, CONDITION_Z(vm)); break;
            case 0xCB: prefixCB(vm); break;
            case 0xCC: callCondition(vm, CONDITION_Z(vm)); break;
            case 0xCD: call(vm); break;
            case 0xCE: adcR8D8(vm, R8_A); break;
            case 0xCF: rst(vm, 0x08); break;
            case 0xD0: retCondition(vm, CONDITION_NC(vm)); break;
            case 0xD1: POP_R16(vm, R16_DE); break;
            case 0xD2: jumpCondition(vm, CONDITION_NC(vm)); break;
            case 0xD4: callCondition(vm, CONDITION_NC(vm)); break;
            case 0xD5: PUSH_R16(vm, R16_DE); break;
            case 0xD6: subR8D8(vm, R8_A); break;
            case 0xD7: rst(vm, 0x10); break;
            case 0xD8: retCondition(vm, CONDITION_C(vm)); break;
            case 0xD9: INTERRUPT_MASTER_ENABLE(vm); ret(vm); break;
            case 0xDA: jumpCondition(vm, CONDITION_C(vm)); break;
            case 0xDC: callCondition(vm, CONDITION_C(vm)); break;
            case 0xDE: sbcR8D8(vm, R8_A); break;
            case 0xDF: rst(vm, 0x18); break;
            case 0xE0: LOAD_D8PORT_R(vm, R8_A); break;
            case 0xE1: POP_R16(vm, R16_HL); break;
            case 0xE2: LOAD_RPORT_R(vm, R8_A, R8_C); break;
            case 0xE5: PUSH_R16(vm, R16_HL); break;
            case 0xE6: andR8D8(vm, R8_A); break;
            case 0xE7: rst(vm, 0x20); break;
            case 0xE8: addR16I8(vm, R16_SP, R16_SP); break;
            case 0xE9: JUMP(vm, get_reg16(vm, R16_HL)); break; 
            case 0xEA: LOAD_MEM_R(vm, R8_A); break;
            case 0xEE: xorR8D8(vm, R8_A); break;
            case 0xEF: rst(vm, 0x28); break;
            case 0xF0: LOAD_R_D8PORT(vm, R8_A); break;
            case 0xF1: POP_R16(vm, R16_AF); break;
            case 0xF2: LOAD_R_RPORT(vm, R8_A, R8_C); break;
            case 0xF3: INTERRUPT_MASTER_DISABLE(vm); break;
            case 0xF5: PUSH_R16(vm, R16_AF); break;
            case 0xF6: orR8D8(vm, R8_A); break;
            case 0xF7: rst(vm, 0x30); break;
            case 0xF8: addR16I8(vm, R16_SP, R16_HL); break;
            case 0xF9: LOAD_RR_RR(vm, R16_SP, R16_HL); break;
            case 0xFA: LOAD_R_MEM(vm, R8_A); break;
            case 0xFB: INTERRUPT_MASTER_ENABLE(vm); break;
            case 0xFE: compareR8D8(vm, R8_A); break;
            case 0xFF: rst(vm, 0x38); break;
        }
    }
}

void loadCartridge(VM *vm, Cartridge *cartridge) {
    vm->cartridge = cartridge;
    cartridge->inserted = true;
#ifdef DEBUG_PRINT_CARTRIDGE_INFO 
    printCartridge(cartridge);
#endif
    /* Cartridge is now in inserted state */
#ifdef DEBUG_LOGGING
    printf("Booting ROM\n");
#endif
    bootROM(vm);
#ifdef DEBUG_LOGGING
    printf("Successfully Booted ROM\n");
    printf("Starting Emulated Display\n");
#endif
    startDisplay(vm);
#ifdef DEBUG_LOGGING
    printf("Successfully Started Emulated Display\n");
    printf("Starting CPU execution\n");
#endif
    /* We provide a 1 second delay to ensure other hardware threads have started up */
    sleep(1);
    runVM(vm);
#ifdef DEBUG_LOGGING
    printf("Finished CPU execution\n");
#endif
}


