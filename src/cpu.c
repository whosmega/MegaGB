#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../include/vm.h"
#include "../include/debug.h"
#include "../include/display.h"
#include "../include/cpu.h"

#define PORT_ADDR 0xFF00

/* Load 16 bit data into an R16 Register */
#define LOAD_RR_D16(vm, RR) set_reg16_8C(vm, RR, read2Bytes(vm))
/* Load contents of R8 Register into address at R16 register (dereferencing) */
#define LOAD_ARR_R(vm, RR, R) writeAddr_4C(vm, get_reg16(vm, RR), vm->GPR[R])
/* Load 8 bit data into R8 Register */
#define LOAD_R_D8(vm, R) vm->GPR[R] = readByte(vm); cyclesSync_4(vm)
/* Dereference the address contained in the R16 register and set it's value 
 * to the R8 register */
#define LOAD_R_ARR(vm, R, RR) vm->GPR[R] = readAddr(vm, get_reg16(vm, RR)); cyclesSync_4(vm)
/* Load 8 bit data into address at R16 register (dereferencing) */
#define LOAD_ARR_D8(vm, RR) writeAddr_4C(vm, get_reg16(vm, RR), readByte_4C(vm))
/* Load contents of R8 register into another R8 register */
#define LOAD_R_R(vm, R1, R2) vm->GPR[R1] = vm->GPR[R2]
/* Load contents of R16 register into another R16 register */
#define LOAD_RR_RR(vm, RR1, RR2) set_reg16(vm, RR1, get_reg16(vm, RR2)); cyclesSync_4(vm)
/* Load instructions from reading into and writing into main memory */
#define LOAD_MEM_R(vm, R) writeAddr_4C(vm, read2Bytes_8C(vm), vm->GPR[R])
/* Load what's at the address specified by the 16 bit data into the R8 register */
#define LOAD_R_MEM(vm, R) vm->GPR[R] = readAddr_4C(vm, read2Bytes_8C(vm))
/* Load 'R' into '(PORT_ADDR + D8)' */
#define LOAD_D8PORT_R(vm, R) writeAddr_4C(vm, PORT_ADDR + readByte_4C(vm), vm->GPR[R])
/* Load '(PORT_ADDR + D8) into 'R' */
#define LOAD_R_D8PORT(vm, R) vm->GPR[R] = readAddr_4C(vm, PORT_ADDR + readByte_4C(vm))
/* Load 'R1' into '(PORT_ADDR + R2)' */
#define LOAD_RPORT_R(vm, R1, R2) writeAddr_4C(vm, PORT_ADDR + vm->GPR[R2], vm->GPR[R1])
/* Load '(PORT_ADDR + R2)' into 'R1' */
#define LOAD_R_RPORT(vm, R1, R2) vm->GPR[R1] = readAddr_4C(vm, PORT_ADDR + vm->GPR[R2])

#define LOAD_RR_RRI8(vm, RR1, RR2) load_rr_rri8(vm, RR1, RR2)
/* Increment contents of R16 register */
#define INC_RR(vm, RR) set_reg16(vm, RR, (get_reg16(vm, RR) + 1)); cyclesSync_4(vm)
/* Decrement contents of R16 register */
#define DEC_RR(vm, RR) set_reg16(vm, RR, (get_reg16(vm, RR) - 1)); cyclesSync_4(vm)

/* Direct Jump */
#define JUMP(vm, a16) vm->PC = a16; cyclesSync_4(vm)
#define JUMP_RR(vm, RR) vm->PC = get_reg16(vm, RR)
/* Relative Jump */
#define JUMP_RL(vm, i8) vm->PC += (int8_t)i8; cyclesSync_4(vm)

#define PUSH_R16(vm, RR) cyclesSync_4(vm); push16(vm, get_reg16(vm, RR))
#define POP_R16(vm, RR) set_reg16(vm, RR, pop16(vm))
#define RST(vm, a16) call(vm, a16)
#define INTERRUPT_MASTER_ENABLE(vm) vm->scheduleInterruptEnable = true
#define INTERRUPT_MASTER_DISABLE(vm) vm->IME = false
#define CCF(vm) set_flag(vm, FLAG_C, get_flag(vm, FLAG_C) ^ 1); \
                set_flag(vm, FLAG_N, 0);                        \
                set_flag(vm, FLAG_H, 0)
/* Flag utility macros */
#define TEST_Z_FLAG(vm, r) set_flag(vm, FLAG_Z, r == 0 ? 1 : 0)
/* We check if a carry over for the last 4 bits happened 
 * ref : https://www.reddit.com/r/EmuDev/comments/4ycoix/a_guide_to_the_gameboys_halfcarry_flag*/
#define TEST_H_FLAG_ADD(vm, x, y) set_flag(vm, FLAG_H, \
                                    (((uint32_t)x & 0xf) + ((uint32_t)y & 0xf) > 0xf) ? 1 : 0)



/* half carry counts when theres carry from bit 11-12 for most 16 bit instructions */

#define TEST_H_FLAG_ADD16(vm, x, y) set_flag(vm, FLAG_H, \
                                    (((uint32_t)x & 0xfff) + ((uint32_t)y & 0xfff) > 0xfff) ? 1 : 0 )

#define TEST_H_FLAG_SUB(vm, x, y) set_flag(vm, FLAG_H, \
                                    (((x & 0xf) - (y & 0xf) & 0x10) ? 1 : 0))
/* Test for integer overloads and set carry flags */
#define TEST_C_FLAG_ADD16(vm, x, y) set_flag(vm, FLAG_C, ((uint32_t)(x) + (uint32_t)(y)) > 0xFFFF ? 1 : 0)
#define TEST_C_FLAG_ADD8(vm, x, y) set_flag(vm, FLAG_C, ((uint16_t)(x) + (uint16_t)(y)) > 0xFF ? 1 : 0)
#define TEST_C_FLAG_SUB16(vm, x, y) set_flag(vm, FLAG_C, ((int)(x) - (int)(y)) < 0 ? 1 : 0)
#define TEST_C_FLAG_SUB8(vm, x, y) set_flag(vm, FLAG_C, ((int)(x) - (int)(y)) < 0 ? 1 : 0)

static uint8_t readAddr(VM* vm, uint16_t addr);
static void writeAddr(VM* vm, uint16_t addr, uint8_t byte);
static inline void writeAddr_4C(VM* vm, uint16_t addr, uint8_t byte);
static inline uint8_t readAddr_4C(VM* vm, uint16_t addr);

static inline uint8_t readByte(VM* vm) {
    /* Reads a byte and doesnt consume any cycles */
    return vm->MEM[vm->PC++];
}

static inline uint8_t readByte_4C(VM* vm) {
    /* Reads a byte and consumes 4 cycles */
    uint8_t byte = vm->MEM[vm->PC++];

    cyclesSync_4(vm);
    return byte;
}

