#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gb/gb.h>
#include <gb/cpu.h>

#define PORT_ADDR 0xFF00

/* Load 16 bit data into an R16 Register */
#define LOAD_RR_D16(gb, RR) set_reg16_8C(gb, RR, read2Bytes(gb))
/* Load contents of R8 Register into address at R16 register (dereferencing) */
#define LOAD_ARR_R(gb, RR, R) writeAddr_4C(gb, get_reg16(gb, RR), get_reg8(gb, R))
/* Load 8 bit data into R8 Register */
#define LOAD_R_D8(gb, R) set_reg8(gb, R, readByte(gb)); cyclesSync_4(gb)
/* Dereference the address contained in the R16 register and set it's value
 * to the R8 register */
#define LOAD_R_ARR(gb, R, RR) set_reg8(gb, R, readAddr(gb, get_reg16(gb, RR))); cyclesSync_4(gb)
/* Load 8 bit data into address at R16 register (dereferencing) */
#define LOAD_ARR_D8(gb, RR) writeAddr_4C(gb, get_reg16(gb, RR), readByte_4C(gb))
/* Load contents of R8 register into another R8 register */
#define LOAD_R_R(gb, R1, R2) set_reg8(gb, R1, get_reg8(gb, R2))
/* Load contents of R16 register into another R16 register */
#define LOAD_RR_RR(gb, RR1, RR2) set_reg16(gb, RR1, get_reg16(gb, RR2)); cyclesSync_4(gb)
/* Load instructions from reading into and writing into main memory */
#define LOAD_MEM_R(gb, R) writeAddr_4C(gb, read2Bytes_8C(gb), get_reg8(gb, R))
/* Load what's at the address specified by the 16 bit data into the R8 register */
#define LOAD_R_MEM(gb, R) set_reg8(gb, R, readAddr_4C(gb, read2Bytes_8C(gb)))
/* Load 'R' into '(PORT_ADDR + D8)' */
#define LOAD_D8PORT_R(gb, R) writeAddr_4C(gb, PORT_ADDR + readByte_4C(gb), get_reg8(gb, R))
/* Load '(PORT_ADDR + D8) into 'R' */
#define LOAD_R_D8PORT(gb, R) set_reg8(gb, R, readAddr_4C(gb, PORT_ADDR + readByte_4C(gb)))
/* Load 'R1' into '(PORT_ADDR + R2)' */
#define LOAD_RPORT_R(gb, R1, R2) writeAddr_4C(gb, PORT_ADDR + get_reg8(gb, R2), get_reg8(gb, R1))
/* Load '(PORT_ADDR + R2)' into 'R1' */
#define LOAD_R_RPORT(gb, R1, R2) set_reg8(gb, R1, readAddr_4C(gb, PORT_ADDR + get_reg8(gb, R2)))

#define LOAD_RR_RRI8(gb, RR1, RR2) load_rr_rri8(gb, RR1, RR2)
/* Increment contents of R16 register */
#define INC_RR(gb, RR) set_reg16(gb, RR, (get_reg16(gb, RR) + 1)); cyclesSync_4(gb)
/* Decrement contents of R16 register */
#define DEC_RR(gb, RR) set_reg16(gb, RR, (get_reg16(gb, RR) - 1)); cyclesSync_4(gb)

/* Direct Jump */
#define JUMP(gb, a16) gb->PC = a16; cyclesSync_4(gb)
#define JUMP_RR(gb, RR) gb->PC = get_reg16(gb, RR)
/* Relative Jump */
#define JUMP_RL(gb, i8) gb->PC += (int8_t)i8; cyclesSync_4(gb)

#define PUSH_R16(gb, RR) cyclesSync_4(gb); push16(gb, get_reg16(gb, RR))
#define POP_R16(gb, RR) set_reg16(gb, RR, pop16(gb))
#define RST(gb, a16) call(gb, a16)
#define INTERRUPT_MASTER_ENABLE(gb) gb->scheduleInterruptEnable = true
#define INTERRUPT_MASTER_DISABLE(gb) gb->IME = false
#define CCF(gb) set_flag(gb, FLAG_C, get_flag(gb, FLAG_C) ^ 1); \
    set_flag(gb, FLAG_N, 0);                        \
    set_flag(gb, FLAG_H, 0)
/* Flag utility macros */
#define TEST_Z_FLAG(gb, r) set_flag(gb, FLAG_Z, r == 0 ? 1 : 0)
/* We check if a carry over for the last 4 bits happened
 * ref : https://www.reddit.com/r/EmuDev/comments/4ycoix/a_guide_to_the_gameboys_halfcarry_flag*/
#define TEST_H_FLAG_ADD(gb, x, y) set_flag(gb, FLAG_H, \
        (((uint32_t)x & 0xf) + ((uint32_t)y & 0xf) > 0xf) ? 1 : 0)



/* half carry counts when theres carry from bit 11-12 for most 16 bit instructions */

#define TEST_H_FLAG_ADD16(gb, x, y) set_flag(gb, FLAG_H, \
        (((uint32_t)x & 0xfff) + ((uint32_t)y & 0xfff) > 0xfff) ? 1 : 0 )

#define TEST_H_FLAG_SUB(gb, x, y) set_flag(gb, FLAG_H, \
        (((x & 0xf) - (y & 0xf) & 0x10) ? 1 : 0))
/* Test for integer overloads and set carry flags */
#define TEST_C_FLAG_ADD16(gb, x, y) set_flag(gb, FLAG_C, ((uint32_t)(x) + (uint32_t)(y)) > 0xFFFF ? 1 : 0)
#define TEST_C_FLAG_ADD8(gb, x, y) set_flag(gb, FLAG_C, ((uint16_t)(x) + (uint16_t)(y)) > 0xFF ? 1 : 0)
#define TEST_C_FLAG_SUB16(gb, x, y) set_flag(gb, FLAG_C, ((int)(x) - (int)(y)) < 0 ? 1 : 0)
#define TEST_C_FLAG_SUB8(gb, x, y) set_flag(gb, FLAG_C, ((int)(x) - (int)(y)) < 0 ? 1 : 0)

static inline void writeAddr_4C(GB* gb, uint16_t addr, uint8_t byte);
static inline uint8_t readAddr_4C(GB* gb, uint16_t addr);

static inline uint8_t readByte(GB* gb) {
    /* Reads a byte and doesnt consume any cycles */
    return readAddr(gb, gb->PC++);
}

static inline uint8_t readByte_4C(GB* gb) {
    /* Reads a byte and consumes 4 cycles */
    return readAddr_4C(gb, gb->PC++);
}

static inline uint16_t read2Bytes(GB* gb) {
    /* Reads 2 bytes and doesnt consume any cycles */
    return (uint16_t)(readAddr(gb, gb->PC++) | readAddr(gb, gb->PC++) << 8);
}

static uint16_t read2Bytes_8C(GB* gb) {
    /* Reads 2 bytes and consumes 8 cycles, 4 per byte */
    return (uint16_t)(readAddr_4C(gb, gb->PC++) | (readAddr_4C(gb, gb->PC++) << 8));
}

static inline uint8_t get_reg8(GB* gb, GP_REG R) {
    return gb->GPR[R];
}

static inline void inc_reg8(GB* gb, GP_REG R) {
    gb->GPR[R]++;
}

static inline void dec_reg8(GB* gb, GP_REG R) {
    gb->GPR[R]--;
}

static inline void set_reg8(GB* gb, GP_REG R, uint8_t value) {
    gb->GPR[R] = value;
}

static inline void set_flag(GB* gb, FLAG flag, uint8_t bit) {
    /* Since the flags are enums and in order, their numeric value
     * can be used to decide which bit to modify
     * We set the flag's corresponding bit to 1 */
    if (bit) set_reg8(gb, R8_F, get_reg8(gb, R8_F) | (1 << (flag + 4)));
    /* Otherwise 0 */
    else set_reg8(gb, R8_F, get_reg8(gb, R8_F) & ~(1 << (flag + 4)));
}

static inline uint8_t get_flag(GB* gb, FLAG flag) {
    return (get_reg8(gb, R8_F) >> (flag + 4)) & 1;
}

/* Retrives the value of the 16 bit registers by combining the values
 * of its 8 bit registers */
static inline uint16_t get_reg16(GB* gb, GP_REG RR) {
    return ((get_reg8(gb, RR) << 8) | get_reg8(gb, RR + 1));
}

/* Sets 16 bit register by modifying the individual 8 bit registers
 * it consists of */
static inline uint16_t set_reg16(GB* gb, GP_REG RR, uint16_t v) {
    set_reg8(gb, RR, v >> 8);
    set_reg8(gb, RR + 1, v & 0xFF);

    return v;
}

static uint16_t set_reg16_8C(GB* gb, GP_REG RR, uint16_t v) {
    /* Takes 8 clock cycles */
    set_reg8(gb, RR, v >> 8);
    cyclesSync_4(gb);

    set_reg8(gb, RR + 1, v & 0xFF);
    cyclesSync_4(gb);
    return v;
}

void resetGBC(GB* gb) {
    gb->PC = 0x0100;
    set_reg16(gb, R16_SP, 0xFFFE);
    set_reg8(gb, R8_A, 0x11);
    set_reg8(gb, R8_F, 0b10000000);
    set_reg8(gb, R8_B, 0x00);
    set_reg8(gb, R8_C, 0x00);
    set_reg8(gb, R8_D, 0xFF);
    set_reg8(gb, R8_E, 0x56);
    set_reg8(gb, R8_H, 0x00);
    set_reg8(gb, R8_L, 0x0D);

    /* Set hardware registers and initialise empty areas */

    /* These registers are set to values that are recoreded at PC = 0x0100
     * this essentially means we dont need to emulate the boot rom */

    uint8_t hreg_defaults[80] = {
        0xCF, 0x00, 0x7F, 0xFF, 0xAC, 0x00, 0x00, 0xF8,                 // 0xFF00 - 0xFF07
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE1,                 // 0xFF08 - 0xFF0F
        0x80, 0xBF, 0xF3, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,                 // 0xFF10 - 0xFF17
        0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,                 // 0xFF18 - 0xFF1F
        0xFF, 0x00, 0x00, 0xBF, 0x77, 0xF3, 0xF1, 0xFF,                 // 0xFF20 - 0xFF27
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF28 - 0xFF2F
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF30 - 0xFF37
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF38 - 0xFF3F
        0x91, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC,                 // 0xFF40 - 0xFF47
        0xFF, 0xFF, 0x00, 0x00, 0x7E, 0xFF, 0xFF, 0xFF,                 // 0xFF48 - 0xFF4F,
    };

    for (int i = 0x00; i < 0x50; i++) {
        gb->IO[i] = hreg_defaults[i];
    }

    /* Reset all background colors to white (ffff) */
    memset(gb->bgColorRAM, 0xFF, 64);
    memset(gb->spriteColorRAM, 0xFF, 64);
    /* 0xFF50 to 0xFFFE = 0xFF */
    memset(&gb->IO[0x50], 0xFF, 0xAF);
	gb->IO[R_SVBK] = 0xF9; 					// Default bank
	gb->IO[R_VBK] = 0xFE;
	gb->IO[R_OPRI] = 0x01; 					// CGB Obj priority
	gb->IO[R_KEY0] = 0x00; 					// CGB Mode
    INTERRUPT_MASTER_DISABLE(gb);
}

