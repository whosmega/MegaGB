#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/vm.h"
#include "../include/debug.h"

#define LOAD_RR_D16(vm, RR) set_reg16(vm, RR, READ_16BIT(vm))
#define LOAD_RR_R(vm, RR, R) set_reg16(vm, RR, vm->GPR[R])
#define LOAD_R_D8(vm, R) vm->GPR[R] = READ_BYTE(vm) 
#define LOAD_R_RR(vm, R, RR) vm->GPR[R] = (uint8_t)get_reg16(vm, RR)
#define LOAD_RR_D8(vm, RR) set_reg16(vm, RR, (uint16_t)READ_BYTE(vm))
#define LOAD_R_R(vm, R1, R2) vm->GPR[R1] = vm->GPR[R2]
#define INC_RR(vm, RR) set_reg16(vm, RR, (get_reg16(vm, RR) + 1))
#define DEC_RR(vm, RR) set_reg16(vm, RR, (get_reg16(vm, RR) - 1)) 

/* Direct Jump */
#define JUMP(vm) vm->PC = READ_16BIT(vm)
/* Relative Jump */
#define JUMP_RL(vm) vm->PC += (int8_t)READ_BYTE(vm)

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
#define TEST_C_FLAG_ADD16(vm, x, y) set_flag(vm, FLAG_C, (uint32_t)x + (uint32_t)y > 0xFFFF ? 1 : 0)
#define TEST_C_FLAG_ADD8(vm, x, y) set_flag(vm, FLAG_C, (uint16_t)x + (uint16_t)y > 0xFF ? 1 : 0)
#define TEST_C_FLAG_SUB8(vm, x, y) set_flag(vm, FLAG_C, (int32_t)x - (int32_t)y < 0 ? 1 : 0)

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

static void rotateLeft(VM* vm, GP_REG R) {
    uint8_t toModify = vm->GPR[R];
    uint8_t bit7 = toModify >> 7;

    toModify <<= 1;
    toModify |= bit7;

    set_flag(vm, FLAG_Z, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit7); 
}

static void rotateRight(VM* vm, GP_REG R) {
    uint8_t toModify = vm->GPR[R];
    uint8_t bit1 = toModify & 1;

    toModify >>= 1;
    toModify |= bit1 << 7;

    set_flag(vm, FLAG_Z, 0);
    set_flag(vm, FLAG_H, 0);
    set_flag(vm, FLAG_N, 0);
    set_flag(vm, FLAG_C, bit1); 
}

static void addR16(VM* vm, GP_REG RR1, GP_REG RR2) {
    uint16_t old = get_reg16(vm, RR1); 
    uint16_t toAdd = get_reg16(vm, RR2);
    uint16_t result = set_reg16(vm, RR1, old + toAdd);
    set_flag(vm, FLAG_N, 0);
    TEST_H_FLAG_ADD(vm, old, result);
    TEST_C_FLAG_ADD16(vm, old, toAdd);
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

static void addR8R16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint16_t toAdd = get_reg16(vm, R16);
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

static void adcR8R16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint16_t toAdd = get_reg16(vm, R16);
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
    TEST_C_FLAG_SUB8(vm, old, toSub);
}

static void subR8R16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    uint16_t toSub = get_reg16(vm, R16);
    uint8_t result = old - toSub;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB8(vm, old, toSub);
}

static void sbcR8(VM* vm, GP_REG R1, GP_REG R2) {
    uint8_t old = vm->GPR[R1];
    int32_t toSub = (int32_t)vm->GPR[R2];
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old - toSub;

    vm->GPR[R1] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB8(vm, old, toSub - carry);
}

static void sbcR8R16(VM* vm, GP_REG R8, GP_REG R16) {
    uint8_t old = vm->GPR[R8];
    int32_t toSub = (int32_t)get_reg16(vm, R16);
    uint8_t carry = get_flag(vm, FLAG_C);
    uint8_t result = old - toSub;

    vm->GPR[R8] = result;

    TEST_Z_FLAG(vm, result);
    set_flag(vm, FLAG_N, 1);
    TEST_H_FLAG_SUB(vm, old, result);
    TEST_C_FLAG_SUB8(vm, old, toSub - carry);
}