static inline uint16_t read2Bytes(VM* vm) {
    /* Reads 2 bytes and doesnt consume any cycles */
    return (uint16_t)(vm->MEM[vm->PC++] | (vm->MEM[vm->PC++] << 8));
}

static uint16_t read2Bytes_8C(VM* vm) {
    /* Reads 2 bytes and consumes 8 cycles, 4 per byte */
    uint8_t low = vm->MEM[vm->PC++]; 
    cyclesSync_4(vm);

    uint8_t high = vm->MEM[vm->PC++];
    cyclesSync_4(vm);

    return (uint16_t)(low | (high << 8));
}

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

/* Sets 16 bit register by modifying the individual 8 bit registers
 * it consists of */
static inline uint16_t set_reg16(VM* vm, GP_REG RR, uint16_t v) {
    vm->GPR[RR] = v >> 8; 
    vm->GPR[RR + 1] = v & 0xFF;

    return v;
}

static uint16_t set_reg16_8C(VM* vm, GP_REG RR, uint16_t v) {
    /* Takes 8 clock cycles */
    vm->GPR[RR] = v >> 8;
    cyclesSync_4(vm);

    vm->GPR[RR + 1] = v & 0xFF;
    cyclesSync_4(vm);
    return v;
}

void resetGBC(VM* vm) {
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

    /* Set hardware registers and initialise empty areas */
    
    uint8_t hreg_defaults[80] = {
        0xCF, 0x00, 0x7F, 0xFF, 0xAC, 0x00, 0x00, 0xF8,                 // 0xFF00 - 0xFF07
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE1,                 // 0xFF08 - 0xFF0F
        0x80, 0xBF, 0xF3, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,                 // 0xFF10 - 0xFF17
        0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,                 // 0xFF18 - 0xFF1F
        0xFF, 0x00, 0x00, 0xBF, 0x77, 0xF3, 0xF1, 0xFF,                 // 0xFF20 - 0xFF27
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF28 - 0xFF2F
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF30 - 0xFF37
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF38 - 0xFF3F
        0x91, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC,                 // 0xFF40 - 0xFF47
        0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF48 - 0xFF4F,
    };

    /* 0xFF50 to 0xFFFE = 0xFF */

    for (int i = 0x00; i < 0x50; i++) {
        vm->MEM[0xFF00 + i] = hreg_defaults[i];
    }

    memset(&vm->MEM[0xFF50], 0xFF, 0xAF);
    INTERRUPT_MASTER_DISABLE(vm);
}

static void load_rr_rri8(VM* vm, GP_REG RR1, GP_REG RR2) {
    /* Opcode 0xF8 specific 
     *
     * RR1 = RR2 + I8 */
    uint16_t old = get_reg16(vm, RR2); 
    int8_t toAdd = (int8_t)readByte_4C(vm);
    uint16_t result = set_reg16(vm, RR1, old + toAdd);

    set_flag(vm, FLAG_Z, 0);
    set_flag(vm, FLAG_N, 0);

    /* For some reason this 16 bit addition does not do normal 
     * 16 bit half carry setting, so we do the 8 bit test */
    old &= 0xFF;

    /* We passed unsigned version to flag tests to handle
     * flags because at the low level its basically the same thing */
    TEST_H_FLAG_ADD(vm, old, (uint8_t)toAdd);
    TEST_C_FLAG_ADD8(vm, old, (uint8_t)toAdd);
	
	/* Internal */
	cyclesSync_4(vm);
}

static void incrementR8(VM* vm, GP_REG R) {
    /* A utility function which does incrementing for 8 bit registers
     * it does all flag updates it needs to */
    uint8_t old = vm->GPR[R];
    vm->GPR[R]++;

    TEST_Z_FLAG(vm, vm->GPR[R]);
    TEST_H_FLAG_ADD(vm, old, 1);
    set_flag(vm, FLAG_N, 0);
}

static void decrementR8(VM* vm, GP_REG R) {
    /* Decrementing for 8 bit registers */
    uint8_t old = vm->GPR[R]; uint8_t new = --vm->GPR[R];
    TEST_Z_FLAG(vm, new);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, 1);
}

/* The folloing functions are used for all 8 bit rotation 
 * operations */ 

static void rotateLeftR8(VM* vm, GP_REG R, bool setZFlag) {
    /* Rotates value to left, moves 7th bit to bit 0 and C flag */
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
    uint8_t toModify = readAddr_4C(vm, addr);
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= bit7;

    writeAddr_4C(vm, addr, toModify);
    if (setZFlag) {
        TEST_Z_FLAG(vm, toModify);
    } else {
        set_flag(vm, FLAG_Z, 0);
    }
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit7);  
}

static void rotateRightR8(VM* vm, GP_REG R, bool setZFlag) {
    /* Rotates value to right, moves bit 0 to bit 7 and C flag */
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
    uint8_t toModify = readAddr_4C(vm, addr);
    uint8_t bit1 = toModify & 1;

    toModify >>= 1;
    toModify |= bit1 << 7;

    writeAddr_4C(vm, addr, toModify);

    if (setZFlag) TEST_Z_FLAG(vm, toModify);
    else set_flag(vm, FLAG_Z, 0);

    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit1); 
}

static void rotateLeftCarryR8(VM* vm, GP_REG R8, bool setZFlag) {
    /* Rotates value to left, moves bit 7 to C flag and C flag's original value
     * to bit 0 */
    uint8_t toModify = vm->GPR[R8];
    bool carryFlag = get_flag(vm, FLAG_C);
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= carryFlag;

    vm->GPR[R8] = toModify;

    if (setZFlag) TEST_Z_FLAG(vm, toModify);
    else set_flag(vm, FLAG_Z, 0);

    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit7);
}

static void rotateLeftCarryAR16(VM* vm, GP_REG R16, bool setZFlag) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t toModify = readAddr_4C(vm, addr);
    bool carryFlag = get_flag(vm, FLAG_C);
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= carryFlag;

    writeAddr_4C(vm, addr, toModify);

    if (setZFlag) TEST_Z_FLAG(vm, toModify);
    else set_flag(vm, FLAG_Z, 0);

    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit7);
}

static void rotateRightCarryR8(VM* vm, GP_REG R8, bool setZFlag) {
    /* Rotates value to right, moves bit 0 to C flag and C flag's original value
     * to bit 7 */
    uint8_t toModify = vm->GPR[R8];
    bool carryFlag = get_flag(vm, FLAG_C);
    uint8_t bit0 = toModify & 1;

    toModify >>= 1;
    toModify |= carryFlag << 7;

    vm->GPR[R8] = toModify;

    if (setZFlag) TEST_Z_FLAG(vm, toModify);
    else set_flag(vm, FLAG_Z, 0);

    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit0);
}