void resetGB(GB* gb) {
    gb->PC = 0x0100;
    set_reg16(gb, R16_SP, 0xFFFE);
    set_reg8(gb, R8_A, 0x01);
    set_reg8(gb, R8_B, 0x00);
    set_reg8(gb, R8_C, 0x13);
    set_reg8(gb, R8_D, 0x00);
    set_reg8(gb, R8_E, 0xD8);
    set_reg8(gb, R8_H, 0x01);
    set_reg8(gb, R8_L, 0x4D);
    /* On a DMG, the value of F depends on the header checksum */
    set_reg8(gb, R8_F, gb->cartridge->headerChecksum == 0 ? 0xF0 : 0xB0);

    uint8_t hreg_defaults[80] = {
        0xCF, 0x00, 0x7E, 0xFF, 0xAB, 0x00, 0x00, 0xF8,                 // 0xFF00 - 0xFF07
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE1,                 // 0xFF08 - 0xFF0F
        0x80, 0xBF, 0xF3, 0xFF, 0xBF, 0xFF, 0x3F, 0x00,                 // 0xFF10 - 0xFF17
        0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF,                 // 0xFF18 - 0xFF1F
        0xFF, 0x00, 0x00, 0xBF, 0x77, 0xF3, 0xF1, 0xFF,                 // 0xFF20 - 0xFF27
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF28 - 0xFF2F
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF30 - 0xFF37
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF38 - 0xFF3F
        0x91, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC,                 // 0xFF40 - 0xFF47
        0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,                 // 0xFF48 - 0xFF4F,
    };

    for (int i = 0x00; i < 0x50; i++) {
        gb->IO[i] = hreg_defaults[i];
    }

    /* 0xFF50 to 0xFFFE = 0xFF */
    memset(&gb->IO[0x50], 0xFF, 0xAF);
    INTERRUPT_MASTER_DISABLE(gb);
}

static void load_rr_rri8(GB* gb, GP_REG RR1, GP_REG RR2) {
    /* Opcode 0xF8 specific
     *
     * RR1 = RR2 + I8 */
    uint16_t old = get_reg16(gb, RR2);
    int8_t toAdd = (int8_t)readByte_4C(gb);
    uint16_t result = set_reg16(gb, RR1, old + toAdd);

    set_flag(gb, FLAG_Z, 0);
    set_flag(gb, FLAG_N, 0);

    /* For some reason this 16 bit addition does not do normal
     * 16 bit half carry setting, so we do the 8 bit test */
    old &= 0xFF;

    /* We passed unsigned version to flag tests to handle
     * flags because at the low level its basically the same thing */
    TEST_H_FLAG_ADD(gb, old, (uint8_t)toAdd);
    TEST_C_FLAG_ADD8(gb, old, (uint8_t)toAdd);

    /* Internal */
    cyclesSync_4(gb);
}

static void incrementR8(GB* gb, GP_REG R) {
    /* A utility function which does incrementing for 8 bit registers
     * it does all flag updates it needs to */
    uint8_t old = get_reg8(gb, R);
    inc_reg8(gb, R);

    TEST_Z_FLAG(gb, get_reg8(gb, R));
    TEST_H_FLAG_ADD(gb, old, 1);
    set_flag(gb, FLAG_N, 0);
}

static void decrementR8(GB* gb, GP_REG R) {
    /* Decrementing for 8 bit registers */
    uint8_t old = get_reg8(gb, R);
    dec_reg8(gb, R);

    TEST_Z_FLAG(gb, get_reg8(gb, R));
    set_flag(gb, FLAG_N, 1);
    TEST_H_FLAG_SUB(gb, old, 1);
}

/* The folloing functions are used for all 8 bit rotation
 * operations */

static void rotateLeftR8(GB* gb, GP_REG R, bool setZFlag) {
    /* Rotates value to left, moves 7th bit to bit 0 and C flag */
    uint8_t toModify = get_reg8(gb, R);
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= bit7;

    set_reg8(gb, R, toModify);
    if (setZFlag) {
        TEST_Z_FLAG(gb, toModify);
    } else {
        set_flag(gb, FLAG_Z, 0);
    }
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit7);
}

static void rotateLeftAR16(GB* gb, GP_REG R16, bool setZFlag) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t toModify = readAddr_4C(gb, addr);
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= bit7;

    writeAddr_4C(gb, addr, toModify);
    if (setZFlag) {
        TEST_Z_FLAG(gb, toModify);
    } else {
        set_flag(gb, FLAG_Z, 0);
    }
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit7);
}

static void rotateRightR8(GB* gb, GP_REG R, bool setZFlag) {
    /* Rotates value to right, moves bit 0 to bit 7 and C flag */
    uint8_t toModify = get_reg8(gb, R);
    uint8_t bit1 = toModify & 1;

    toModify >>= 1;
    toModify |= bit1 << 7;

    set_reg8(gb, R, toModify);

    if (setZFlag) {
        TEST_Z_FLAG(gb, toModify);
    } else {
        set_flag(gb, FLAG_Z, 0);
    }
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit1);
}

static void rotateRightAR16(GB* gb, GP_REG R16, bool setZFlag) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t toModify = readAddr_4C(gb, addr);
    uint8_t bit1 = toModify & 1;

    toModify >>= 1;
    toModify |= bit1 << 7;

    writeAddr_4C(gb, addr, toModify);

    if (setZFlag) TEST_Z_FLAG(gb, toModify);
    else set_flag(gb, FLAG_Z, 0);

    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit1);
}

static void rotateLeftCarryR8(GB* gb, GP_REG R8, bool setZFlag) {
    /* Rotates value to left, moves bit 7 to C flag and C flag's original value
     * to bit 0 */
    uint8_t toModify = get_reg8(gb, R8);
    bool carryFlag = get_flag(gb, FLAG_C);
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= carryFlag;

    set_reg8(gb, R8, toModify);

    if (setZFlag) TEST_Z_FLAG(gb, toModify);
    else set_flag(gb, FLAG_Z, 0);

    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit7);
}

static void rotateLeftCarryAR16(GB* gb, GP_REG R16, bool setZFlag) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t toModify = readAddr_4C(gb, addr);
    bool carryFlag = get_flag(gb, FLAG_C);
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= carryFlag;

    writeAddr_4C(gb, addr, toModify);

    if (setZFlag) TEST_Z_FLAG(gb, toModify);
    else set_flag(gb, FLAG_Z, 0);

    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit7);
}

static void rotateRightCarryR8(GB* gb, GP_REG R8, bool setZFlag) {
    /* Rotates value to right, moves bit 0 to C flag and C flag's original value
     * to bit 7 */
    uint8_t toModify = get_reg8(gb, R8);
    bool carryFlag = get_flag(gb, FLAG_C);
    uint8_t bit0 = toModify & 1;

    toModify >>= 1;
    toModify |= carryFlag << 7;

    set_reg8(gb, R8, toModify);

    if (setZFlag) TEST_Z_FLAG(gb, toModify);
    else set_flag(gb, FLAG_Z, 0);

    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit0);
}

static void rotateRightCarryAR16(GB* gb, GP_REG R16, bool setZFlag) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t toModify = readAddr_4C(gb, addr);
    bool carryFlag = get_flag(gb, FLAG_C);
    uint8_t bit0 = toModify & 1;

    toModify >>= 1;
    toModify |= carryFlag << 7;

    writeAddr_4C(gb, addr, toModify);

    if (setZFlag) TEST_Z_FLAG(gb, toModify);
    else set_flag(gb, FLAG_Z, 0);

    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit0);
}

static void shiftLeftArithmeticR8(GB* gb, GP_REG R) {
    uint8_t value = get_reg8(gb, R);
    uint8_t bit7 = value >> 7;
    uint8_t result = value << 1;

    set_reg8(gb, R, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit7);
}

static void shiftLeftArithmeticAR16(GB* gb, GP_REG R16) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t value = readAddr_4C(gb, addr);
    uint8_t bit7 = value >> 7;
    uint8_t result = value << 1;

    writeAddr_4C(gb, addr, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit7);
}

static void shiftRightLogicalR8(GB* gb, GP_REG R) {
    uint8_t value = get_reg8(gb, R);
    uint8_t bit1 = value & 0x1;
    uint8_t result = value >> 1;

    set_reg8(gb, R, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit1);
}

static void shiftRightLogicalAR16(GB* gb, GP_REG R16) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t value = readAddr_4C(gb, addr);
    uint8_t bit1 = value & 0x1;
    uint8_t result = value >> 1;

    writeAddr_4C(gb, addr, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit1);
}

static void shiftRightArithmeticR8(GB* gb, GP_REG R) {
    uint8_t value = get_reg8(gb, R);
    uint8_t bit7 = value >> 7;
    uint8_t bit0 = value & 0x1;
    uint8_t result = value >> 1;

    /* Copy the 7th bit to its original location after the shift */
    result |= bit7 << 7;
    set_reg8(gb, R, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit0);
}

static void shiftRightArithmeticAR16(GB* gb, GP_REG R16) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t value = readAddr_4C(gb, addr);
    uint8_t bit7 = value >> 7;
    uint8_t bit0 = value & 0x1;
    uint8_t result = value >> 1;

    /* Copy the 7th bit to its original location after the shift */
    result |= bit7 << 7;
    writeAddr_4C(gb, addr, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, bit0);
}

static void swapR8(GB* gb, GP_REG R8) {
    uint8_t value = get_reg8(gb, R8);
    uint8_t highNibble = value >> 4;
    uint8_t lowNibble = value & 0xF;

    uint8_t newValue = (lowNibble << 4) | highNibble;

    set_reg8(gb, R8, newValue);

    TEST_Z_FLAG(gb, newValue);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, 0);
}

static void swapAR16(GB* gb, GP_REG R16) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t value = readAddr_4C(gb, addr);
    uint8_t highNibble = value >> 4;
    uint8_t lowNibble = value & 0xF;

    uint8_t newValue = (lowNibble << 4) | highNibble;

    writeAddr_4C(gb, addr, newValue);

    TEST_Z_FLAG(gb, newValue);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_C, 0);
}

static void testBitR8(GB* gb, GP_REG R8, uint8_t bit) {
    uint8_t value = get_reg8(gb, R8);
    uint8_t bitValue = (value >> bit) & 0x1;

    TEST_Z_FLAG(gb, bitValue);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 1);
}

static void testBitAR16(GB* gb, GP_REG R16, uint8_t bit) {
    uint8_t value = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t bitValue = (value >> bit) & 0x1;

    TEST_Z_FLAG(gb, bitValue);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 1);
}

static void setBitR8(GB* gb, GP_REG R8, uint8_t bit) {
    uint8_t value = get_reg8(gb, R8);
    uint8_t orValue = 1 << bit;
    uint8_t result = value | orValue;

    set_reg8(gb, R8, result);
}

static void setBitAR16(GB* gb, GP_REG R16, uint8_t bit) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t value = readAddr_4C(gb, addr);
    uint8_t orValue = 1 << bit;
    uint8_t result = value | orValue;

    writeAddr_4C(gb, addr, result);
}

static void resetBitR8(GB* gb, GP_REG R8, uint8_t bit) {
    uint8_t value = get_reg8(gb, R8);
    /* Converts 00010000 to 11101111 for the andValue when bit 3 has to be reset for ex */
    uint8_t andValue = ~(1 << bit);
    uint8_t result = value & andValue;

    set_reg8(gb, R8, result);
}

static void resetBitAR16(GB* gb, GP_REG R16, uint8_t bit) {
    uint16_t addr = get_reg16(gb, R16);
    uint8_t value = readAddr_4C(gb, addr);
    uint8_t andValue = ~(1 << bit);
    uint8_t result = value & andValue;

    writeAddr_4C(gb, addr, result);
}

/* The following functions form the most of the arithmetic and logical
 * operations of the CPU */

static void addR16(GB* gb, GP_REG RR1, GP_REG RR2) {
    uint16_t old = get_reg16(gb, RR1);
    uint16_t toAdd = get_reg16(gb, RR2);
    uint16_t result = set_reg16(gb, RR1, old + toAdd);

    set_flag(gb, FLAG_N, 0);
    TEST_H_FLAG_ADD16(gb, old, toAdd);
    TEST_C_FLAG_ADD16(gb, old, toAdd);

    cyclesSync_4(gb);
}

/* Adding a signed 8 bit integer to a 16 bit register */

static void addR16I8(GB* gb, GP_REG RR) {
    uint16_t old = get_reg16(gb, RR);
    int8_t toAdd = (int8_t)readByte_4C(gb);
    uint16_t result = set_reg16_8C(gb, RR, old + toAdd);
    set_flag(gb, FLAG_Z, 0);
    set_flag(gb, FLAG_N, 0);

    /* For some reason this 16 bit addition does not do normal
     * 16 bit half carry setting, so we do the 8 bit test */
    old &= 0xFF;

    /* We passed unsigned version to flag tests to handle
     * flags because at the low level its basically the same thing */
    TEST_H_FLAG_ADD(gb, old, (uint8_t)toAdd);
    TEST_C_FLAG_ADD8(gb, old, (uint8_t)toAdd);
}