static void andR8(VM* vm, GP_REG R8, GP_REG R8) {
    uint8_t 
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
        printf("Error : Attempt to write to address 0x%x\n", addr);
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
#ifndef NO_CARTRIDGE_VERIFICATION    
    bool logoVerified = memcmp(&vm->cartridge->logoChecksum, &logo, 0x18) == 0;
    
    if (!logoVerified) {
        printf("FATAL : Logo Verification Failed\n");
        exit(5);
    }
    
    int checksum = 0;
    for (int i = 0x134; i <= 0x14C; i++) {
        checksum = checksum - vm->cartridge->allocated[i] - 1;
    }

    if ((checksum & 0xFF) != vm->cartridge->headerChecksum) {
        printf("FATAL : Header Checksum Doesn't Match, it is possibly corrupted\n");
        exit(5);
    }
#endif   
    /* Map the cartridge rom to the GBC rom space 
     * occupying bank 0 and 1, a total of 32 KB*/
    memcpy(&vm->MEM[ROM_N0_16KB], vm->cartridge->allocated, 0x8000);
}

void runVM(VM* vm) {
    for (;;) {
#ifdef REALTIME_PRINTING
        if (vm->PC < 0x200) printInstruction(vm);
#endif
        uint8_t byte = READ_BYTE(vm);
        switch (byte) {
            // nop
            case 0x00: break;
            case 0x01: LOAD_RR_D16(vm, R16_BC); break;
            case 0x02: LOAD_RR_R(vm, R16_BC, R8_A); break;
            case 0x03: INC_RR(vm, R16_BC); break;
            case 0x04: incrementR8(vm, R8_B); break;
            case 0x05: decrementR8(vm, R8_B); break;
            case 0x06: LOAD_R_D8(vm, R8_B); break;
            case 0x07: rotateLeft(vm, R8_A); break;
            case 0x08: {
                uint16_t a = READ_16BIT(vm);
                writeAddr(vm, a, vm->GPR[R16_SP] & 0xFF);
                writeAddr(vm, a+1, vm->GPR[R16_SP] >> 8);
                break;
            }
            case 0x09: addR16(vm, R16_HL, R16_BC); break;
            case 0x0A: LOAD_R_RR(vm, R8_A, R16_BC); break;
            case 0x0B: DEC_RR(vm, R16_BC); break;
            case 0x0C: incrementR8(vm, R8_C); break;
            case 0x0D: decrementR8(vm, R8_C); break;
            case 0x0E: LOAD_R_D8(vm, R8_C); break;
            case 0x0F: rotateRight(vm, R8_A); break;
            /* OPCODE : STOP, for testing only */
            case 0x10: return;
            case 0x11: LOAD_RR_D16(vm, R16_DE); break;
            case 0x12: LOAD_RR_R(vm, R16_DE, R8_A); break;
            case 0x13: INC_RR(vm, R16_DE); break;
            case 0x14: incrementR8(vm, R8_D); break;
            case 0x15: decrementR8(vm, R8_D); break;
            case 0x16: LOAD_R_D8(vm, R8_D); break;
            case 0x17: rotateLeft(vm, R8_A); break;
            case 0x18: JUMP_RL(vm); break;
            case 0x19: addR16(vm, R16_HL, R16_DE); break;
            case 0x1A: LOAD_R_RR(vm, R8_A, R16_DE); break;
            case 0x1B: DEC_RR(vm, R16_DE); break;
            case 0x1C: incrementR8(vm, R8_E); break;
            case 0x1D: decrementR8(vm, R8_E); break;
            case 0x1E: LOAD_R_D8(vm, R8_E); break;
            case 0x1F: rotateRight(vm, R8_A); break;
            case 0x20: jumpRelativeCondition(vm, CONDITION_NZ(vm)); break;
            case 0x21: LOAD_RR_D16(vm, R16_HL); break;
            case 0x22: LOAD_RR_R(vm, R16_HL, R8_A); INC_RR(vm, R16_HL); break;
            case 0x23: INC_RR(vm, R16_SP); break;
            case 0x24: incrementR8(vm, R8_H); break;
            case 0x25: decrementR8(vm, R8_H); break;
            case 0x26: LOAD_R_D8(vm, R8_H); break;
            case 0x27: decimalAdjust(vm); break;
            case 0x28: jumpRelativeCondition(vm, CONDITION_Z(vm)); break;
            case 0x29: addR16(vm, R16_HL, R16_HL); break;
            case 0x2A: LOAD_R_RR(vm, R8_A, R16_HL); INC_RR(vm, R16_HL); break;
            case 0x2B: DEC_RR(vm, R16_HL); break;
            case 0x2C: incrementR8(vm, R8_L); break;
            case 0x2D: decrementR8(vm, R8_L); break;
            case 0x2E: LOAD_R_D8(vm, R8_L); break;
            case 0x2F: cpl(vm); break;
            case 0x30: jumpRelativeCondition(vm, CONDITION_NC(vm)); break;
            case 0x31: LOAD_RR_D16(vm, R16_SP); break;
            case 0x32: LOAD_RR_R(vm, R16_HL, R8_A); DEC_RR(vm, R16_HL); break;
            case 0x33: INC_RR(vm, R16_SP); break;
            case 0x34: {
                uint16_t old = get_reg16(vm, R16_HL);
                uint16_t new = INC_RR(vm, R16_HL); 
                
                TEST_Z_FLAG(vm, new);
                TEST_H_FLAG_ADD(vm, old, new);
                set_flag(vm, FLAG_N, 0);
                break;
            }
            case 0x35: {
                uint16_t old = get_reg16(vm, R16_HL);
                uint16_t new = INC_RR(vm, R16_HL); 
                
                TEST_Z_FLAG(vm, new);
                TEST_H_FLAG_SUB(vm, old, new);
                set_flag(vm, FLAG_N, 1);
                break;
            }
            case 0x36: LOAD_RR_D8(vm, R16_HL); break;
            case 0x37: {
                set_flag(vm, FLAG_C, 1);
                set_flag(vm, FLAG_N, 0);
                set_flag(vm, FLAG_H, 0);
                break;
            }
            case 0x38: jumpRelativeCondition(vm, CONDITION_C(vm)); break;
            case 0x39: addR16(vm, R16_HL, R16_SP); break;
            case 0x3A: {
                LOAD_R_RR(vm, R8_A, R16_HL);
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
            case 0x46: LOAD_R_RR(vm, R8_B, R16_HL); break;
            case 0x47: LOAD_R_R(vm, R8_B, R8_A); break;
            case 0x48: LOAD_R_R(vm, R8_C, R8_B); break;
            case 0x49: LOAD_R_R(vm, R8_C, R8_C); break;
            case 0x4A: LOAD_R_R(vm, R8_C, R8_D); break;
            case 0x4B: LOAD_R_R(vm, R8_C, R8_E); break;
            case 0x4C: LOAD_R_R(vm, R8_C, R8_H); break;
            case 0x4D: LOAD_R_R(vm, R8_C, R8_L); break;
            case 0x4E: LOAD_R_RR(vm, R8_C, R16_HL); break;
            case 0x4F: LOAD_R_R(vm, R8_C, R8_A); break;
            case 0x50: LOAD_R_R(vm, R8_D, R8_B); break;
            case 0x51: LOAD_R_R(vm, R8_D, R8_C); break;
            case 0x52: LOAD_R_R(vm, R8_D, R8_D); break;
            case 0x53: LOAD_R_R(vm, R8_D, R8_E); break;
            case 0x54: LOAD_R_R(vm, R8_D, R8_H); break;
            case 0x55: LOAD_R_R(vm, R8_D, R8_L); break;
            case 0x56: LOAD_R_RR(vm, R8_D, R16_HL); break;
            case 0x57: LOAD_R_R(vm, R8_D, R8_A); break;
            case 0x58: LOAD_R_R(vm, R8_E, R8_B); break;
            case 0x59: LOAD_R_R(vm, R8_E, R8_C); break;
            case 0x5A: LOAD_R_R(vm, R8_E, R8_D); break;
            case 0x5B: LOAD_R_R(vm, R8_E, R8_E); break;
            case 0x5C: LOAD_R_R(vm, R8_E, R8_H); break;
            case 0x5D: LOAD_R_R(vm, R8_E, R8_L); break;
            case 0x5E: LOAD_R_RR(vm, R8_E, R16_HL); break;
            case 0x5F: LOAD_R_R(vm, R8_E, R8_A); break;
            case 0x60: LOAD_R_R(vm, R8_H, R8_B); break;
            case 0x61: LOAD_R_R(vm, R8_H, R8_C); break;
            case 0x62: LOAD_R_R(vm, R8_H, R8_D); break;
            case 0x63: LOAD_R_R(vm, R8_H, R8_E); break;
            case 0x64: LOAD_R_R(vm, R8_H, R8_H); break;
            case 0x65: LOAD_R_R(vm, R8_H, R8_L); break;
            case 0x66: LOAD_R_RR(vm, R8_H, R16_HL); break;
            case 0x67: LOAD_R_R(vm, R8_H, R8_A); break;
            case 0x68: LOAD_R_R(vm, R8_L, R8_B); break;
            case 0x69: LOAD_R_R(vm, R8_L, R8_C); break;
            case 0x6A: LOAD_R_R(vm, R8_L, R8_D); break;
            case 0x6B: LOAD_R_R(vm, R8_L, R8_E); break;
            case 0x6C: LOAD_R_R(vm, R8_L, R8_H); break;
            case 0x6D: LOAD_R_R(vm, R8_L, R8_L); break;
            case 0x6E: LOAD_R_RR(vm, R8_L, R16_HL); break;
            case 0x6F: LOAD_R_R(vm, R8_L, R8_A); break;
            case 0x70: LOAD_RR_R(vm, R16_HL, R8_B); break;
            case 0x71: LOAD_RR_R(vm, R16_HL, R8_C); break;
            case 0x72: LOAD_RR_R(vm, R16_HL, R8_D); break;
            case 0x73: LOAD_RR_R(vm, R16_HL, R8_E); break;
            case 0x74: LOAD_RR_R(vm, R16_HL, R8_H); break;
            case 0x75: LOAD_RR_R(vm, R16_HL, R8_L); break;
            /* TODO: HALT */ 
            case 0x76: break;
            case 0x77: LOAD_RR_R(vm, R16_HL, R8_A); break;
            case 0x78: LOAD_R_R(vm, R8_A, R8_B); break;
            case 0x79: LOAD_R_R(vm, R8_A, R8_C); break;
            case 0x7A: LOAD_R_R(vm, R8_A, R8_D); break;
            case 0x7B: LOAD_R_R(vm, R8_A, R8_E); break;
            case 0x7C: LOAD_R_R(vm, R8_A, R8_H); break;
            case 0x7D: LOAD_R_R(vm, R8_A, R8_L); break;
            case 0x7E: LOAD_R_RR(vm, R8_A, R16_HL); break;
            case 0x7F: LOAD_R_R(vm, R8_A, R8_A); break;
            case 0x80: addR8(vm, R8_A, R8_B); break;
            case 0x81: addR8(vm, R8_A, R8_C); break;
            case 0x82: addR8(vm, R8_A, R8_D); break;
            case 0x83: addR8(vm, R8_A, R8_E); break;
            case 0x84: addR8(vm, R8_A, R8_H); break;
            case 0x85: addR8(vm, R8_A, R8_L); break;
            case 0x86: addR8R16(vm, R8_A, R16_HL); break;
            case 0x87: addR8(vm, R8_A, R8_A); break;
            case 0x88: adcR8(vm, R8_A, R8_B); break;
            case 0x89: adcR8(vm, R8_A, R8_C); break;
            case 0x8A: adcR8(vm, R8_A, R8_D); break;
            case 0x8B: adcR8(vm, R8_A, R8_E); break;
            case 0x8C: adcR8(vm, R8_A, R8_H); break;
            case 0x8D: adcR8(vm, R8_A, R8_L); break;
            case 0x8E: adcR8R16(vm, R8_A, R16_HL); break;
            case 0x8F: adcR8(vm, R8_A, R8_A); break;
            case 0x90: subR8(vm, R8_A, R8_B); break;
            case 0x91: subR8(vm, R8_A, R8_C); break;
            case 0x92: subR8(vm, R8_A, R8_D); break;
            case 0x93: subR8(vm, R8_A, R8_E); break;
            case 0x94: subR8(vm, R8_A, R8_H); break;
            case 0x95: subR8(vm, R8_A, R8_L); break;
            case 0x96: subR8R16(vm, R8_A, R16_HL); break;
            case 0x97: subR8(vm, R8_A, R8_A); break;
            case 0x98: sbcR8(vm, R8_A, R8_B); break;
            case 0x99: sbcR8(vm, R8_A, R8_C); break;
            case 0x9A: sbcR8(vm, R8_A, R8_D); break;
            case 0x9B: sbcR8(vm, R8_A, R8_E); break;
            case 0x9C: sbcR8(vm, R8_A, R8_H); break;
            case 0x9D: sbcR8(vm, R8_A, R8_L); break;
            case 0x9E: sbcR8R16(vm, R8_A, R16_HL); break;
            case 0x9F: sbcR8(vm, R8_A, R8_A); break;
            case 0xA0: 
            case 0xC3: JUMP(vm); break;
        }
    }    
}

void loadCartridge(VM *vm, Cartridge *cartridge) {
    vm->cartridge = cartridge;
    cartridge->inserted = true;
#ifdef PRINT_CARTRIDGE_INFO 
    printCartridge(cartridge);
#endif
    /* Cartridge is now in inserted state */
    bootROM(vm);
    runVM(vm);
}