static void rotateRightCarryAR16(VM* vm, GP_REG R16, bool setZFlag) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t toModify = readAddr_4C(vm, addr);
    bool carryFlag = get_flag(vm, FLAG_C);
    uint8_t bit0 = toModify & 1;

    toModify >>= 1;
    toModify |= carryFlag << 7;

    writeAddr_4C(vm, addr, toModify);

    if (setZFlag) TEST_Z_FLAG(vm, toModify);
    else set_flag(vm, FLAG_Z, 0);

    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit0);
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
    uint8_t value = readAddr_4C(vm, addr);
    uint8_t bit7 = value >> 7;
    uint8_t result = value << 1;

    writeAddr_4C(vm, addr, result);

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
    uint8_t value = readAddr_4C(vm, addr);
    uint8_t bit1 = value & 0x1;
    uint8_t result = value >> 1;

    writeAddr_4C(vm, addr, result);

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit1);
}

static void shiftRightArithmeticR8(VM* vm, GP_REG R) {
    uint8_t value = vm->GPR[R];
    uint8_t bit7 = value >> 7;
    uint8_t bit0 = value & 0x1;
    uint8_t result = value >> 1;
    
    /* Copy the 7th bit to its original location after the shift */
    result |= bit7 << 7;
    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit0);
}

static void shiftRightArithmeticAR16(VM* vm, GP_REG R16) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t value = readAddr_4C(vm, addr); 
    uint8_t bit7 = value >> 7;
    uint8_t bit0 = value & 0x1;
    uint8_t result = value >> 1;
    
    /* Copy the 7th bit to its original location after the shift */
    result |= bit7 << 7;
    writeAddr_4C(vm, addr, result);

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit0);
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
     uint8_t value = readAddr_4C(vm, addr); 
     uint8_t highNibble = value >> 4;
     uint8_t lowNibble = value & 0xF;

     uint8_t newValue = (lowNibble << 4) | highNibble;

     writeAddr_4C(vm, addr, newValue);

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
    uint8_t value = readAddr_4C(vm, get_reg16(vm, R16));
    uint8_t bitValue = (value >> bit) & 0x1;

    TEST_Z_FLAG(vm, bitValue);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 1);
}

static void setBitR8(VM* vm, GP_REG R8, uint8_t bit) {
    uint8_t value = vm->GPR[R8];
    uint8_t orValue = 1 << bit;
    uint8_t result = value | orValue;

    vm->GPR[R8] = result;
}

static void setBitAR16(VM* vm, GP_REG R16, uint8_t bit) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t value = readAddr_4C(vm, addr);
    uint8_t orValue = 1 << bit;
    uint8_t result = value | orValue;

    writeAddr_4C(vm, addr, result);
}

static void resetBitR8(VM* vm, GP_REG R8, uint8_t bit) {
    uint8_t value = vm->GPR[R8];
    /* Converts 00010000 to 11101111 for the andValue when bit 3 has to be reset for ex */
    uint8_t andValue = ~(1 << bit);
    uint8_t result = value & andValue;

    vm->GPR[R8] = result;
}

static void resetBitAR16(VM* vm, GP_REG R16, uint8_t bit) {
    uint16_t addr = get_reg16(vm, R16);
    uint8_t value = readAddr_4C(vm, addr);
    uint8_t andValue = ~(1 << bit);
    uint8_t result = value & andValue;

    writeAddr_4C(vm, addr, result);
}

/* The following functions form the most of the arithmetic and logical
 * operations of the CPU */

static void addR16(VM* vm, GP_REG RR1, GP_REG RR2) {
    uint16_t old = get_reg16(vm, RR1); 
    uint16_t toAdd = get_reg16(vm, RR2);
    uint16_t result = set_reg16(vm, RR1, old + toAdd);

    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD16(vm, old, toAdd);
    TEST_C_FLAG_ADD16(vm, old, toAdd);

    cyclesSync_4(vm);
}

/* Adding a signed 8 bit integer to a 16 bit register */

static void addR16I8(VM* vm, GP_REG RR) {
    uint16_t old = get_reg16(vm, RR);
    int8_t toAdd = (int8_t)readByte_4C(vm);
    uint16_t result = set_reg16_8C(vm, RR, old + toAdd);
    set_flag(vm, FLAG_Z, 0);
    set_flag(vm, FLAG_N, 0);

    /* For some reason this 16 bit addition does not do normal 
     * 16 bit half carry setting, so we do the 8 bit test */
    old &= 0xFF;

    /* We passed unsigned version to flag tests to handle
     * flags because at the low level its basically the same thing */
    TEST_H_FLAG_ADD(vm, old, (uint8_t)toAdd);
    TEST_C_FLAG_ADD8(vm, old, (uint8_t)toAdd);
}

static void addR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t toAdd = vm->GPR[R2];
    uint8_t result = old + toAdd;

    vm->GPR[R1] = result;
    
    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, toAdd);
    TEST_C_FLAG_ADD8(vm, old, toAdd);
}

static void addR8D8(VM* vm, GP_REG R) {
    uint8_t data = readByte_4C(vm);
    uint8_t old = vm->GPR[R];
    uint8_t result = old + data;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, data);
    TEST_C_FLAG_ADD8(vm, old, data);
}

static void addR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t toAdd = readAddr_4C(vm, get_reg16(vm, R16));
    uint8_t result = old + toAdd;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, toAdd);
    TEST_C_FLAG_ADD8(vm, old, toAdd);
}

static void test_adc_hcflags(VM* vm, uint8_t old, uint8_t toAdd, uint8_t result, uint8_t carry) {
    /* ADC has a slightly different behaviour i.e it adds the
     * carry flag to the result, the normal H & C flag tests 
     * have edge cases if used with ADC, so we do a custom test here */

    bool halfCarryOccurred = false;
    bool carryOccured = false;

    /* Half carry - It can occur either when:
     * 1. The initial value is added to the value that has to be added 
     * 2. The result of the above is added to the carry flag 
     *
     * We note if a half carry occured in both the cases */

    if (((old & 0xF) + (toAdd & 0xF)) > 0xF) halfCarryOccurred = true;
    if (((result & 0xF) + (carry & 0xF)) > 0xF) halfCarryOccurred = true;

    /* Carry Flag - When theres an integer overflow for the 8 bit addition
     * we set the carry flag, the overflow must also be calculated separately like
     * we did for half carry */

    if (((uint16_t)old + (uint16_t)toAdd) > 0xFF) carryOccured = true;
    if (((uint16_t)result + (uint16_t)carry) > 0xFF) carryOccured = true;

    set_flag(vm, FLAG_H, halfCarryOccurred);
    set_flag(vm, FLAG_C, carryOccured);
}

static void adcR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t toAdd = vm->GPR[R2];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old + toAdd;
    uint8_t finalResult = result + carry;

    vm->GPR[R1] = finalResult;

    TEST_Z_FLAG(vm, finalResult);
    set_flag(vm, FLAG_N, 0);
    test_adc_hcflags(vm, old, toAdd, result, carry);
}