static void addR8(GB* gb, GP_REG R1, GP_REG R2) {
    uint8_t old = get_reg8(gb, R1);
    uint8_t toAdd = get_reg8(gb, R2);
    uint8_t result = old + toAdd;

    set_reg8(gb, R1, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    TEST_H_FLAG_ADD(gb, old, toAdd);
    TEST_C_FLAG_ADD8(gb, old, toAdd);
}

static void addR8D8(GB* gb, GP_REG R) {
    uint8_t data = readByte_4C(gb);
    uint8_t old = get_reg8(gb, R);
    uint8_t result = old + data;

    set_reg8(gb, R, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    TEST_H_FLAG_ADD(gb, old, data);
    TEST_C_FLAG_ADD8(gb, old, data);
}

static void addR8_AR16(GB* gb, GP_REG R8, GP_REG R16) {
    uint8_t old = get_reg8(gb, R8);
    uint8_t toAdd = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t result = old + toAdd;

    set_reg8(gb, R8, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    TEST_H_FLAG_ADD(gb, old, toAdd);
    TEST_C_FLAG_ADD8(gb, old, toAdd);
}

static void test_adc_hcflags(GB* gb, uint8_t old, uint8_t toAdd, uint8_t result, uint8_t carry) {
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

    set_flag(gb, FLAG_H, halfCarryOccurred);
    set_flag(gb, FLAG_C, carryOccured);
}

static void adcR8(GB* gb, GP_REG R1, GP_REG R2) {
    uint8_t old = get_reg8(gb, R1);
    uint8_t toAdd = get_reg8(gb, R2);
    uint8_t carry = get_flag(gb, FLAG_C);
    uint8_t result = old + toAdd;
    uint8_t finalResult = result + carry;

    set_reg8(gb, R1, finalResult);

    TEST_Z_FLAG(gb, finalResult);
    set_flag(gb, FLAG_N, 0);
    test_adc_hcflags(gb, old, toAdd, result, carry);
}

static void adcR8D8(GB* gb, GP_REG R) {
    uint16_t data = (uint16_t)readByte_4C(gb);
    uint8_t old = get_reg8(gb, R);
    uint8_t carry = get_flag(gb, FLAG_C);
    uint8_t result = old + data;
    uint8_t finalResult = result + carry;

    set_reg8(gb, R, finalResult);

    TEST_Z_FLAG(gb, finalResult);
    set_flag(gb, FLAG_N, 0);
    test_adc_hcflags(gb, old, data, result, carry);
}

static void adcR8_AR16(GB* gb, GP_REG R8, GP_REG R16) {
    uint8_t old = get_reg8(gb, R8);
    uint8_t toAdd = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t carry = get_flag(gb, FLAG_C);
    uint8_t result = old + toAdd;
    uint8_t finalResult = result + carry;

    set_reg8(gb, R8, finalResult);

    TEST_Z_FLAG(gb, finalResult);
    set_flag(gb, FLAG_N, 0);
    test_adc_hcflags(gb, old, toAdd, result, carry);
}

static void subR8(GB* gb, GP_REG R1, GP_REG R2) {
    uint8_t old = get_reg8(gb, R1);
    uint8_t toSub = get_reg8(gb, R2);
    uint8_t result = old - toSub;

    set_reg8(gb, R1, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 1);
    TEST_H_FLAG_SUB(gb, old, toSub);
    TEST_C_FLAG_SUB8(gb, old, toSub);
}

static void subR8D8(GB* gb, GP_REG R) {
    uint8_t data = readByte_4C(gb);
    uint8_t old = get_reg8(gb, R);
    uint8_t result = old - data;

    set_reg8(gb, R, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 1);
    TEST_H_FLAG_SUB(gb, old, data);
    TEST_C_FLAG_SUB8(gb, old, data);
}

static void subR8_AR16(GB* gb, GP_REG R8, GP_REG R16) {
    uint8_t old = get_reg8(gb, R8);
    uint8_t toSub = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t result = old - toSub;

    set_reg8(gb, R8, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 1);
    TEST_H_FLAG_SUB(gb, old, toSub);
    TEST_C_FLAG_SUB8(gb, old, toSub);
}

static void test_sbc_hcflags(GB* gb, uint8_t old, uint8_t toSub, uint8_t result, uint8_t carry) {
    /* SBC is also similar to ADC so we need a custom test here aswell */
    bool halfCarryOccurred = false;
    bool carryOccurred = false;

    uint8_t oldLow = old & 0xF;
    uint8_t resultLow = result & 0xF;

    if ((uint8_t)(oldLow - (toSub & 0xF)) > oldLow) halfCarryOccurred = true;
    if ((uint8_t)(resultLow - carry) > resultLow) halfCarryOccurred = true;

    if ((uint8_t)((uint16_t)old - (uint16_t)toSub) > old) carryOccurred = true;
    if ((uint8_t)((uint16_t)result - carry) > result) carryOccurred = true;

    set_flag(gb, FLAG_H, halfCarryOccurred);
    set_flag(gb, FLAG_C, carryOccurred);
}

static void sbcR8(GB* gb, GP_REG R1, GP_REG R2) {
    uint8_t old = get_reg8(gb, R1);
    uint8_t toSub = get_reg8(gb, R2);
    uint8_t carry = get_flag(gb, FLAG_C);
    uint8_t result = old - toSub;
    uint8_t finalResult = result - carry;

    set_reg8(gb, R1, finalResult);

    TEST_Z_FLAG(gb, finalResult);
    set_flag(gb, FLAG_N, 1);
    test_sbc_hcflags(gb, old, toSub, result, carry);
}

static void sbcR8D8(GB* gb, GP_REG R) {
    uint8_t data = readByte_4C(gb);
    uint8_t old = get_reg8(gb, R);
    uint8_t carry = get_flag(gb, FLAG_C);
    uint8_t result = old - data;
    uint8_t finalResult = result - carry;

    set_reg8(gb, R, finalResult);

    TEST_Z_FLAG(gb, finalResult);
    set_flag(gb, FLAG_N, 1);
    test_sbc_hcflags(gb, old, data, result, carry);
}

static void sbcR8_AR16(GB* gb, GP_REG R8, GP_REG R16) {
    uint8_t old = get_reg8(gb, R8);
    uint8_t toSub = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t carry = get_flag(gb, FLAG_C);
    uint8_t result = old - toSub;
    uint8_t finalResult = result - carry;

    set_reg8(gb, R8, finalResult);

    TEST_Z_FLAG(gb, finalResult);
    set_flag(gb, FLAG_N, 1);
    test_sbc_hcflags(gb, old, toSub, result, carry);
}

static void andR8(GB* gb, GP_REG R1, GP_REG R2) {
    uint8_t old = get_reg8(gb, R1);
    uint8_t operand = get_reg8(gb, R2);
    uint8_t result = old & operand;

    set_reg8(gb, R1, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 1);
    set_flag(gb, FLAG_C, 0);
}

static void andR8D8(GB* gb, GP_REG R) {
    uint8_t old = get_reg8(gb, R);
    uint8_t operand = readByte_4C(gb);
    uint8_t result = old & operand;

    set_reg8(gb, R, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 1);
    set_flag(gb, FLAG_C, 0);
}

static void andR8_AR16(GB* gb, GP_REG R8, GP_REG R16) {
    uint8_t old = get_reg8(gb, R8);
    uint8_t operand = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t result = old & operand;

    set_reg8(gb, R8, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 1);
    set_flag(gb, FLAG_C, 0);
}

static void xorR8(GB* gb, GP_REG R1, GP_REG R2) {
    uint8_t old = get_reg8(gb, R1);
    uint8_t operand = get_reg8(gb, R2);
    uint8_t result = old ^ operand;

    set_reg8(gb, R1, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_C, 0);
}

static void xorR8D8(GB* gb, GP_REG R) {
    uint8_t old = get_reg8(gb, R);
    uint8_t operand = readByte_4C(gb);
    uint8_t result = old ^ operand;

    set_reg8(gb, R, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_C, 0);
}

static void xorR8_AR16(GB* gb, GP_REG R8, GP_REG R16) {
    uint8_t old = get_reg8(gb, R8);
    uint8_t operand = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t result = old ^ operand;

    set_reg8(gb, R8, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_C, 0);
}

static void orR8(GB* gb, GP_REG R1, GP_REG R2) {
    uint8_t old = get_reg8(gb, R1);
    uint8_t operand = get_reg8(gb, R2);
    uint8_t result = old | operand;

    set_reg8(gb, R1, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_C, 0);
}

static void orR8D8(GB* gb, GP_REG R) {
    uint8_t old = get_reg8(gb, R);
    uint8_t operand = readByte_4C(gb);
    uint8_t result = old | operand;

    set_reg8(gb, R, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_C, 0);
}

static void orR8_AR16(GB* gb, GP_REG R8, GP_REG R16) {
    uint8_t old = get_reg8(gb, R8);
    uint8_t operand = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t result = old | operand;

    set_reg8(gb, R8, result);

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 0);
    set_flag(gb, FLAG_H, 0);
    set_flag(gb, FLAG_C, 0);
}

static void compareR8(GB* gb, GP_REG R1, GP_REG R2) {
    /* Basically R1 - R2, but results are thrown away and R1 is unchanged */
    uint8_t old = get_reg8(gb, R1);
    uint8_t toSub = get_reg8(gb, R2);
    uint8_t result = old - toSub;

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 1);
    TEST_H_FLAG_SUB(gb, old, toSub);
    TEST_C_FLAG_SUB8(gb, old, toSub);
}

static void compareR8D8(GB* gb, GP_REG R) {
    uint8_t old = get_reg8(gb, R);
    uint8_t toSub = readByte_4C(gb);
    uint8_t result = old - toSub;

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 1);
    TEST_H_FLAG_SUB(gb, old, toSub);
    TEST_C_FLAG_SUB8(gb, old, toSub);
}

static void compareR8_AR16(GB* gb, GP_REG R8, GP_REG R16) {
    uint8_t old = get_reg8(gb, R8);
    uint8_t toSub = readAddr_4C(gb, get_reg16(gb, R16));
    uint8_t result = old - toSub;

    TEST_Z_FLAG(gb, result);
    set_flag(gb, FLAG_N, 1);
    TEST_H_FLAG_SUB(gb, old, toSub);
    TEST_C_FLAG_SUB8(gb, old, toSub);
}

/* The following functions are responsible for returning, calling & stack manipulation*/

static inline void push16(GB* gb, uint16_t u16) {
    /* Pushes a 2 byte value to the stack */
    uint16_t stackPointer = get_reg16(gb, R16_SP);

    /* Write the high byte */
    writeAddr_4C(gb, stackPointer - 1, u16 >> 8);
    /* Write the low byte */
    writeAddr_4C(gb, stackPointer - 2, u16 & 0xFF);

    /* Update the SP */
    set_reg16(gb, R16_SP, stackPointer - 2);
}

static inline uint16_t pop16(GB* gb) {
    uint16_t stackPointer = get_reg16(gb, R16_SP);

    uint8_t lowByte = readAddr_4C(gb, stackPointer);
    uint8_t highByte = readAddr_4C(gb, stackPointer + 1);
    set_reg16(gb, R16_SP, stackPointer + 2);

    return (uint16_t)((highByte << 8) | lowByte);
}

static void call(GB* gb, uint16_t addr) {
    /* Branch decision cycle sync */
    cyclesSync_4(gb);

    push16(gb, gb->PC);
    gb->PC = addr;
}

static void callCondition(GB* gb, uint16_t addr, bool isTrue) {
    if (isTrue) {
        /* Branch decision cycles sync */
        cyclesSync_4(gb);
        push16(gb, gb->PC);
        gb->PC = addr;
    }
}

static inline void ret(GB* gb) {
    gb->PC = pop16(gb);

    /* Internal : Cycle sync */
    cyclesSync_4(gb);
}

static void retCondition(GB* gb, bool isTrue) {
    /* Branch decision cycles */
    cyclesSync_4(gb);

    if (isTrue) {
        gb->PC = pop16(gb);
        /* Cycles sync after setting PC */
        cyclesSync_4(gb);
    }
}

static void cpl(GB* gb) {
    set_reg8(gb, R8_A, ~get_reg8(gb, R8_A));
    set_flag(gb, FLAG_N, 1);
    set_flag(gb, FLAG_H, 1);
}

/* Conditional Jumps (both relative and direct) */

#define CONDITION_NZ(gb) (get_flag(gb, FLAG_Z) != 1)
#define CONDITION_NC(gb) (get_flag(gb, FLAG_C) != 1)
#define CONDITION_Z(gb)  (get_flag(gb, FLAG_Z) == 1)
#define CONDITION_C(gb)  (get_flag(gb, FLAG_C) == 1)

static void jumpCondition(GB* gb, bool isTrue) {
    uint16_t address = read2Bytes_8C(gb);

    if (isTrue) {
        /* Cycles sync after branch decision */
        cyclesSync_4(gb);
        gb->PC = address;
    }
}

static void jumpRelativeCondition(GB* gb, bool isTrue) {
    int8_t jumpCount = (int8_t)readByte_4C(gb);
    if (isTrue) {
        gb->PC += jumpCount;

        /* Cycles sync after incrementing PC */
        cyclesSync_4(gb);
    }
}

/* Procedure for the decimal adjust instruction (DAA) */

static void decimalAdjust(GB* gb) {
    uint8_t value = get_reg8(gb, R8_A);

    if (get_flag(gb, FLAG_N)) {
        /* Previous operation was subtraction */
        if (get_flag(gb, FLAG_H)) {
            value -= 0x06;
        }

        if (get_flag(gb, FLAG_C)) {
            value -= 0x60;
        }
    } else {
        /* Previous operation was addition */
        if (get_flag(gb, FLAG_H) || (value & 0xF) > 0x9) {
            uint8_t original = value;
            value += 0x6;

            if (original > value) set_flag(gb, FLAG_C, 1);
        }

        if (get_flag(gb, FLAG_C) || value > 0x9F) {
            value += 0x60;
            set_flag(gb, FLAG_C, 1);
        }
    }

    TEST_Z_FLAG(gb, value);
    set_flag(gb, FLAG_H, 0);

    set_reg8(gb, R8_A, value);

    /* Implementation from
     * 'https://github.com/guigzzz/GoGB/blob/master/backend/cpu_arithmetic.go#L349' */
}

/* This function is responsible for writing 1 byte to a memory address */

void writeAddr(GB* gb, uint16_t addr, uint8_t byte) {
#ifdef DEBUG_MEM_LOGGING
    printf("Writing 0x%02x to address 0x%04x\n", byte, addr);
#endif

    if (addr >= WRAM_N0_4KB && addr <= WRAM_NN_4KB_END) {
        if (addr >= WRAM_NN_4KB) {
            /* Respect banking */
            gb->wram[(gb->selectedWRAMBank * 0x1000) + (addr - WRAM_NN_4KB)] = byte;
            return;
        }

        /* Otherwise use bank 0 */
        gb->wram[addr - WRAM_N0_4KB] = byte;
        return;
    } else if (addr >= RAM_NN_8KB && addr <= RAM_NN_8KB_END) {
        /* External RAM write request */
        mbc_writeExternalRAM(gb, addr - RAM_NN_8KB, byte);
        return;
    } else if (addr >= IO_REG && addr <= IO_REG_END) {
        /* We perform some actions before writing in some
         * cases */
        switch (addr - IO_REG) {
            case R_TIMA : syncTimer(gb); break;
            case R_TMA  : syncTimer(gb); break;
            case R_TAC  : {
                              syncTimer(gb);

                              /* Writing to TAC sometimes glitches TIMA,
                               * algorithm from 'https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html' */
                              uint8_t oldTAC = gb->IO[R_TAC];
                              /* Make sure unused bits are unchanged */
                              uint8_t newTAC = byte | 0xF8;

                              gb->IO[R_TAC] = newTAC;

                              int clocks_array[] = {1024, 16, 64, 256};
                              uint8_t old_clock = clocks_array[oldTAC & 3];
                              uint8_t new_clock = clocks_array[newTAC & 3];

                              uint8_t old_enable = (oldTAC >> 2) & 1;
                              uint8_t new_enable = (newTAC >> 2) & 1;
                              uint16_t sys_clock = gb->clock & 0xFFFF;

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
                                  incrementTIMA(gb);
                              }
                              return;
                          }
            case R_DIV  : {
                              /* Write to DIV resets it */
                              syncTimer(gb);

                              if (gb->IO[R_DIV] == 1 && ((gb->IO[R_TAC] >> 2) & 1)) {
                                  /* If DIV = 1 and timer is enabled, theres a bug in which
                                   * the TIMA increases */
                                  gb->IO[R_DIV] = 0;
                                  incrementTIMA(gb);
                                  return;
                              }
                              gb->IO[R_DIV] = 0;
                              return;
                          }
            case R_SC:
#ifdef DEBUG_PRINT_SERIAL_OUTPUT
                          if (byte == 0x81) {
                              printf("%c", gb->IO[R_SB]);
                              gb->IO[R_SC] = 0x00;
                          }
#endif
                          break;
            case R_P1_JOYP:
                          /* Set the upper 2 bits because they're unused */
                          SET_BIT(byte, 6);
                          SET_BIT(byte, 7);

                          /* Make sure the lower nibble if unaffected */
                          byte &= 0xF0;
                          byte |= gb->IO[R_P1_JOYP] & 0xF;

                          /* Check bit 4-5to get selected mode */
                          uint8_t selected = ((byte >> 4) & 0b11);
                          gb->joypadSelectedMode = selected;

                          /* Write the selected mode */
                          gb->IO[R_P1_JOYP] = byte;

                          /* Update register to show the keys for the new mode */
                          updateJoypadRegBuffer(gb, selected);
                          return;
            case R_SVBK: {
                             if (gb->emuMode != EMU_CGB) return;
                             /* In CGB Mode, switch WRAM banks */
                             uint8_t bankNumber = byte & 0b00000111;

                             if (bankNumber == 0) bankNumber = 1;
                             gb->selectedWRAMBank = bankNumber;
                             /* Ignore bits 7-3 */
                             gb->IO[R_SVBK] |= bankNumber;
                             return;
                         }
            case R_VBK: {
                            if (gb->emuMode != EMU_CGB) return;
                            /* In CGB Mode, switch VRAM banks
                             *
                             * Only bit 0 matters */
                            uint8_t bankNumber = byte & 1;

                            gb->selectedVRAMBank = bankNumber;
                            /* Ignore all bits other than bit 0 */
                            gb->IO[R_VBK] |= bankNumber;
                            return;
                        }
            case R_BCPD: {
                             if (gb->emuMode != EMU_CGB) return;
                             if (gb->lockPalettes) {
                                 if (GET_BIT(gb->IO[R_BCPS], 7)) {
                                     /* If auto increment is enabled, writes to
                                      * BCPD increment the color ram index address
                                      *
                                      * Note : This happens even if palettes are inaccessible */
                                     gb->currentBackgroundCRAMIndex++;

                                     /* Wrap back to 0 if it exceeds 64 bytes */
                                     if (gb->currentBackgroundCRAMIndex >= 0x40) {
                                         gb->currentBackgroundCRAMIndex = 0;
                                     }
                                 }
                                 return;
                             }

                             gb->bgColorRAM[gb->currentBackgroundCRAMIndex] = byte;
                             if (GET_BIT(gb->IO[R_BCPS], 7)) {
                                 gb->currentBackgroundCRAMIndex++;

                                 if (gb->currentBackgroundCRAMIndex >= 0x40) {
                                     gb->currentBackgroundCRAMIndex = 0;
                                 }
                             }
                             break;
                         }
            case R_BCPS: {
                             if (gb->emuMode != EMU_CGB) return;
                             if (gb->lockPalettes) return;

                             /* Used to index color ram on CGB */
                             SET_BIT(byte, 6);           // bit 6 unused
                             gb->currentBackgroundCRAMIndex = byte & 0b00111111;
                             break;
                         }
            case R_OCPD: {
                             if (gb->emuMode != EMU_CGB) return;
                             if (gb->lockPalettes) {
                                 if (GET_BIT(gb->IO[R_OCPS], 7)) {
                                     /* If auto increment is enabled, writes to
                                      * OCPD increment the color ram index address
                                      *
                                      * Note : This happens even if palettes are inaccessible */
                                     gb->currentSpriteCRAMIndex++;

                                     /* Wrap back to 0 if it exceeds 64 bytes */
                                     if (gb->currentSpriteCRAMIndex >= 0x40) {
                                         gb->currentSpriteCRAMIndex = 0;
                                     }
                                 }
                                 return;
                             }

                             gb->spriteColorRAM[gb->currentSpriteCRAMIndex] = byte;
                             if (GET_BIT(gb->IO[R_OCPS], 7)) {
                                 gb->currentSpriteCRAMIndex++;

                                 if (gb->currentSpriteCRAMIndex >= 0x40) {
                                     gb->currentSpriteCRAMIndex = 0;
                                 }
                             }
							 
                             break;
                         }
            case R_OCPS: {
                             if (gb->emuMode != EMU_CGB) return;
                             if (gb->lockPalettes) return;

                             /* Used to index color ram on CGB */
                             SET_BIT(byte, 6);           // bit 6 unused
                             gb->currentSpriteCRAMIndex = byte & 0b00111111;
                             break;
                         }
            case R_BGP: {
                            break;
                        }
            case R_OBP0:
            case R_OBP1:
						/* NOTE: BGP, OBP0 and OBP1 are still R/W in CGB mode as some games seem to use it*/
                        // if (gb->emuMode != EMU_DMG) return;
                        break;
            case R_STAT:
                        /* Bit 7 in STAT is unused so it has to always be 1.
                         * Bit 2-0 are read only, and are left unchanged */
                        SET_BIT(byte, 7);

                        /* Clear last 3 bits */
                        byte &= ~0x7;
                        /* Set the last 3 bits of STAT to byte */
                        byte |= gb->IO[R_STAT] & 0x7;

                        gb->IO[R_STAT] = byte;
                        return;
            case R_LCDC: {
                             /* Bit 7 = LCD/PPU enable */
                             uint8_t lcdcBit7 = GET_BIT(gb->IO[R_LCDC], 7);
                             uint8_t byteBit7 = GET_BIT(byte, 7);

                             if (lcdcBit7 != byteBit7) {
                                 if (byteBit7) {
                                     /* Enable PPU */
                                     enablePPU(gb);
                                 } else {
                                     /* Disable PPU */
                                     disablePPU(gb);
                                 }
                             }
                             break;
                         }
            case R_DMA: scheduleDMATransfer(gb, byte); break;
            case R_LY: return;
			case R_KEY1: {
				if (gb->emuMode != EMU_CGB) return;
				byte = (gb->IO[R_KEY1] & ~1) | (byte & 1);
				break;
			}
			case R_HDMA5: {
				if (gb->emuMode != EMU_CGB) return;

				if (gb->doingHDMA && (byte >> 7) == 0) {
					/* Cancel of ongoing HDMA transfer requested */
					cancelHDMATransfer(gb);
					return;
				}
				uint8_t type = byte >> 7;
				uint8_t length = (byte & 0x7F) + 1; 		// In blocks of 0x10 bytes
				uint16_t source = (gb->IO[R_HDMA1] << 8) | (gb->IO[R_HDMA2] & ~0xF);
				uint16_t dest = (gb->IO[R_HDMA3] << 8) | gb->IO[R_HDMA4];
				dest = (dest & 0x1FF0) | (1 << 15);

				if (type == 0) {
					/* General Purpose DMA (instant) */
					scheduleGDMATransfer(gb, source, dest, length);
				} else {
					/* HBlank DMA (concurrent) */
					scheduleHDMATransfer(gb, source, dest, length);
				}
				break;
			}
        }

        gb->IO[addr - IO_REG] = byte;
        return;
    } else if (addr >= VRAM_N0_8KB && addr <= VRAM_N0_8KB_END) {
        /* Handle the case when VRAM has been locked by PPU */
        if (gb->lockVRAM) {
            return;
        }

        gb->vram[(gb->selectedVRAMBank * 0x2000) + (addr - VRAM_N0_8KB)] = byte;
        return;
    } else if (addr >= ROM_N0_16KB && addr <= ROM_NN_16KB_END) {
        /* Pass over control to an MBC, maybe this is a call for
         * bank switch */
        mbc_interceptROMWrite(gb, addr, byte);
        return;
    } else if (addr >= HRAM_N0 && addr <= HRAM_N0_END) {
        gb->hram[addr - HRAM_N0] = byte;
        return;
    } else if (addr == R_IE) {
        gb->IE = byte;
        return;
    } else if (addr >= OAM_N0_160B && addr <= OAM_N0_160B_END) {
        /* Handle the case when OAM has been locked by PPU */
        if (gb->lockOAM || gb->doingDMA) return;

        gb->OAM[addr - OAM_N0_160B] = byte;
        return;
    } else if (addr >= ECHO_N0_8KB && addr <= ECHO_N0_8KB_END) {
        gb->wram[addr - ECHO_N0_8KB] = byte;
        return;
    } else if (addr >= UNUSABLE_N0 && addr <= UNUSABLE_N0_END) {
#ifdef DEBUG_LOGGING
        // printf("[WARNING] Attempt to write to address 0x%x (read only)\n", addr);
#endif
        return;
    } 
}