static void adcR8D8(VM* vm, GP_REG R) {
    uint16_t data = (uint16_t)readByte_4C(vm);
    uint8_t old = vm->GPR[R];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old + data;
    uint8_t finalResult = result + carry;

    vm->GPR[R] = finalResult;

    TEST_Z_FLAG(vm, finalResult);
    set_flag(vm, FLAG_N, 0);
    test_adc_hcflags(vm, old, data, result, carry);
}

static void adcR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t toAdd = readAddr_4C(vm, get_reg16(vm, R16));
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old + toAdd;
    uint8_t finalResult = result + carry;

    vm->GPR[R8] = finalResult;

    TEST_Z_FLAG(vm, finalResult);
    set_flag(vm, FLAG_N, 0);
    test_adc_hcflags(vm, old, toAdd, result, carry);
}

static void subR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t toSub = vm->GPR[R2];
    uint8_t result = old - toSub;

    vm->GPR[R1] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, toSub);
    TEST_C_FLAG_SUB8(vm, old, toSub);
}

static void subR8D8(VM* vm, GP_REG R) {
    uint8_t data = readByte_4C(vm);
    uint8_t old = vm->GPR[R];
    uint8_t result = old - data;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, data);
    TEST_C_FLAG_SUB8(vm, old, data);
}

static void subR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t toSub = readAddr_4C(vm, get_reg16(vm, R16));
    uint8_t result = old - toSub;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, toSub);
    TEST_C_FLAG_SUB8(vm, old, toSub);
}

static void test_sbc_hcflags(VM* vm, uint8_t old, uint8_t toSub, uint8_t result, uint8_t carry) {
    /* SBC is also similar to ADC so we need a custom test here aswell */
    bool halfCarryOccurred = false;
    bool carryOccurred = false;
    
    uint8_t oldLow = old & 0xF;
    uint8_t resultLow = result & 0xF;

    if ((uint8_t)(oldLow - (toSub & 0xF)) > oldLow) halfCarryOccurred = true;
    if ((uint8_t)(resultLow - carry) > resultLow) halfCarryOccurred = true;

    if ((uint8_t)((uint16_t)old - (uint16_t)toSub) > old) carryOccurred = true;
    if ((uint8_t)((uint16_t)result - carry) > result) carryOccurred = true;

    set_flag(vm, FLAG_H, halfCarryOccurred);
    set_flag(vm, FLAG_C, carryOccurred);
}

static void sbcR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    uint8_t toSub = vm->GPR[R2];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old - toSub;
    uint8_t finalResult = result - carry;

    vm->GPR[R1] = finalResult;

    TEST_Z_FLAG(vm, finalResult);
    set_flag(vm, FLAG_N, 1);
    test_sbc_hcflags(vm, old, toSub, result, carry);
}

static void sbcR8D8(VM* vm, GP_REG R) {
    uint8_t data = readByte_4C(vm);
    uint8_t old = vm->GPR[R];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old - data;
    uint8_t finalResult = result - carry;

    vm->GPR[R] = finalResult;

    TEST_Z_FLAG(vm, finalResult);
    set_flag(vm, FLAG_N, 1);
    test_sbc_hcflags(vm, old, data, result, carry);
}

static void sbcR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t toSub = readAddr_4C(vm, get_reg16(vm, R16));
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old - toSub;
    uint8_t finalResult = result - carry;

    vm->GPR[R8] = finalResult;

    TEST_Z_FLAG(vm, finalResult);
    set_flag(vm, FLAG_N, 1);
    test_sbc_hcflags(vm, old, toSub, result, carry);
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
    uint8_t operand = readByte_4C(vm);
    uint8_t result = old & operand;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 1);
    set_flag(vm, FLAG_C, 0);
}

static void andR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t operand = readAddr_4C(vm, get_reg16(vm, R16));
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
    uint8_t operand = readByte_4C(vm);
    uint8_t result = old ^ operand;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);
}

static void xorR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t operand = readAddr_4C(vm, get_reg16(vm, R16));
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
    uint8_t operand = readByte_4C(vm);
    uint8_t result = old | operand;

    vm->GPR[R] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);   
}

static void orR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t operand = readAddr_4C(vm, get_reg16(vm, R16));
    uint8_t result = old | operand;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_C, 0);
}

static void compareR8(VM* vm, GP_REG R1, GP_REG R2) {
    /* Basically R1 - R2, but results are thrown away and R1 is unchanged */
    uint8_t old = vm->GPR[R1];
    uint8_t toSub = vm->GPR[R2];
    uint8_t result = old - toSub;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, toSub);
    TEST_C_FLAG_SUB8(vm, old, toSub);
}

static void compareR8D8(VM* vm, GP_REG R) {
    uint8_t old = vm->GPR[R];
    uint8_t toSub = readByte_4C(vm);
    uint8_t result = old - toSub;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, toSub);
    TEST_C_FLAG_SUB8(vm, old, toSub);
}

static void compareR8_AR16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint8_t toSub = readAddr_4C(vm, get_reg16(vm, R16));
    uint8_t result = old - toSub;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, toSub);
    TEST_C_FLAG_SUB8(vm, old, toSub);       
}

/* The following functions are responsible for returning, calling & stack manipulation*/

static inline void push16(VM* vm, uint16_t u16) {
    /* Pushes a 2 byte value to the stack */
    uint16_t stackPointer = get_reg16(vm, R16_SP);
    
    /* Write the high byte */
    writeAddr_4C(vm, stackPointer - 1, u16 >> 8);
    /* Write the low byte */
    writeAddr_4C(vm, stackPointer - 2, u16 & 0xFF);

    /* Update the SP */
    set_reg16(vm, R16_SP, stackPointer - 2);
}

static inline uint16_t pop16(VM* vm) {
    uint16_t stackPointer = get_reg16(vm, R16_SP);

    uint8_t lowByte = readAddr_4C(vm, stackPointer);
    uint8_t highByte = readAddr_4C(vm, stackPointer + 1);
    set_reg16(vm, R16_SP, stackPointer + 2);

    return (uint16_t)((highByte << 8) | lowByte);
}

static void call(VM* vm, uint16_t addr) {
    /* Branch decision cycle sync */
    cyclesSync_4(vm);

    push16(vm, vm->PC);
    vm->PC = addr;
}

static void callCondition(VM* vm, uint16_t addr, bool isTrue) {
    if (isTrue) {
        /* Branch decision cycles sync */
        cyclesSync_4(vm);
        push16(vm, vm->PC);
        vm->PC = addr;
    }
}

static inline void ret(VM* vm) {
    vm->PC = pop16(vm);

    /* Internal : Cycle sync */
    cyclesSync_4(vm);
}