uint8_t readAddr(GB* gb, uint16_t addr) {
    if (addr >= ROM_N0_16KB && addr <= ROM_N0_16KB_END) {
        if (gb->memControllerType == MBC_NONE) {
            return gb->cartridge->allocated[addr];
        }

        return mbc_readROM_N0(gb, addr);
    } else if (addr >= ROM_NN_16KB && addr <= ROM_NN_16KB_END) {
        if (gb->memControllerType == MBC_NONE) {
            return gb->cartridge->allocated[addr];
        }

        return mbc_readROM_NN(gb, addr - ROM_NN_16KB);
    } else if (addr >= WRAM_N0_4KB && addr <= WRAM_NN_4KB_END) {
        if (addr >= WRAM_NN_4KB) {
            /* Respect banking */
            return gb->wram[(gb->selectedWRAMBank * 0x1000) + (addr - WRAM_NN_4KB)];
        }

        /* Otherwise use bank 0 */
        return gb->wram[addr - WRAM_N0_4KB];
    } else if (addr >= RAM_NN_8KB && addr <= RAM_NN_8KB_END) {
        /* Read from external RAM */
        return mbc_readExternalRAM(gb, addr - RAM_NN_8KB);
    }  else if (addr >= IO_REG && addr <= IO_REG_END) {
        /* If we have IO registers to read from, we perform some
         * actions before the read is done in some cases
         *
         * or modify the read values accordingly*/
        switch (addr - IO_REG) {
            case R_DIV  :
            case R_TIMA :
            case R_TMA  :
            case R_TAC  : syncTimer(gb); break;
            case R_BCPD: {
                             if (gb->lockPalettes) return 0xFF;
                             return gb->bgColorRAM[gb->currentBackgroundCRAMIndex];
                         }
            case R_OCPD: {
                             if (gb->lockPalettes) return 0xFF;
                             return gb->spriteColorRAM[gb->currentSpriteCRAMIndex];
                         }
            case R_BCPS:
            case R_OCPS:
                         /* Handle the case when Palettes have been locked by the PPU */
                         if (gb->lockPalettes) return 0xFF;
                         break;
        }

        return gb->IO[addr - IO_REG];
    } else if (addr == R_IE) {
        return gb->IE;
    } else if (addr >= HRAM_N0 && addr <= HRAM_N0_END) {
        return gb->hram[addr - HRAM_N0];
    } else if (addr >= VRAM_N0_8KB && addr <= VRAM_N0_8KB_END) {
        /* Handle the case when VRAM has been locked by the PPU */
        if (gb->lockVRAM) return 0xFF;

        return gb->vram[(gb->selectedVRAMBank * 0x2000) + (addr - VRAM_N0_8KB)];
    } else if (addr >= OAM_N0_160B && addr <= OAM_N0_160B_END) {
        /* Handle the case when OAM has been locked by the PPU */
        if (gb->lockOAM || gb->doingDMA) return 0xFF;

        return gb->OAM[addr - OAM_N0_160B];
    } else if (addr >= UNUSABLE_N0 && addr <= UNUSABLE_N0_END) {
        return 0xFF;
    } else if (addr >= ECHO_N0_8KB && addr <= ECHO_N0_8KB_END) {
        return gb->wram[addr - ECHO_N0_8KB];
    } 

    return 0xFF;
}


static void writeAddr_4C(GB* gb, uint16_t addr, uint8_t byte) {
    writeAddr(gb, addr, byte);
    cyclesSync_4(gb);
}

static uint8_t readAddr_4C(GB* gb, uint16_t addr) {
    uint8_t byte = readAddr(gb, addr);
    cyclesSync_4(gb);

    return byte;
}

/* Interrupt handling and helper functions */

void requestInterrupt(GB* gb, INTERRUPT interrupt) {
    /* This is called by external hardware to request interrupts
     *
     * We set the corresponding bit */
    gb->IO[R_IF] |= 1 << interrupt;
}

static void dispatchInterrupt(GB* gb, INTERRUPT interrupt) {
    /* Disable all interrupts */
    INTERRUPT_MASTER_DISABLE(gb);

    /* Set the bit of this interrupt in the IF register to 0 */
    gb->IO[R_IF] &= ~(1 << interrupt);

    /* Now we pass control to the interrupt handler
     *
     * CPU does nothing for 8 cycles (or executes NOPs) */
    cyclesSync_4(gb);
    cyclesSync_4(gb);

    /* Jump to interrupt vector */
    switch (interrupt) {
        case INTERRUPT_VBLANK:      call(gb, 0x40); break;
        case INTERRUPT_LCD_STAT:    call(gb, 0x48); break;
        case INTERRUPT_TIMER:       call(gb, 0x50); break;
        case INTERRUPT_SERIAL:      call(gb, 0x58); break;
        case INTERRUPT_JOYPAD:      call(gb, 0x60); break;

        default: break;
    }
}

static void handleInterrupts(GB* gb) {
    /* Main interrupt handler for the CPU */

    /* Checks for an interrupt that can be handled and returns
     * it if found, otherwise returns -1 */

    /* We read interrupt flags, which tell us which interrupts are requested
     * if any */
    uint8_t requestedInterrupts = gb->IO[R_IF];
    uint8_t enabledInterrups = gb->IE;

    if (gb->IME) {
        if ((enabledInterrups & requestedInterrupts & 0x1F) != 0) {
            /* Exit halt mode */
            gb->haltMode = false;
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
                    dispatchInterrupt(gb, i);
                    return;
                }
            }
        }
    } else {
        /* Even though the interrupt cannot be handled because IME is false,
         * we still need to exit halt mode */
        if ((enabledInterrups & requestedInterrupts & 0x1F) != 0) {
            gb->haltMode = false;
        }
    }
}

static void halt(GB* gb) {
    /* HALT Instruction procedure */
    uint8_t IE = gb->IE;
    uint8_t IF = gb->IO[R_IF];

    if (gb->IME) {
        if ((IE & IF & 0x1F) == 0) {
            /* IME Enabled, No enabled interrupts requested */
            gb->haltMode = true;
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
            gb->haltMode = true;
        } else {
            /* IME Disabled, 1 or more enabled interrupts requested
             *
             * Halt Bug Occurs */
            gb->scheduleHaltBug = true;
        }
    }
}

static void stop(GB* gb) {
	readByte_4C(gb); 				// Discard (stop is 2 byte long)
	/* STOP Instruction and speed switch mechanism 
	 * Not implemented for DMG, and for CGB we only implement speed switching */
	/* Reset DIV */
	gb->IO[R_DIV] = 0;

	if (gb->emuMode != EMU_CGB) return;
	gb->doingSpeedSwitch = true;

	/* CPU Idles for 2050 M-Cycles, TIMA keeps ticking, DIV doesnt tick,
	 * Interrupts are not handled as CPU is stopped */
#ifdef DEBUG_LOGGING
	printf("Speed Switch Invoked Via STOP\n");
#endif
	for (int i = 0; i < 2050; i++) {
		cyclesSync_4(gb);
		syncTimer(gb);
	}

	gb->doingSpeedSwitch = false;
	gb->isDoubleSpeedMode = !gb->isDoubleSpeedMode;
	gb->IO[R_KEY1] &= 0x7E;
	gb->IO[R_KEY1] |= (int)gb->isDoubleSpeedMode << 7;
	/* CPU has switched speed */
#ifdef DEBUG_LOGGING
	printf("Speed Switch Finished, Now: %s\n", gb->isDoubleSpeedMode?"Double":"Single");
#endif
}

/* Main CPU instruction dispatchers */