static void retCondition(VM* vm, bool isTrue) {
    /* Branch decision cycles */
    cyclesSync_4(vm);

    if (isTrue) {
        vm->PC = pop16(vm);
        /* Cycles sync after setting PC */
        cyclesSync_4(vm);
    }
}

static void cpl(VM* vm) {
    vm->GPR[R8_A] = ~vm->GPR[R8_A];
    set_flag(vm, FLAG_N, 1);
    set_flag(vm, FLAG_H, 1);
}

/* Conditional Jumps (both relative and direct) */ 

#define CONDITION_NZ(vm) (get_flag(vm, FLAG_Z) != 1)
#define CONDITION_NC(vm) (get_flag(vm, FLAG_C) != 1)
#define CONDITION_Z(vm)  (get_flag(vm, FLAG_Z) == 1)
#define CONDITION_C(vm)  (get_flag(vm, FLAG_C) == 1)

static void jumpCondition(VM* vm, bool isTrue) {
    uint16_t address = read2Bytes_8C(vm);

    if (isTrue) {
	    /* Cycles sync after branch decision */
		cyclesSync_4(vm);
		vm->PC = address;
	}
}

static void jumpRelativeCondition(VM* vm, bool isTrue) {
    int8_t jumpCount = (int8_t)readByte_4C(vm);

    if (isTrue) {
        vm->PC += jumpCount;

        /* Cycles sync after incrementing PC */
        cyclesSync_4(vm);
    }
}

/* Procedure for the decimal adjust instruction (DAA) */

static void decimalAdjust(VM* vm) {
    uint8_t value = vm->GPR[R8_A];

    if (get_flag(vm, FLAG_N)) {
        /* Previous operation was subtraction */
        if (get_flag(vm, FLAG_H)) {
            value -= 0x06;
        }

        if (get_flag(vm, FLAG_C)) {
            value -= 0x60;
        }
    } else {
        /* Previous operation was addition */
        if (get_flag(vm, FLAG_H) || (value & 0xF) > 0x9) {
            uint8_t original = value;
            value += 0x6;

            if (original > value) set_flag(vm, FLAG_C, 1);
        }

        if (get_flag(vm, FLAG_C) || value > 0x9F) {
            value += 0x60;
            set_flag(vm, FLAG_C, 1);
        }
    }

    TEST_Z_FLAG(vm, value);
    set_flag(vm, FLAG_H, 0);

    vm->GPR[R8_A] = value;

    /* Implementation from 
     * 'https://github.com/guigzzz/GoGB/blob/master/backend/cpu_arithmetic.go#L349' */
}

/* This function is responsible for writing 1 byte to a memory address */

static void writeAddr(VM* vm, uint16_t addr, uint8_t byte) {
#ifdef DEBUG_MEM_LOGGING
    printf("Writing 0x%02x to address 0x%04x\n", byte, addr);
#endif
    if (addr >= RAM_NN_8KB && addr <= RAM_NN_8KB_END) {
        /* External RAM write request */
        mbc_writeExternalRAM(vm, addr, byte);
        return;
    } else if (addr >= ROM_N0_16KB && addr <= ROM_NN_16KB_END) {
        /* Pass over control to an MBC, maybe this is a call for 
         * bank switch */
        mbc_interceptROMWrite(vm, addr, byte); 
        return;
    } else if ((addr >= ECHO_N0_8KB && addr <= ECHO_N0_8KB_END) || 
            (addr >= UNUSABLE_N0 && addr <= UNUSABLE_N0_END)) {

        printf("[WARNING] Attempt to write to address 0x%x (read only)\n", addr);
        return;
    } else if (addr >= IO_REG && addr <= IO_REG_END) {
        /* We perform some actions before writing in some 
         * cases */
        switch (addr) {
            case R_TIMA : syncTimer(vm); break;
            case R_TMA  : syncTimer(vm); break;
            case R_TAC  : {
                syncTimer(vm); 
                
                /* Writing to TAC sometimes glitches TIMA,
                 * algorithm from 'https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html' */
                uint8_t oldTAC = vm->MEM[R_TAC];
				/* Make sure unused bits are unchanged */
                uint8_t newTAC = byte | 0xF8;

                vm->MEM[R_TAC] = newTAC;

                int clocks_array[] = {1024, 16, 64, 256};
                uint8_t old_clock = clocks_array[oldTAC & 3];
                uint8_t new_clock = clocks_array[newTAC & 3];

                uint8_t old_enable = (oldTAC >> 2) & 1;
                uint8_t new_enable = (newTAC >> 2) & 1;
                uint16_t sys_clock = vm->clock & 0xFFFF;

                bool glitch = false;
                if (old_enable != 0) {
                    if (new_enable == 0) {
                        glitch = (sys_clock & (old_clock / 2)) != 0;
                    } else {
                        glitch = ((sys_clock & (old_clock / 2)) != 0) && 
                                 ((sys_clock & (new_clock / 2)) == 0);
                    }
                }
                
                if (glitch) {
                    incrementTIMA(vm);
                }
                return;
            }
            case R_DIV  : {
                /* Write to DIV resets it */
                syncTimer(vm);

                if (vm->MEM[R_DIV] == 1 && ((vm->MEM[R_TAC] >> 2) & 1)) {
                    /* If DIV = 1 and timer is enabled, theres a bug in which
                     * the TIMA increases */
                    vm->MEM[R_DIV] = 0;
                    incrementTIMA(vm); 
                    return;
                }
                vm->MEM[R_DIV] = 0;
                return;
            }
            case R_SC:
#ifdef DEBUG_PRINT_SERIAL_OUTPUT
                if (byte == 0x81) {
                    printf("%c", vm->MEM[R_SB]);
                    vm->MEM[addr] = 0x00;
                }
#endif
                break;
			case R_P1_JOYP:
				/* Perform Checks */
				if ((byte & 0xF) != (vm->MEM[addr] & 0xF)) {
					/* Attempt to write to read only area,
					 * we ignore the value the user gave for the read only bits */
					byte &= 0xF;
					byte |= vm->MEM[addr] & 0xF;
				}
				
				/* Check bit 4-5to get selected mode */
				uint8_t selected = ((byte >> 4) & 0b11);
				vm->joypadSelectedMode = selected;
				
				/* Write the selected mode */
				vm->MEM[addr] = byte;
				
				/* Update register to show the keys for the new mode */
				updateJoypadRegBuffer(vm, selected);
				return;
			case R_SVBK: {
				/* In CGB Mode, switch WRAM banks */
				uint8_t oldBankNumber = vm->MEM[R_SVBK] & 0b00000111;
				uint8_t bankNumber = byte & 0b00000111;
				
				if (bankNumber == 0) bankNumber = 1;
				switchCGB_WRAM(vm, oldBankNumber, bankNumber);
				/* Ignore bits 7-3 */
				vm->MEM[R_SVBK] = byte | 0b11111000;
				return;	
			}
			case R_VBK: {
				/* In CGB Mode, switch VRAM banks 
				 *
				 * Only bit 0 matters */
				uint8_t oldBankNumber = vm->MEM[R_VBK] & 1;
				uint8_t bankNumber = byte & 1;
				
				switchCGB_VRAM(vm, oldBankNumber, bankNumber);
				/* Ignore all bits other than bit 0 */
				vm->MEM[R_VBK] = byte | ~1;
				return;
			}
			case R_BCPD:
			case R_BCPS:
			case R_OCPD:
			case R_OCPS:
				/* Handle the case when palettes have been locked by PPU */
				if (vm->lockPalettes) return;
				break;
        }
    } else if (addr >= VRAM_N0_8KB && addr <= VRAM_N0_8KB_END) {
		/* Handle the case when VRAM has been locked by PPU */
		if (vm->lockVRAM) return;
	} else if (addr >= OAM_N0_160B && addr <= OAM_N0_160B_END) {
		/* Handle the case when OAM has been locked by PPU */
		if (vm->lockOAM) return;
	}
    vm->MEM[addr] = byte; 
}

static uint8_t readAddr(VM* vm, uint16_t addr) {
    if (addr >= RAM_NN_8KB && addr <= RAM_NN_8KB_END) {
        /* Read from external RAM */
        return mbc_readExternalRAM(vm, addr);
    } else if (addr >= IO_REG && addr <= IO_REG_END) {
        /* If we have IO registers to read from, we perform some 
         * actions before the read is done in some cases 
		 *
		 * or modify the read values accordingly*/
        switch (addr) {
            case R_DIV  :
            case R_TIMA :
            case R_TMA  :
            case R_TAC  : syncTimer(vm); break;
			case R_BCPD:
			case R_BCPS:
			case R_OCPD:
			case R_OCPS:
				/* Handle the case when Palettes have been locked by the PPU */
				if (vm->lockPalettes) return 0xFF;
				break;
        }
    } else if (addr >= VRAM_N0_8KB && addr <= VRAM_N0_8KB_END) {
		/* Handle the case when VRAM has been locked by the PPU */
		if (vm->lockVRAM) return 0xFF;
	} else if (addr >= OAM_N0_160B && addr <= OAM_N0_160B_END) {
		/* Handle the case when OAM has been locked by the PPU */
		if (vm->lockOAM) return 0xFF;
	}

    return vm->MEM[addr];
}


static void writeAddr_4C(VM* vm, uint16_t addr, uint8_t byte) {
    writeAddr(vm, addr, byte);
    cyclesSync_4(vm);
}

static uint8_t readAddr_4C(VM* vm, uint16_t addr) {
    uint8_t byte = readAddr(vm, addr);
    cyclesSync_4(vm);

    return byte;
}

/* Interrupt handling and helper functions */

void requestInterrupt(VM* vm, INTERRUPT interrupt) {
    /* This is called by external hardware to request interrupts
     *
     * We set the corresponding bit */
    vm->MEM[R_IF] |= 1 << interrupt;
}

static void dispatchInterrupt(VM* vm, INTERRUPT interrupt) {
    /* Disable all interrupts */
    INTERRUPT_MASTER_DISABLE(vm);
    
    /* Set the bit of this interrupt in the IF register to 0 */
    vm->MEM[R_IF] &= ~(1 << interrupt);
    
    /* Now we pass control to the interrupt handler 
     *
     * CPU does nothing for 8 cycles (or executes NOPs) */
    cyclesSync_4(vm);
    cyclesSync_4(vm);

    /* Jump to interrupt vector */
    switch (interrupt) {
        case INTERRUPT_VBLANK:      call(vm, 0x40); break;
        case INTERRUPT_LCD_STAT:    call(vm, 0x48); break;
        case INTERRUPT_TIMER:       call(vm, 0x50); break;
        case INTERRUPT_SERIAL:      call(vm, 0x58); break;
        case INTERRUPT_JOYPAD:      call(vm, 0x60); break;

        default: break;
    }

}

static INTERRUPT getInterrupt(VM* vm) {

	return -1;
}

static void handleInterrupts(VM* vm) {
    /* Main interrupt handler for the CPU */
    
	/* Checks for an interrupt that can be handled and returns 
	 * it if found, otherwise returns -1 */

    /* We read interrupt flags, which tell us which interrupts are requested
     * if any */
    uint8_t requestedInterrupts = vm->MEM[R_IF];
    uint8_t enabledInterrups = vm->MEM[R_IE];

	if (vm->IME) {
		if ((enabledInterrups & requestedInterrupts & 0x1F) != 0) {
			/* Exit halt mode */
			vm->haltMode = false;
			/* Atleast 1 interrupt has been requested and is enabled too
			*
			* Note : Upper 3 bits should always be 0, this is handled when normally
			* writing to interrupt flag register and in normal hardware requests this is
			* never encountered 
			*
			* We loop to find the interrupt requested with the highest priority thats enabled */
	
		    for (int i = 0; i < INTERRUPT_COUNT; i++) {
			    /* 'i' corresponds to the individual interrupts ranging from bit 0 to 4 */

				uint8_t requestBit = (requestedInterrupts >> i) & 0x1;
				uint8_t enabledBit = (enabledInterrups >> i) & 0x1;

				if (requestBit && enabledBit) {
					/* 'i' is currently the interrupt at the highest priority thats enabled */
					dispatchInterrupt(vm, i);
					return;
				}
			}
		}
	} else {
		/* Even though the interrupt cannot be handled because IME is false,
		 * we still need to exit halt mode */
		if ((enabledInterrups & requestedInterrupts & 0x1F) != 0) {
			vm->haltMode = false;
		}
	}
}

static void halt(VM* vm) {
	/* HALT Instruction procedure */
	uint8_t IE = vm->MEM[R_IE];
	uint8_t IF = vm->MEM[R_IF];

	if (vm->IME) {
		if ((IE & IF & 0x1F) == 0) {
			/* IME Enabled, No enabled interrupts requested */
			vm->haltMode = true;	
			return;
		}	

		/* If an enabled interrupt is already requested 
		 * we normally exit */
		return;
	} else {

		if ((IE & IF & 0x1F) == 0) {
			/* IME Disabled, No enabled interrupts requested 
			 *
			 * We wait till an interrupt is requested, then we dont jump to the 
			 * interrupt vector and just continue executing instructions */
			vm->haltMode = true;
		} else {
			/* IME Disabled, 1 or more enabled interrupts requested
			 *
			 * Halt Bug Occurs */
			vm->scheduleHaltBug = true;
		}
	}
}

/* Main CPU instruction dispatchers */