static void prefixCB(GB* gb) {
    /* This function contains opcode interpretations for
     * all the instruction prefixed by opcode CB */
    uint8_t byte = readByte_4C(gb);
	
#ifdef DEBUG_PRINT_REGISTERS
    printRegisters(gb);
#endif
#ifdef DEBUG_REALTIME_PRINTING
    printCBInstruction(gb, byte);
#endif

    switch (byte) {
        case 0x00: rotateLeftR8(gb, R8_B, true); break;
        case 0x01: rotateLeftR8(gb, R8_C, true); break;
        case 0x02: rotateLeftR8(gb, R8_D, true); break;
        case 0x03: rotateLeftR8(gb, R8_E, true); break;
        case 0x04: rotateLeftR8(gb, R8_H, true); break;
        case 0x05: rotateLeftR8(gb, R8_L, true); break;
        case 0x06: rotateLeftAR16(gb, R16_HL, true); break;
        case 0x07: rotateLeftR8(gb, R8_A, true); break;
        case 0x08: rotateRightR8(gb, R8_B, true); break;
        case 0x09: rotateRightR8(gb, R8_C, true); break;
        case 0x0A: rotateRightR8(gb, R8_D, true); break;
        case 0x0B: rotateRightR8(gb, R8_E, true); break;
        case 0x0C: rotateRightR8(gb, R8_H, true); break;
        case 0x0D: rotateRightR8(gb, R8_L, true); break;
        case 0x0E: rotateRightAR16(gb, R16_HL, true); break;
        case 0x0F: rotateRightR8(gb, R8_A, true); break;
        case 0x10: rotateLeftCarryR8(gb, R8_B, true); break;
        case 0x11: rotateLeftCarryR8(gb, R8_C, true); break;
        case 0x12: rotateLeftCarryR8(gb, R8_D, true); break;
        case 0x13: rotateLeftCarryR8(gb, R8_E, true); break;
        case 0x14: rotateLeftCarryR8(gb, R8_H, true); break;
        case 0x15: rotateLeftCarryR8(gb, R8_L, true); break;
        case 0x16: rotateLeftCarryAR16(gb, R16_HL, true); break;
        case 0x17: rotateLeftCarryR8(gb, R8_A, true); break;
        case 0x18: rotateRightCarryR8(gb, R8_B, true); break;
        case 0x19: rotateRightCarryR8(gb, R8_C, true); break;
        case 0x1A: rotateRightCarryR8(gb, R8_D, true); break;
        case 0x1B: rotateRightCarryR8(gb, R8_E, true); break;
        case 0x1C: rotateRightCarryR8(gb, R8_H, true); break;
        case 0x1D: rotateRightCarryR8(gb, R8_L, true); break;
        case 0x1E: rotateRightCarryAR16(gb, R16_HL, true); break;
        case 0x1F: rotateRightCarryR8(gb, R8_A, true); break;
        case 0x20: shiftLeftArithmeticR8(gb, R8_B); break;
        case 0x21: shiftLeftArithmeticR8(gb, R8_C); break;
        case 0x22: shiftLeftArithmeticR8(gb, R8_D); break;
        case 0x23: shiftLeftArithmeticR8(gb, R8_E); break;
        case 0x24: shiftLeftArithmeticR8(gb, R8_H); break;
        case 0x25: shiftLeftArithmeticR8(gb, R8_L); break;
        case 0x26: shiftLeftArithmeticAR16(gb, R16_HL); break;
        case 0x27: shiftLeftArithmeticR8(gb, R8_A); break;
        case 0x28: shiftRightArithmeticR8(gb, R8_B); break;
        case 0x29: shiftRightArithmeticR8(gb, R8_C); break;
        case 0x2A: shiftRightArithmeticR8(gb, R8_D); break;
        case 0x2B: shiftRightArithmeticR8(gb, R8_E); break;
        case 0x2C: shiftRightArithmeticR8(gb, R8_H); break;
        case 0x2D: shiftRightArithmeticR8(gb, R8_L); break;
        case 0x2E: shiftRightArithmeticAR16(gb, R16_HL); break;
        case 0x2F: shiftRightArithmeticR8(gb, R8_A); break;
        case 0x30: swapR8(gb, R8_B); break;
        case 0x31: swapR8(gb, R8_C); break;
        case 0x32: swapR8(gb, R8_D); break;
        case 0x33: swapR8(gb, R8_E); break;
        case 0x34: swapR8(gb, R8_H); break;
        case 0x35: swapR8(gb, R8_L); break;
        case 0x36: swapAR16(gb, R16_HL); break;
        case 0x37: swapR8(gb, R8_A); break;
        case 0x38: shiftRightLogicalR8(gb, R8_B); break;
        case 0x39: shiftRightLogicalR8(gb, R8_C); break;
        case 0x3A: shiftRightLogicalR8(gb, R8_D); break;
        case 0x3B: shiftRightLogicalR8(gb, R8_E); break;
        case 0x3C: shiftRightLogicalR8(gb, R8_H); break;
        case 0x3D: shiftRightLogicalR8(gb, R8_L); break;
        case 0x3E: shiftRightLogicalAR16(gb, R16_HL); break;
        case 0x3F: shiftRightLogicalR8(gb, R8_A); break;
        case 0x40: testBitR8(gb, R8_B, 0); break;
        case 0x41: testBitR8(gb, R8_C, 0); break;
        case 0x42: testBitR8(gb, R8_D, 0); break;
        case 0x43: testBitR8(gb, R8_E, 0); break;
        case 0x44: testBitR8(gb, R8_H, 0); break;
        case 0x45: testBitR8(gb, R8_L, 0); break;
        case 0x46: testBitAR16(gb, R16_HL, 0); break;
        case 0x47: testBitR8(gb, R8_A, 0); break;
        case 0x48: testBitR8(gb, R8_B, 1); break;
        case 0x49: testBitR8(gb, R8_C, 1); break;
        case 0x4A: testBitR8(gb, R8_D, 1); break;
        case 0x4B: testBitR8(gb, R8_E, 1); break;
        case 0x4C: testBitR8(gb, R8_H, 1); break;
        case 0x4D: testBitR8(gb, R8_L, 1); break;
        case 0x4E: testBitAR16(gb, R16_HL, 1); break;
        case 0x4F: testBitR8(gb, R8_A, 1); break;
        case 0x50: testBitR8(gb, R8_B, 2); break;
        case 0x51: testBitR8(gb, R8_C, 2); break;
        case 0x52: testBitR8(gb, R8_D, 2); break;
        case 0x53: testBitR8(gb, R8_E, 2); break;
        case 0x54: testBitR8(gb, R8_H, 2); break;
        case 0x55: testBitR8(gb, R8_L, 2); break;
        case 0x56: testBitAR16(gb, R16_HL, 2); break;
        case 0x57: testBitR8(gb, R8_A, 2); break;
        case 0x58: testBitR8(gb, R8_B, 3); break;
        case 0x59: testBitR8(gb, R8_C, 3); break;
        case 0x5A: testBitR8(gb, R8_D, 3); break;
        case 0x5B: testBitR8(gb, R8_E, 3); break;
        case 0x5C: testBitR8(gb, R8_H, 3); break;
        case 0x5D: testBitR8(gb, R8_L, 3); break;
        case 0x5E: testBitAR16(gb, R16_HL, 3); break;
        case 0x5F: testBitR8(gb, R8_A, 3); break;
        case 0x60: testBitR8(gb, R8_B, 4); break;
        case 0x61: testBitR8(gb, R8_C, 4); break;
        case 0x62: testBitR8(gb, R8_D, 4); break;
        case 0x63: testBitR8(gb, R8_E, 4); break;
        case 0x64: testBitR8(gb, R8_H, 4); break;
        case 0x65: testBitR8(gb, R8_L, 4); break;
        case 0x66: testBitAR16(gb, R16_HL, 4); break;
        case 0x67: testBitR8(gb, R8_A, 4); break;
        case 0x68: testBitR8(gb, R8_B, 5); break;
        case 0x69: testBitR8(gb, R8_C, 5); break;
        case 0x6A: testBitR8(gb, R8_D, 5); break;
        case 0x6B: testBitR8(gb, R8_E, 5); break;
        case 0x6C: testBitR8(gb, R8_H, 5); break;
        case 0x6D: testBitR8(gb, R8_L, 5); break;
        case 0x6E: testBitAR16(gb, R16_HL, 5); break;
        case 0x6F: testBitR8(gb, R8_A, 5); break;
        case 0x70: testBitR8(gb, R8_B, 6); break;
        case 0x71: testBitR8(gb, R8_C, 6); break;
        case 0x72: testBitR8(gb, R8_D, 6); break;
        case 0x73: testBitR8(gb, R8_E, 6); break;
        case 0x74: testBitR8(gb, R8_H, 6); break;
        case 0x75: testBitR8(gb, R8_L, 6); break;
        case 0x76: testBitAR16(gb, R16_HL, 6); break;
        case 0x77: testBitR8(gb, R8_A, 6); break;
        case 0x78: testBitR8(gb, R8_B, 7); break;
        case 0x79: testBitR8(gb, R8_C, 7); break;
        case 0x7A: testBitR8(gb, R8_D, 7); break;
        case 0x7B: testBitR8(gb, R8_E, 7); break;
        case 0x7C: testBitR8(gb, R8_H, 7); break;
        case 0x7D: testBitR8(gb, R8_L, 7); break;
        case 0x7E: testBitAR16(gb, R16_HL, 7); break;
        case 0x7F: testBitR8(gb, R8_A, 7); break;
        case 0x80: resetBitR8(gb, R8_B, 0); break;
        case 0x81: resetBitR8(gb, R8_C, 0); break;
        case 0x82: resetBitR8(gb, R8_D, 0); break;
        case 0x83: resetBitR8(gb, R8_E, 0); break;
        case 0x84: resetBitR8(gb, R8_H, 0); break;
        case 0x85: resetBitR8(gb, R8_L, 0); break;
        case 0x86: resetBitAR16(gb, R16_HL, 0); break;
        case 0x87: resetBitR8(gb, R8_A, 0); break;
        case 0x88: resetBitR8(gb, R8_B, 1); break;
        case 0x89: resetBitR8(gb, R8_C, 1); break;
        case 0x8A: resetBitR8(gb, R8_D, 1); break;
        case 0x8B: resetBitR8(gb, R8_E, 1); break;
        case 0x8C: resetBitR8(gb, R8_H, 1); break;
        case 0x8D: resetBitR8(gb, R8_L, 1); break;
        case 0x8E: resetBitAR16(gb, R16_HL, 1); break;
        case 0x8F: resetBitR8(gb, R8_A, 1); break;
        case 0x90: resetBitR8(gb, R8_B, 2); break;
        case 0x91: resetBitR8(gb, R8_C, 2); break;
        case 0x92: resetBitR8(gb, R8_D, 2); break;
        case 0x93: resetBitR8(gb, R8_E, 2); break;
        case 0x94: resetBitR8(gb, R8_H, 2); break;
        case 0x95: resetBitR8(gb, R8_L, 2); break;
        case 0x96: resetBitAR16(gb, R16_HL, 2); break;
        case 0x97: resetBitR8(gb, R8_A, 2); break;
        case 0x98: resetBitR8(gb, R8_B, 3); break;
        case 0x99: resetBitR8(gb, R8_C, 3); break;
        case 0x9A: resetBitR8(gb, R8_D, 3); break;
        case 0x9B: resetBitR8(gb, R8_E, 3); break;
        case 0x9C: resetBitR8(gb, R8_H, 3); break;
        case 0x9D: resetBitR8(gb, R8_L, 3); break;
        case 0x9E: resetBitAR16(gb, R16_HL, 3); break;
        case 0x9F: resetBitR8(gb, R8_A, 3); break;
        case 0xA0: resetBitR8(gb, R8_B, 4); break;
        case 0xA1: resetBitR8(gb, R8_C, 4); break;
        case 0xA2: resetBitR8(gb, R8_D, 4); break;
        case 0xA3: resetBitR8(gb, R8_E, 4); break;
        case 0xA4: resetBitR8(gb, R8_H, 4); break;
        case 0xA5: resetBitR8(gb, R8_L, 4); break;
        case 0xA6: resetBitAR16(gb, R16_HL, 4); break;
        case 0xA7: resetBitR8(gb, R8_A, 4); break;
        case 0xA8: resetBitR8(gb, R8_B, 5); break;
        case 0xA9: resetBitR8(gb, R8_C, 5); break;
        case 0xAA: resetBitR8(gb, R8_D, 5); break;
        case 0xAB: resetBitR8(gb, R8_E, 5); break;
        case 0xAC: resetBitR8(gb, R8_H, 5); break;
        case 0xAD: resetBitR8(gb, R8_L, 5); break;
        case 0xAE: resetBitAR16(gb, R16_HL, 5); break;
        case 0xAF: resetBitR8(gb, R8_A, 5); break;
        case 0xB0: resetBitR8(gb, R8_B, 6); break;
        case 0xB1: resetBitR8(gb, R8_C, 6); break;
        case 0xB2: resetBitR8(gb, R8_D, 6); break;
        case 0xB3: resetBitR8(gb, R8_E, 6); break;
        case 0xB4: resetBitR8(gb, R8_H, 6); break;
        case 0xB5: resetBitR8(gb, R8_L, 6); break;
        case 0xB6: resetBitAR16(gb, R16_HL, 6); break;
        case 0xB7: resetBitR8(gb, R8_A, 6); break;
        case 0xB8: resetBitR8(gb, R8_B, 7); break;
        case 0xB9: resetBitR8(gb, R8_C, 7); break;
        case 0xBA: resetBitR8(gb, R8_D, 7); break;
        case 0xBB: resetBitR8(gb, R8_E, 7); break;
        case 0xBC: resetBitR8(gb, R8_H, 7); break;
        case 0xBD: resetBitR8(gb, R8_L, 7); break;
        case 0xBE: resetBitAR16(gb, R16_HL, 7); break;
        case 0xBF: resetBitR8(gb, R8_A, 7); break;
        case 0xC0: setBitR8(gb, R8_B, 0); break;
        case 0xC1: setBitR8(gb, R8_C, 0); break;
        case 0xC2: setBitR8(gb, R8_D, 0); break;
        case 0xC3: setBitR8(gb, R8_E, 0); break;
        case 0xC4: setBitR8(gb, R8_H, 0); break;
        case 0xC5: setBitR8(gb, R8_L, 0); break;
        case 0xC6: setBitAR16(gb, R16_HL, 0); break;
        case 0xC7: setBitR8(gb, R8_A, 0); break;
        case 0xC8: setBitR8(gb, R8_B, 1); break;
        case 0xC9: setBitR8(gb, R8_C, 1); break;
        case 0xCA: setBitR8(gb, R8_D, 1); break;
        case 0xCB: setBitR8(gb, R8_E, 1); break;
        case 0xCC: setBitR8(gb, R8_H, 1); break;
        case 0xCD: setBitR8(gb, R8_L, 1); break;
        case 0xCE: setBitAR16(gb, R16_HL, 1); break;
        case 0xCF: setBitR8(gb, R8_A, 1); break;
        case 0xD0: setBitR8(gb, R8_B, 2); break;
        case 0xD1: setBitR8(gb, R8_C, 2); break;
        case 0xD2: setBitR8(gb, R8_D, 2); break;
        case 0xD3: setBitR8(gb, R8_E, 2); break;
        case 0xD4: setBitR8(gb, R8_H, 2); break;
        case 0xD5: setBitR8(gb, R8_L, 2); break;
        case 0xD6: setBitAR16(gb, R16_HL, 2); break;
        case 0xD7: setBitR8(gb, R8_A, 2); break;
        case 0xD8: setBitR8(gb, R8_B, 3); break;
        case 0xD9: setBitR8(gb, R8_C, 3); break;
        case 0xDA: setBitR8(gb, R8_D, 3); break;
        case 0xDB: setBitR8(gb, R8_E, 3); break;
        case 0xDC: setBitR8(gb, R8_H, 3); break;
        case 0xDD: setBitR8(gb, R8_L, 3); break;
        case 0xDE: setBitAR16(gb, R16_HL, 3); break;
        case 0xDF: setBitR8(gb, R8_A, 3); break;
        case 0xE0: setBitR8(gb, R8_B, 4); break;
        case 0xE1: setBitR8(gb, R8_C, 4); break;
        case 0xE2: setBitR8(gb, R8_D, 4); break;
        case 0xE3: setBitR8(gb, R8_E, 4); break;
        case 0xE4: setBitR8(gb, R8_H, 4); break;
        case 0xE5: setBitR8(gb, R8_L, 4); break;
        case 0xE6: setBitAR16(gb, R16_HL, 4); break;
        case 0xE7: setBitR8(gb, R8_A, 4); break;
        case 0xE8: setBitR8(gb, R8_B, 5); break;
        case 0xE9: setBitR8(gb, R8_C, 5); break;
        case 0xEA: setBitR8(gb, R8_D, 5); break;
        case 0xEB: setBitR8(gb, R8_E, 5); break;
        case 0xEC: setBitR8(gb, R8_H, 5); break;
        case 0xED: setBitR8(gb, R8_L, 5); break;
        case 0xEE: setBitAR16(gb, R16_HL, 5); break;
        case 0xEF: setBitR8(gb, R8_A, 5); break;
        case 0xF0: setBitR8(gb, R8_B, 6); break;
        case 0xF1: setBitR8(gb, R8_C, 6); break;
        case 0xF2: setBitR8(gb, R8_D, 6); break;
        case 0xF3: setBitR8(gb, R8_E, 6); break;
        case 0xF4: setBitR8(gb, R8_H, 6); break;
        case 0xF5: setBitR8(gb, R8_L, 6); break;
        case 0xF6: setBitAR16(gb, R16_HL, 6); break;
        case 0xF7: setBitR8(gb, R8_A, 6); break;
        case 0xF8: setBitR8(gb, R8_B, 7); break;
        case 0xF9: setBitR8(gb, R8_C, 7); break;
        case 0xFA: setBitR8(gb, R8_D, 7); break;
        case 0xFB: setBitR8(gb, R8_E, 7); break;
        case 0xFC: setBitR8(gb, R8_H, 7); break;
        case 0xFD: setBitR8(gb, R8_L, 7); break;
        case 0xFE: setBitAR16(gb, R16_HL, 7); break;
        case 0xFF: setBitR8(gb, R8_A, 7); break;
    }
}