static void prefixCB(VM* vm) {
    /* This function contains opcode interpretations for
     * all the instruction prefixed by opcode CB */
    uint8_t byte = readByte_4C(vm);
    
#ifdef DEBUG_PRINT_REGISTERS
    printRegisters(vm);
#endif
#ifdef DEBUG_REALTIME_PRINTING
    printCBInstruction(vm, byte);
#endif

    switch (byte) {
        case 0x00: rotateLeftR8(vm, R8_B, true); break;
        case 0x01: rotateLeftR8(vm, R8_C, true); break;
        case 0x02: rotateLeftR8(vm, R8_D, true); break;
        case 0x03: rotateLeftR8(vm, R8_E, true); break;
        case 0x04: rotateLeftR8(vm, R8_H, true); break;
        case 0x05: rotateLeftR8(vm, R8_L, true); break;
        case 0x06: rotateLeftAR16(vm, R16_HL, true); break;
        case 0x07: rotateLeftR8(vm, R8_A, true); break;
        case 0x08: rotateRightR8(vm, R8_B, true); break;
        case 0x09: rotateRightR8(vm, R8_C, true); break;
        case 0x0A: rotateRightR8(vm, R8_D, true); break;
        case 0x0B: rotateRightR8(vm, R8_E, true); break;
        case 0x0C: rotateRightR8(vm, R8_H, true); break;
        case 0x0D: rotateRightR8(vm, R8_L, true); break;
        case 0x0E: rotateRightAR16(vm, R16_HL, true); break;
        case 0x0F: rotateRightR8(vm, R8_A, true); break;
        case 0x10: rotateLeftCarryR8(vm, R8_B, true); break;
        case 0x11: rotateLeftCarryR8(vm, R8_C, true); break;
        case 0x12: rotateLeftCarryR8(vm, R8_D, true); break;
        case 0x13: rotateLeftCarryR8(vm, R8_E, true); break;
        case 0x14: rotateLeftCarryR8(vm, R8_H, true); break;
        case 0x15: rotateLeftCarryR8(vm, R8_L, true); break;
        case 0x16: rotateLeftCarryAR16(vm, R16_HL, true); break;
        case 0x17: rotateLeftCarryR8(vm, R8_A, true); break;
        case 0x18: rotateRightCarryR8(vm, R8_B, true); break;
        case 0x19: rotateRightCarryR8(vm, R8_C, true); break;
        case 0x1A: rotateRightCarryR8(vm, R8_D, true); break;
        case 0x1B: rotateRightCarryR8(vm, R8_E, true); break;
        case 0x1C: rotateRightCarryR8(vm, R8_H, true); break;
        case 0x1D: rotateRightCarryR8(vm, R8_L, true); break;
        case 0x1E: rotateRightCarryAR16(vm, R16_HL, true); break;
        case 0x1F: rotateRightCarryR8(vm, R8_A, true); break;
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

/* Instruction Set : https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html */

void dispatch(VM* vm) {
#ifdef DEBUG_PRINT_REGISTERS
        printRegisters(vm);
#endif
#ifdef DEBUG_REALTIME_PRINTING
        printInstruction(vm);
#endif
		uint8_t byte = 0; /* Will get set later */

        /* Enable interrupts if it was scheduled */
        if (vm->scheduleInterruptEnable) {
            vm->scheduleInterruptEnable = false;
            vm->IME = true;
        }

		if (vm->haltMode) {
			/* Skip dispatch and directly check for pending interrupts */

			/* The CPU is in sleep mode while halt mode is true,
			 * but the device clock will not be affected and continue 
			 * ticking 
			 *
			 * Other syncs will also continue taking place */
			cyclesSync_4(vm);
			goto skipLabel;
		} else if (vm->scheduleHaltBug) {
			/* Revert the PC increment */

			byte = readByte(vm);
			vm->PC--;
			cyclesSync_4(vm);
		} else {
			/* Normal Read */
			byte = readByte_4C(vm);	
		}
	
		/* Do the dispatch */
        switch (byte) {
            // nop
            case 0x00: break;
            case 0x01: LOAD_RR_D16(vm, R16_BC); break;
            case 0x02: LOAD_ARR_R(vm, R16_BC, R8_A); break;
            case 0x03: INC_RR(vm, R16_BC); break;
            case 0x04: incrementR8(vm, R8_B); break;
            case 0x05: decrementR8(vm, R8_B); break;
            case 0x06: LOAD_R_D8(vm, R8_B); break;
            case 0x07: rotateLeftR8(vm, R8_A, false); break;
            case 0x08: {
                uint16_t a = read2Bytes_8C(vm);
                uint16_t sp = get_reg16(vm, R16_SP);
                /* Write high byte to high and low byte to low */
                writeAddr_4C(vm, a+1, sp >> 8);
                writeAddr_4C(vm, a, sp & 0xFF);
                break;
            }
            case 0x09: addR16(vm, R16_HL, R16_BC); break;
            case 0x0A: LOAD_R_ARR(vm, R8_A, R16_BC); break;
            case 0x0B: DEC_RR(vm, R16_BC); break;
            case 0x0C: incrementR8(vm, R8_C); break;
            case 0x0D: decrementR8(vm, R8_C); break;
            case 0x0E: LOAD_R_D8(vm, R8_C); break;
            case 0x0F: rotateRightR8(vm, R8_A, false); break;
            /* OPCODE 10 TODO - STOP, it stops the CPU from running */
            case 0x10: break;
            case 0x11: LOAD_RR_D16(vm, R16_DE); break;
            case 0x12: LOAD_ARR_R(vm, R16_DE, R8_A); break;
            case 0x13: INC_RR(vm, R16_DE); break;
            case 0x14: incrementR8(vm, R8_D); break;
            case 0x15: decrementR8(vm, R8_D); break;
            case 0x16: LOAD_R_D8(vm, R8_D); break;
            case 0x17: rotateLeftCarryR8(vm, R8_A, false); break;
            case 0x18: JUMP_RL(vm, readByte_4C(vm)); break;
            case 0x19: addR16(vm, R16_HL, R16_DE); break;
            case 0x1A: LOAD_R_ARR(vm, R8_A, R16_DE); break;
            case 0x1B: DEC_RR(vm, R16_DE); break;
            case 0x1C: incrementR8(vm, R8_E); break;
            case 0x1D: decrementR8(vm, R8_E); break;
            case 0x1E: LOAD_R_D8(vm, R8_E); break;
            case 0x1F: rotateRightCarryR8(vm, R8_A, false); break;
            case 0x20: jumpRelativeCondition(vm, CONDITION_NZ(vm)); break;
            case 0x21: LOAD_RR_D16(vm, R16_HL); break;
            case 0x22: LOAD_ARR_R(vm, R16_HL, R8_A); 
                       set_reg16(vm, R16_HL, (get_reg16(vm, R16_HL) + 1));
                       break;
            case 0x23: INC_RR(vm, R16_HL); break;
            case 0x24: incrementR8(vm, R8_H); break;
            case 0x25: decrementR8(vm, R8_H); break;
            case 0x26: LOAD_R_D8(vm, R8_H); break;
            case 0x27: decimalAdjust(vm); break;
            case 0x28: jumpRelativeCondition(vm, CONDITION_Z(vm)); break;
            case 0x29: addR16(vm, R16_HL, R16_HL); break;
            case 0x2A: LOAD_R_ARR(vm, R8_A, R16_HL);
                       set_reg16(vm, R16_HL, (get_reg16(vm, R16_HL) + 1)); 
                       break;
            case 0x2B: DEC_RR(vm, R16_HL); break;
            case 0x2C: incrementR8(vm, R8_L); break;
            case 0x2D: decrementR8(vm, R8_L); break;
            case 0x2E: LOAD_R_D8(vm, R8_L); break;
            case 0x2F: cpl(vm); break;
            case 0x30: jumpRelativeCondition(vm, CONDITION_NC(vm)); break;
            case 0x31: LOAD_RR_D16(vm, R16_SP); break;
            case 0x32: LOAD_ARR_R(vm, R16_HL, R8_A); 
                       set_reg16(vm, R16_HL, (get_reg16(vm, R16_HL) - 1));
                       break;
            case 0x33: INC_RR(vm, R16_SP); break;
            case 0x34: {
                /* Increment what is at the address in HL */
                uint16_t address = get_reg16(vm, R16_HL);
                uint8_t old = readAddr_4C(vm, address);
                uint8_t new = old + 1; 
                
                TEST_Z_FLAG(vm, new);
                TEST_H_FLAG_ADD(vm, old, 1);
                set_flag(vm, FLAG_N, 0);
                writeAddr_4C(vm, address, new);
                break;
            }
            case 0x35: {
                /* Decrement what is at the address in HL */
                uint16_t address = get_reg16(vm, R16_HL);
                uint8_t old = readAddr_4C(vm, address);
                uint8_t new = old - 1; 

                TEST_Z_FLAG(vm, new);
                TEST_H_FLAG_SUB(vm, old, 1);
                set_flag(vm, FLAG_N, 1);
                writeAddr_4C(vm, address, new);
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
                set_reg16(vm, R16_HL, (get_reg16(vm, R16_HL) - 1));
                break;
            }
            case 0x3B: DEC_RR(vm, R16_SP); break;
            case 0x3C: incrementR8(vm, R8_A); break;
            case 0x3D: decrementR8(vm, R8_A); break;
            case 0x3E: LOAD_R_D8(vm, R8_A); break;
            case 0x3F: CCF(vm); break;
            case 0x40: LOAD_R_R(vm, R8_B, R8_B); 
#ifdef DEBUG_LDBB_BREAKPOINT 
                       exit(0); 
#endif                  
                       break;
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
            case 0x76: halt(vm); break;
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
            case 0xC3: JUMP(vm, read2Bytes_8C(vm)); break;
            case 0xC4: callCondition(vm, read2Bytes_8C(vm), CONDITION_NZ(vm)); break;
            case 0xC5: PUSH_R16(vm, R16_BC); break;
            case 0xC6: addR8D8(vm, R8_A); break;
            case 0xC7: RST(vm, 0x00); break;
            case 0xC8: retCondition(vm, CONDITION_Z(vm)); break;
            case 0xC9: ret(vm); break;
            case 0xCA: jumpCondition(vm, CONDITION_Z(vm)); break;
            case 0xCB: prefixCB(vm); break;
            case 0xCC: callCondition(vm, read2Bytes_8C(vm), CONDITION_Z(vm)); break;
            case 0xCD: call(vm, read2Bytes_8C(vm)); break;
            case 0xCE: adcR8D8(vm, R8_A); break;
            case 0xCF: RST(vm, 0x08); break;
            case 0xD0: retCondition(vm, CONDITION_NC(vm)); break;
            case 0xD1: POP_R16(vm, R16_DE); break;
            case 0xD2: jumpCondition(vm, CONDITION_NC(vm)); break;
            case 0xD4: callCondition(vm, read2Bytes_8C(vm), CONDITION_NC(vm)); break;
            case 0xD5: PUSH_R16(vm, R16_DE); break;
            case 0xD6: subR8D8(vm, R8_A); break;
            case 0xD7: RST(vm, 0x10); break;
            case 0xD8: retCondition(vm, CONDITION_C(vm)); break;
            case 0xD9: INTERRUPT_MASTER_ENABLE(vm); ret(vm); break;
            case 0xDA: jumpCondition(vm, CONDITION_C(vm)); break;
            case 0xDC: callCondition(vm, read2Bytes_8C(vm), CONDITION_C(vm)); break;
            case 0xDE: sbcR8D8(vm, R8_A); break;
            case 0xDF: RST(vm, 0x18); break;
            case 0xE0: LOAD_D8PORT_R(vm, R8_A); break;
            case 0xE1: POP_R16(vm, R16_HL); break;
            case 0xE2: LOAD_RPORT_R(vm, R8_A, R8_C); break;
            case 0xE5: PUSH_R16(vm, R16_HL); break;
            case 0xE6: andR8D8(vm, R8_A); break;
            case 0xE7: RST(vm, 0x20); break;
            case 0xE8: addR16I8(vm, R16_SP); break;
            case 0xE9: JUMP_RR(vm, R16_HL); break; 
            case 0xEA: LOAD_MEM_R(vm, R8_A); break;
            case 0xEE: xorR8D8(vm, R8_A); break;
            case 0xEF: RST(vm, 0x28); break;
            case 0xF0: LOAD_R_D8PORT(vm, R8_A); break;
            case 0xF1: {
                POP_R16(vm, R16_AF); 
                
                /* Always clear the lower 4 bits, they need to always
                 * be 0, we failed a blargg test because of this lol */
                vm->GPR[R8_F] &= 0xF0;
                break;
            }
            case 0xF2: LOAD_R_RPORT(vm, R8_A, R8_C); break;
            case 0xF3: INTERRUPT_MASTER_DISABLE(vm); break;
            case 0xF5: PUSH_R16(vm, R16_AF); break;
            case 0xF6: orR8D8(vm, R8_A); break;
            case 0xF7: RST(vm, 0x30); break;
            case 0xF8: LOAD_RR_RRI8(vm, R16_HL, R16_SP); break;
            case 0xF9: LOAD_RR_RR(vm, R16_SP, R16_HL); break;
            case 0xFA: LOAD_R_MEM(vm, R8_A); break;
            case 0xFB: INTERRUPT_MASTER_ENABLE(vm); break;
            case 0xFE: compareR8D8(vm, R8_A); break;
            case 0xFF: RST(vm, 0x38); break;
    }

skipLabel:
    /* We sync the timer after every dispatch just before checking for interrupts */
    syncTimer(vm);
    /* We handle any interrupts that are requested */
    handleInterrupts(vm);
}