/* Instruction Set : https://www.pastraiser.com/cpu/gameboy/gameboy_opcodes.html */

void dispatch(GB* gb) {	
#ifdef DEBUG_PRINT_REGISTERS
    printRegisters(gb);
#endif
#ifdef DEBUG_REALTIME_PRINTING
    printInstruction(gb);
#endif

    uint8_t byte = 0; /* Will get set later */

    /* Enable interrupts if it was scheduled */
    if (gb->scheduleInterruptEnable) {
        gb->scheduleInterruptEnable = false;
        gb->IME = true;
    }

    if (gb->haltMode) {
        /* Skip dispatch and directly check for pending interrupts */

        /* The CPU is in sleep mode while halt mode is true,
         * but the device clock will not be affected and continue
         * ticking
         *
         * Other syncs will also continue taking place */
        cyclesSync_4(gb);
        syncTimer(gb);
        handleInterrupts(gb);
        return;
    } else if (gb->scheduleHaltBug) {
        /* Revert the PC increment */

        byte = readByte(gb);
        gb->PC--;
		gb->dispatchedAddresses[gb->dispatchedAddressesStart++] = gb->PC;
		if (gb->dispatchedAddressesStart > 10) gb->dispatchedAddressesStart = 0;
        cyclesSync_4(gb);
    } else {
        /* Normal Read */
        byte = readByte_4C(gb);

		gb->dispatchedAddresses[gb->dispatchedAddressesStart++] = gb->PC-1;
		if (gb->dispatchedAddressesStart > 10) gb->dispatchedAddressesStart = 0;
    }

    /* Do the dispatch */
    switch (byte) {
        // nop
        case 0x00: break;
        case 0x01: LOAD_RR_D16(gb, R16_BC); break;
        case 0x02: LOAD_ARR_R(gb, R16_BC, R8_A); break;
        case 0x03: INC_RR(gb, R16_BC); break;
        case 0x04: incrementR8(gb, R8_B); break;
        case 0x05: decrementR8(gb, R8_B); break;
        case 0x06: LOAD_R_D8(gb, R8_B); break;
        case 0x07: rotateLeftR8(gb, R8_A, false); break;
        case 0x08: {
                       uint16_t a = read2Bytes_8C(gb);
                       uint16_t sp = get_reg16(gb, R16_SP);
                       /* Write high byte to high and low byte to low */
                       writeAddr_4C(gb, a+1, sp >> 8);
                       writeAddr_4C(gb, a, sp & 0xFF);
                       break;
                   }
        case 0x09: addR16(gb, R16_HL, R16_BC); break;
        case 0x0A: LOAD_R_ARR(gb, R8_A, R16_BC); break;
        case 0x0B: DEC_RR(gb, R16_BC); break;
        case 0x0C: incrementR8(gb, R8_C); break;
        case 0x0D: decrementR8(gb, R8_C); break;
        case 0x0E: LOAD_R_D8(gb, R8_C); break;
        case 0x0F: rotateRightR8(gb, R8_A, false); break;
        case 0x10: stop(gb); return;
        case 0x11: LOAD_RR_D16(gb, R16_DE); break;
        case 0x12: LOAD_ARR_R(gb, R16_DE, R8_A); break;
        case 0x13: INC_RR(gb, R16_DE); break;
        case 0x14: incrementR8(gb, R8_D); break;
        case 0x15: decrementR8(gb, R8_D); break;
        case 0x16: LOAD_R_D8(gb, R8_D); break;
        case 0x17: rotateLeftCarryR8(gb, R8_A, false); break;
        case 0x18: JUMP_RL(gb, readByte_4C(gb)); break;
        case 0x19: addR16(gb, R16_HL, R16_DE); break;
        case 0x1A: LOAD_R_ARR(gb, R8_A, R16_DE); break;
        case 0x1B: DEC_RR(gb, R16_DE); break;
        case 0x1C: incrementR8(gb, R8_E); break;
        case 0x1D: decrementR8(gb, R8_E); break;
        case 0x1E: LOAD_R_D8(gb, R8_E); break;
        case 0x1F: rotateRightCarryR8(gb, R8_A, false); break;
        case 0x20: jumpRelativeCondition(gb, CONDITION_NZ(gb)); break;
        case 0x21: LOAD_RR_D16(gb, R16_HL); break;
        case 0x22: LOAD_ARR_R(gb, R16_HL, R8_A);
                   set_reg16(gb, R16_HL, (get_reg16(gb, R16_HL) + 1));
                   break;
        case 0x23: INC_RR(gb, R16_HL); break;
        case 0x24: incrementR8(gb, R8_H); break;
        case 0x25: decrementR8(gb, R8_H); break;
        case 0x26: LOAD_R_D8(gb, R8_H); break;
        case 0x27: decimalAdjust(gb); break;
        case 0x28: jumpRelativeCondition(gb, CONDITION_Z(gb)); break;
        case 0x29: addR16(gb, R16_HL, R16_HL); break;
        case 0x2A: LOAD_R_ARR(gb, R8_A, R16_HL);
                   set_reg16(gb, R16_HL, (get_reg16(gb, R16_HL) + 1));
                   break;
        case 0x2B: DEC_RR(gb, R16_HL); break;
        case 0x2C: incrementR8(gb, R8_L); break;
        case 0x2D: decrementR8(gb, R8_L); break;
        case 0x2E: LOAD_R_D8(gb, R8_L); break;
        case 0x2F: cpl(gb); break;
        case 0x30: jumpRelativeCondition(gb, CONDITION_NC(gb)); break;
        case 0x31: LOAD_RR_D16(gb, R16_SP); break;
        case 0x32: LOAD_ARR_R(gb, R16_HL, R8_A);
                   set_reg16(gb, R16_HL, (get_reg16(gb, R16_HL) - 1));
                   break;
        case 0x33: INC_RR(gb, R16_SP); break;
        case 0x34: {
                       /* Increment what is at the address in HL */
                       uint16_t address = get_reg16(gb, R16_HL);
                       uint8_t old = readAddr_4C(gb, address);
                       uint8_t new = old + 1;

                       TEST_Z_FLAG(gb, new);
                       TEST_H_FLAG_ADD(gb, old, 1);
                       set_flag(gb, FLAG_N, 0);
                       writeAddr_4C(gb, address, new);
                       break;
                   }
        case 0x35: {
                       /* Decrement what is at the address in HL */
                       uint16_t address = get_reg16(gb, R16_HL);
                       uint8_t old = readAddr_4C(gb, address);
                       uint8_t new = old - 1;

                       TEST_Z_FLAG(gb, new);
                       TEST_H_FLAG_SUB(gb, old, 1);
                       set_flag(gb, FLAG_N, 1);
                       writeAddr_4C(gb, address, new);
                       break;
                   }
        case 0x36: LOAD_ARR_D8(gb, R16_HL); break;
        case 0x37: {
                       set_flag(gb, FLAG_C, 1);
                       set_flag(gb, FLAG_N, 0);
                       set_flag(gb, FLAG_H, 0);
                       break;
                   }
        case 0x38: jumpRelativeCondition(gb, CONDITION_C(gb)); break;
        case 0x39: addR16(gb, R16_HL, R16_SP); break;
        case 0x3A: {
                       LOAD_R_ARR(gb, R8_A, R16_HL);
                       set_reg16(gb, R16_HL, (get_reg16(gb, R16_HL) - 1));
                       break;
                   }
        case 0x3B: DEC_RR(gb, R16_SP); break;
        case 0x3C: incrementR8(gb, R8_A); break;
        case 0x3D: decrementR8(gb, R8_A); break;
        case 0x3E: LOAD_R_D8(gb, R8_A); break;
        case 0x3F: CCF(gb); break;
        case 0x40: LOAD_R_R(gb, R8_B, R8_B);
#ifdef DEBUG_LDBB_BREAKPOINT
                   exit(0);
#endif
                   break;
        case 0x41: LOAD_R_R(gb, R8_B, R8_C); break;
        case 0x42: LOAD_R_R(gb, R8_B, R8_D); break;
        case 0x43: LOAD_R_R(gb, R8_B, R8_E); break;
        case 0x44: LOAD_R_R(gb, R8_B, R8_H); break;
        case 0x45: LOAD_R_R(gb, R8_B, R8_L); break;
        case 0x46: LOAD_R_ARR(gb, R8_B, R16_HL); break;
        case 0x47: LOAD_R_R(gb, R8_B, R8_A); break;
        case 0x48: LOAD_R_R(gb, R8_C, R8_B); break;
        case 0x49: LOAD_R_R(gb, R8_C, R8_C); break;
        case 0x4A: LOAD_R_R(gb, R8_C, R8_D); break;
        case 0x4B: LOAD_R_R(gb, R8_C, R8_E); break;
        case 0x4C: LOAD_R_R(gb, R8_C, R8_H); break;
        case 0x4D: LOAD_R_R(gb, R8_C, R8_L); break;
        case 0x4E: LOAD_R_ARR(gb, R8_C, R16_HL); break;
        case 0x4F: LOAD_R_R(gb, R8_C, R8_A); break;
        case 0x50: LOAD_R_R(gb, R8_D, R8_B); break;
        case 0x51: LOAD_R_R(gb, R8_D, R8_C); break;
        case 0x52: LOAD_R_R(gb, R8_D, R8_D); break;
        case 0x53: LOAD_R_R(gb, R8_D, R8_E); break;
        case 0x54: LOAD_R_R(gb, R8_D, R8_H); break;
        case 0x55: LOAD_R_R(gb, R8_D, R8_L); break;
        case 0x56: LOAD_R_ARR(gb, R8_D, R16_HL); break;
        case 0x57: LOAD_R_R(gb, R8_D, R8_A); break;
        case 0x58: LOAD_R_R(gb, R8_E, R8_B); break;
        case 0x59: LOAD_R_R(gb, R8_E, R8_C); break;
        case 0x5A: LOAD_R_R(gb, R8_E, R8_D); break;
        case 0x5B: LOAD_R_R(gb, R8_E, R8_E); break;
        case 0x5C: LOAD_R_R(gb, R8_E, R8_H); break;
        case 0x5D: LOAD_R_R(gb, R8_E, R8_L); break;
        case 0x5E: LOAD_R_ARR(gb, R8_E, R16_HL); break;
        case 0x5F: LOAD_R_R(gb, R8_E, R8_A); break;
        case 0x60: LOAD_R_R(gb, R8_H, R8_B); break;
        case 0x61: LOAD_R_R(gb, R8_H, R8_C); break;
        case 0x62: LOAD_R_R(gb, R8_H, R8_D); break;
        case 0x63: LOAD_R_R(gb, R8_H, R8_E); break;
        case 0x64: LOAD_R_R(gb, R8_H, R8_H); break;
        case 0x65: LOAD_R_R(gb, R8_H, R8_L); break;
        case 0x66: LOAD_R_ARR(gb, R8_H, R16_HL); break;
        case 0x67: LOAD_R_R(gb, R8_H, R8_A); break;
        case 0x68: LOAD_R_R(gb, R8_L, R8_B); break;
        case 0x69: LOAD_R_R(gb, R8_L, R8_C); break;
        case 0x6A: LOAD_R_R(gb, R8_L, R8_D); break;
        case 0x6B: LOAD_R_R(gb, R8_L, R8_E); break;
        case 0x6C: LOAD_R_R(gb, R8_L, R8_H); break;
        case 0x6D: LOAD_R_R(gb, R8_L, R8_L); break;
        case 0x6E: LOAD_R_ARR(gb, R8_L, R16_HL); break;
        case 0x6F: LOAD_R_R(gb, R8_L, R8_A); break;
        case 0x70: LOAD_ARR_R(gb, R16_HL, R8_B); break;
        case 0x71: LOAD_ARR_R(gb, R16_HL, R8_C); break;
        case 0x72: LOAD_ARR_R(gb, R16_HL, R8_D); break;
        case 0x73: LOAD_ARR_R(gb, R16_HL, R8_E); break;
        case 0x74: LOAD_ARR_R(gb, R16_HL, R8_H); break;
        case 0x75: LOAD_ARR_R(gb, R16_HL, R8_L); break;
        case 0x76: halt(gb); break;
        case 0x77: LOAD_ARR_R(gb, R16_HL, R8_A); break;
        case 0x78: LOAD_R_R(gb, R8_A, R8_B); break;
        case 0x79: LOAD_R_R(gb, R8_A, R8_C); break;
        case 0x7A: LOAD_R_R(gb, R8_A, R8_D); break;
        case 0x7B: LOAD_R_R(gb, R8_A, R8_E); break;
        case 0x7C: LOAD_R_R(gb, R8_A, R8_H); break;
        case 0x7D: LOAD_R_R(gb, R8_A, R8_L); break;
        case 0x7E: LOAD_R_ARR(gb, R8_A, R16_HL); break;
        case 0x7F: LOAD_R_R(gb, R8_A, R8_A); break;
        case 0x80: addR8(gb, R8_A, R8_B); break;
        case 0x81: addR8(gb, R8_A, R8_C); break;
        case 0x82: addR8(gb, R8_A, R8_D); break;
        case 0x83: addR8(gb, R8_A, R8_E); break;
        case 0x84: addR8(gb, R8_A, R8_H); break;
        case 0x85: addR8(gb, R8_A, R8_L); break;
        case 0x86: addR8_AR16(gb, R8_A, R16_HL); break;
        case 0x87: addR8(gb, R8_A, R8_A); break;
        case 0x88: adcR8(gb, R8_A, R8_B); break;
        case 0x89: adcR8(gb, R8_A, R8_C); break;
        case 0x8A: adcR8(gb, R8_A, R8_D); break;
        case 0x8B: adcR8(gb, R8_A, R8_E); break;
        case 0x8C: adcR8(gb, R8_A, R8_H); break;
        case 0x8D: adcR8(gb, R8_A, R8_L); break;
        case 0x8E: adcR8_AR16(gb, R8_A, R16_HL); break;
        case 0x8F: adcR8(gb, R8_A, R8_A); break;
        case 0x90: subR8(gb, R8_A, R8_B); break;
        case 0x91: subR8(gb, R8_A, R8_C); break;
        case 0x92: subR8(gb, R8_A, R8_D); break;
        case 0x93: subR8(gb, R8_A, R8_E); break;
        case 0x94: subR8(gb, R8_A, R8_H); break;
        case 0x95: subR8(gb, R8_A, R8_L); break;
        case 0x96: subR8_AR16(gb, R8_A, R16_HL); break;
        case 0x97: subR8(gb, R8_A, R8_A); break;
        case 0x98: sbcR8(gb, R8_A, R8_B); break;
        case 0x99: sbcR8(gb, R8_A, R8_C); break;
        case 0x9A: sbcR8(gb, R8_A, R8_D); break;
        case 0x9B: sbcR8(gb, R8_A, R8_E); break;
        case 0x9C: sbcR8(gb, R8_A, R8_H); break;
        case 0x9D: sbcR8(gb, R8_A, R8_L); break;
        case 0x9E: sbcR8_AR16(gb, R8_A, R16_HL); break;
        case 0x9F: sbcR8(gb, R8_A, R8_A); break;
        case 0xA0: andR8(gb, R8_A, R8_B); break;
        case 0xA1: andR8(gb, R8_A, R8_C); break;
        case 0xA2: andR8(gb, R8_A, R8_D); break;
        case 0xA3: andR8(gb, R8_A, R8_E); break;
        case 0xA4: andR8(gb, R8_A, R8_H); break;
        case 0xA5: andR8(gb, R8_A, R8_L); break;
        case 0xA6: andR8_AR16(gb, R8_A, R16_HL); break;
        case 0xA7: andR8(gb, R8_A, R8_A); break;
        case 0xA8: xorR8(gb, R8_A, R8_B); break;
        case 0xA9: xorR8(gb, R8_A, R8_C); break;
        case 0xAA: xorR8(gb, R8_A, R8_D); break;
        case 0xAB: xorR8(gb, R8_A, R8_E); break;
        case 0xAC: xorR8(gb, R8_A, R8_H); break;
        case 0xAD: xorR8(gb, R8_A, R8_L); break;
        case 0xAE: xorR8_AR16(gb, R8_A, R16_HL); break;
        case 0xAF: xorR8(gb, R8_A, R8_A); break;
        case 0xB0: orR8(gb, R8_A, R8_B); break;
        case 0xB1: orR8(gb, R8_A, R8_C); break;
        case 0xB2: orR8(gb, R8_A, R8_D); break;
        case 0xB3: orR8(gb, R8_A, R8_E); break;
        case 0xB4: orR8(gb, R8_A, R8_H); break;
        case 0xB5: orR8(gb, R8_A, R8_L); break;
        case 0xB6: orR8_AR16(gb, R8_A, R16_HL); break;
        case 0xB7: orR8(gb, R8_A, R8_A); break;
        case 0xB8: compareR8(gb, R8_A, R8_B); break;
        case 0xB9: compareR8(gb, R8_A, R8_C); break;
        case 0xBA: compareR8(gb, R8_A, R8_D); break;
        case 0xBB: compareR8(gb, R8_A, R8_E); break;
        case 0xBC: compareR8(gb, R8_A, R8_H); break;
        case 0xBD: compareR8(gb, R8_A, R8_L); break;
        case 0xBE: compareR8_AR16(gb, R8_A, R16_HL); break;
        case 0xBF: compareR8(gb, R8_A, R8_A); break;
        case 0xC0: retCondition(gb, CONDITION_NZ(gb)); break;
        case 0xC1: POP_R16(gb, R16_BC); break;
        case 0xC2: jumpCondition(gb, CONDITION_NZ(gb)); break;
        case 0xC3: JUMP(gb, read2Bytes_8C(gb)); break;
        case 0xC4: callCondition(gb, read2Bytes_8C(gb), CONDITION_NZ(gb)); break;
        case 0xC5: PUSH_R16(gb, R16_BC); break;
        case 0xC6: addR8D8(gb, R8_A); break;
        case 0xC7: RST(gb, 0x00); break;
        case 0xC8: retCondition(gb, CONDITION_Z(gb)); break;
        case 0xC9: ret(gb); break;
        case 0xCA: jumpCondition(gb, CONDITION_Z(gb)); break;
        case 0xCB: prefixCB(gb); break;
        case 0xCC: callCondition(gb, read2Bytes_8C(gb), CONDITION_Z(gb)); break;
        case 0xCD: call(gb, read2Bytes_8C(gb)); break;
        case 0xCE: adcR8D8(gb, R8_A); break;
        case 0xCF: RST(gb, 0x08); break;
        case 0xD0: retCondition(gb, CONDITION_NC(gb)); break;
        case 0xD1: POP_R16(gb, R16_DE); break;
        case 0xD2: jumpCondition(gb, CONDITION_NC(gb)); break;
        case 0xD4: callCondition(gb, read2Bytes_8C(gb), CONDITION_NC(gb)); break;
        case 0xD5: PUSH_R16(gb, R16_DE); break;
        case 0xD6: subR8D8(gb, R8_A); break;
        case 0xD7: RST(gb, 0x10); break;
        case 0xD8: retCondition(gb, CONDITION_C(gb)); break;
        case 0xD9: INTERRUPT_MASTER_ENABLE(gb); ret(gb); break;
        case 0xDA: jumpCondition(gb, CONDITION_C(gb)); break;
        case 0xDC: callCondition(gb, read2Bytes_8C(gb), CONDITION_C(gb)); break;
        case 0xDE: sbcR8D8(gb, R8_A); break;
        case 0xDF: RST(gb, 0x18); break;
        case 0xE0: LOAD_D8PORT_R(gb, R8_A); break;
        case 0xE1: POP_R16(gb, R16_HL); break;
        case 0xE2: LOAD_RPORT_R(gb, R8_A, R8_C); break;
        case 0xE5: PUSH_R16(gb, R16_HL); break;
        case 0xE6: andR8D8(gb, R8_A); break;
        case 0xE7: RST(gb, 0x20); break;
        case 0xE8: addR16I8(gb, R16_SP); break;
        case 0xE9: JUMP_RR(gb, R16_HL); break;
        case 0xEA: LOAD_MEM_R(gb, R8_A); break;
        case 0xEE: xorR8D8(gb, R8_A); break;
        case 0xEF: RST(gb, 0x28); break;
        case 0xF0: LOAD_R_D8PORT(gb, R8_A); break;
        case 0xF1: {
                       POP_R16(gb, R16_AF);

                       /* Always clear the lower 4 bits, they need to always
                        * be 0, we failed a blargg test because of this lol */
                       set_reg8(gb, R8_F, get_reg8(gb, R8_F) & 0xF0);
                       break;
                   }
        case 0xF2: LOAD_R_RPORT(gb, R8_A, R8_C); break;
        case 0xF3: INTERRUPT_MASTER_DISABLE(gb); break;
        case 0xF5: PUSH_R16(gb, R16_AF); break;
        case 0xF6: orR8D8(gb, R8_A); break;
        case 0xF7: RST(gb, 0x30); break;
        case 0xF8: LOAD_RR_RRI8(gb, R16_HL, R16_SP); break;
        case 0xF9: LOAD_RR_RR(gb, R16_SP, R16_HL); break;
        case 0xFA: LOAD_R_MEM(gb, R8_A); break;
        case 0xFB: INTERRUPT_MASTER_ENABLE(gb); break;
        case 0xFE: compareR8D8(gb, R8_A); break;
        case 0xFF: RST(gb, 0x38); break;
    }

    /* We sync the timer after every dispatch just before checking for interrupts */
    syncTimer(gb);
    /* We handle any interrupts that are requested */
    handleInterrupts(gb);
}
