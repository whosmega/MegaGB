#include <stdio.h>
#include "../include/debug.h"

void log_fatal(GB* gb, const char* string) {
    printf("[FATAL]");
    printf(" %s", string);
    printf("\n");

    stopEmulator(gb);
    exit(99);
}

void log_warning(GB* gb, const char* string) {
    printf("[WARNING]");
    printf(" %s", string);
    printf("\n");
}

static uint16_t read2Bytes(GB* gb) {
    uint8_t b1 = gb->MEM[gb->PC + 1];
    uint8_t b2 = gb->MEM[gb->PC + 2];
    uint16_t D16 = (b2 << 8) | b1;
    return D16;
}

static void printFlags(GB* gb) {
    uint8_t flagState = gb->GPR[R8_F];

    printf("[Z%d", flagState >> 7);
    printf(" N%d", (flagState >> 6) & 1);
    printf(" H%d", (flagState >> 5) & 1);
    printf(" C%d]", (flagState >> 4) & 1);
}

static void simpleInstruction(GB* gb, char* ins) {
    printf("%s\n", ins);;
}

static void d16(GB* gb, char* ins) {
    printf("%s (0x%04x)\n", ins, read2Bytes(gb));
}

static void d8(GB* gb, char* ins) {
    printf("%s (0x%02x)\n", ins, gb->MEM[gb->PC + 1]);
}

static void a16(GB* gb, char* ins) {
    printf("%s (0x%04x)\n", ins, read2Bytes(gb));
}

static void r8(GB* gb, char* ins) {
    printf("%s (%d)\n", ins, (int8_t)gb->MEM[gb->PC + 1]);
}

void printCBInstruction(GB* gb, uint8_t byte) {
#ifdef DEBUG_PRINT_ADDRESS
    printf("[0x%04x]", gb->PC - 1);
#endif
#ifdef DEBUG_PRINT_FLAGS
    printFlags(gb);
#endif
#ifdef DEBUG_PRINT_CYCLES
    printf("[%ld]", gb->clock);
#endif
#ifdef DEBUG_PRINT_JOYPAD_REG
    printf("[sel:%x|", (gb->MEM[R_P1_JOYP] >> 4) & 0x3);
    printf("sig:%x]", (gb->MEM[R_P1_JOYP] & 0b00001111));
#endif
#ifdef DEBUG_PRINT_TIMERS
    printf("[%x|%x|%x|%x]", gb->MEM[R_DIV], gb->MEM[R_TIMA], gb->MEM[R_TMA], gb->MEM[R_TAC]);
#endif
    printf(" %5s", "");

    switch (byte) {
        case 0x00: return simpleInstruction(gb, "RLC B");
        case 0x01: return simpleInstruction(gb, "RLC C");
        case 0x02: return simpleInstruction(gb, "RLC D");
        case 0x03: return simpleInstruction(gb, "RLC E");
        case 0x04: return simpleInstruction(gb, "RLC H");
        case 0x05: return simpleInstruction(gb, "RLC L");
        case 0x06: return simpleInstruction(gb, "RLC (HL)");
        case 0x07: return simpleInstruction(gb, "RLC A");
        case 0x08: return simpleInstruction(gb, "RRC B");
        case 0x09: return simpleInstruction(gb, "RRC C");
        case 0x0A: return simpleInstruction(gb, "RRC D");
        case 0x0B: return simpleInstruction(gb, "RRC E");
        case 0x0C: return simpleInstruction(gb, "RRC H");
        case 0x0D: return simpleInstruction(gb, "RRC L");
        case 0x0E: return simpleInstruction(gb, "RRC (HL)");
        case 0x0F: return simpleInstruction(gb, "RRC A");
        case 0x10: return simpleInstruction(gb, "RL B");
        case 0x11: return simpleInstruction(gb, "RL C");
        case 0x12: return simpleInstruction(gb, "RL D");
        case 0x13: return simpleInstruction(gb, "RL E");
        case 0x14: return simpleInstruction(gb, "RL H");
        case 0x15: return simpleInstruction(gb, "RL L");
        case 0x16: return simpleInstruction(gb, "RL (HL)");
        case 0x17: return simpleInstruction(gb, "RL A");
        case 0x18: return simpleInstruction(gb, "RR B");
        case 0x19: return simpleInstruction(gb, "RR C");
        case 0x1A: return simpleInstruction(gb, "RR D");
        case 0x1B: return simpleInstruction(gb, "RR E");
        case 0x1C: return simpleInstruction(gb, "RR H");
        case 0x1D: return simpleInstruction(gb, "RR L");
        case 0x1E: return simpleInstruction(gb, "RR (HL)");
        case 0x1F: return simpleInstruction(gb, "RR A");
        case 0x20: return simpleInstruction(gb, "SLA B");
        case 0x21: return simpleInstruction(gb, "SLA C");
        case 0x22: return simpleInstruction(gb, "SLA D");
        case 0x23: return simpleInstruction(gb, "SLA E");
        case 0x24: return simpleInstruction(gb, "SLA H");
        case 0x25: return simpleInstruction(gb, "SLA L");
        case 0x26: return simpleInstruction(gb, "SLA (HL)");
        case 0x27: return simpleInstruction(gb, "SLA A");
        case 0x28: return simpleInstruction(gb, "SRA B");
        case 0x29: return simpleInstruction(gb, "SRA C");
        case 0x2A: return simpleInstruction(gb, "SRA D");
        case 0x2B: return simpleInstruction(gb, "SRA E");
        case 0x2C: return simpleInstruction(gb, "SRA H");
        case 0x2D: return simpleInstruction(gb, "SRA L");
        case 0x2E: return simpleInstruction(gb, "SRA (HL)");
        case 0x2F: return simpleInstruction(gb, "SRA A");
        case 0x30: return simpleInstruction(gb, "SWAP B");
        case 0x31: return simpleInstruction(gb, "SWAP C");
        case 0x32: return simpleInstruction(gb, "SWAP D");
        case 0x33: return simpleInstruction(gb, "SWAP E");
        case 0x34: return simpleInstruction(gb, "SWAP H");
        case 0x35: return simpleInstruction(gb, "SWAP L");
        case 0x36: return simpleInstruction(gb, "SWAP (HL)");
        case 0x37: return simpleInstruction(gb, "SWAP A");
        case 0x38: return simpleInstruction(gb, "SRL B");
        case 0x39: return simpleInstruction(gb, "SRL C");
        case 0x3A: return simpleInstruction(gb, "SRL D");
        case 0x3B: return simpleInstruction(gb, "SRL E");
        case 0x3C: return simpleInstruction(gb, "SRL H");
        case 0x3D: return simpleInstruction(gb, "SRL L");
        case 0x3E: return simpleInstruction(gb, "SRL (HL)");
        case 0x3F: return simpleInstruction(gb, "SRL A");
        case 0x40: return simpleInstruction(gb, "BIT 0, B");
        case 0x41: return simpleInstruction(gb, "BIT 0, C");
        case 0x42: return simpleInstruction(gb, "BIT 0, D");
        case 0x43: return simpleInstruction(gb, "BIT 0, E");
        case 0x44: return simpleInstruction(gb, "BIT 0, H");
        case 0x45: return simpleInstruction(gb, "BIT 0, L");
        case 0x46: return simpleInstruction(gb, "BIT 0, (HL)");
        case 0x47: return simpleInstruction(gb, "BIT 0, A");
        case 0x48: return simpleInstruction(gb, "BIT 1, B");
        case 0x49: return simpleInstruction(gb, "BIT 1, C");
        case 0x4A: return simpleInstruction(gb, "BIT 1, D");
        case 0x4B: return simpleInstruction(gb, "BIT 1, E");
        case 0x4C: return simpleInstruction(gb, "BIT 1, H");
        case 0x4D: return simpleInstruction(gb, "BIT 1, L");
        case 0x4E: return simpleInstruction(gb, "BIT 1, (HL)");
        case 0x4F: return simpleInstruction(gb, "BIT 1, A");
        case 0x50: return simpleInstruction(gb, "BIT 2, B");
        case 0x51: return simpleInstruction(gb, "BIT 2, C");
        case 0x52: return simpleInstruction(gb, "BIT 2, D");
        case 0x53: return simpleInstruction(gb, "BIT 2, E");
        case 0x54: return simpleInstruction(gb, "BIT 2, H");
        case 0x55: return simpleInstruction(gb, "BIT 2, L");
        case 0x56: return simpleInstruction(gb, "BIT 2, (HL)");
        case 0x57: return simpleInstruction(gb, "BIT 2, A");
        case 0x58: return simpleInstruction(gb, "BIT 3, B");
        case 0x59: return simpleInstruction(gb, "BIT 3, C");
        case 0x5A: return simpleInstruction(gb, "BIT 3, D");
        case 0x5B: return simpleInstruction(gb, "BIT 3, E");
        case 0x5C: return simpleInstruction(gb, "BIT 3, H");
        case 0x5D: return simpleInstruction(gb, "BIT 3, L");
        case 0x5E: return simpleInstruction(gb, "BIT 3, (HL)");
        case 0x5F: return simpleInstruction(gb, "BIT 3, A");
        case 0x60: return simpleInstruction(gb, "BIT 4, B");
        case 0x61: return simpleInstruction(gb, "BIT 4, C");
        case 0x62: return simpleInstruction(gb, "BIT 4, D");
        case 0x63: return simpleInstruction(gb, "BIT 4, E");
        case 0x64: return simpleInstruction(gb, "BIT 4, H");
        case 0x65: return simpleInstruction(gb, "BIT 4, L");
        case 0x66: return simpleInstruction(gb, "BIT 4, (HL)");
        case 0x67: return simpleInstruction(gb, "BIT 4, A");
        case 0x68: return simpleInstruction(gb, "BIT 5, B");
        case 0x69: return simpleInstruction(gb, "BIT 5, C");
        case 0x6A: return simpleInstruction(gb, "BIT 5, D");
        case 0x6B: return simpleInstruction(gb, "BIT 5, E");
        case 0x6C: return simpleInstruction(gb, "BIT 5, H");
        case 0x6D: return simpleInstruction(gb, "BIT 5, L");
        case 0x6E: return simpleInstruction(gb, "BIT 5, (HL)");
        case 0x6F: return simpleInstruction(gb, "BIT 5, A");
        case 0x70: return simpleInstruction(gb, "BIT 6, B");
        case 0x71: return simpleInstruction(gb, "BIT 6, C");
        case 0x72: return simpleInstruction(gb, "BIT 6, D");
        case 0x73: return simpleInstruction(gb, "BIT 6, E");
        case 0x74: return simpleInstruction(gb, "BIT 6, H");
        case 0x75: return simpleInstruction(gb, "BIT 6, L");
        case 0x76: return simpleInstruction(gb, "BIT 6, (HL)");
        case 0x77: return simpleInstruction(gb, "BIT 6, A");
        case 0x78: return simpleInstruction(gb, "BIT 7, B");
        case 0x79: return simpleInstruction(gb, "BIT 7, C");
        case 0x7A: return simpleInstruction(gb, "BIT 7, D");
        case 0x7B: return simpleInstruction(gb, "BIT 7, E");
        case 0x7C: return simpleInstruction(gb, "BIT 7, H");
        case 0x7D: return simpleInstruction(gb, "BIT 7, L");
        case 0x7E: return simpleInstruction(gb, "BIT 7, (HL)");
        case 0x7F: return simpleInstruction(gb, "BIT 7, A");
        case 0x80: return simpleInstruction(gb, "RES 0, B");
        case 0x81: return simpleInstruction(gb, "RES 0, C");
        case 0x82: return simpleInstruction(gb, "RES 0, D");
        case 0x83: return simpleInstruction(gb, "RES 0, E");
        case 0x84: return simpleInstruction(gb, "RES 0, H");
        case 0x85: return simpleInstruction(gb, "RES 0, L");
        case 0x86: return simpleInstruction(gb, "RES 0, (HL)");
        case 0x87: return simpleInstruction(gb, "RES 0, A");
        case 0x88: return simpleInstruction(gb, "RES 1, B");
        case 0x89: return simpleInstruction(gb, "RES 1, C");
        case 0x8A: return simpleInstruction(gb, "RES 1, D");
        case 0x8B: return simpleInstruction(gb, "RES 1, E");
        case 0x8C: return simpleInstruction(gb, "RES 1, H");
        case 0x8D: return simpleInstruction(gb, "RES 1, L");
        case 0x8E: return simpleInstruction(gb, "RES 1, (HL)");
        case 0x8F: return simpleInstruction(gb, "RES 1, A");
        case 0x90: return simpleInstruction(gb, "RES 2, B");
        case 0x91: return simpleInstruction(gb, "RES 2, C");
        case 0x92: return simpleInstruction(gb, "RES 2, D");
        case 0x93: return simpleInstruction(gb, "RES 2, E");
        case 0x94: return simpleInstruction(gb, "RES 2, H");
        case 0x95: return simpleInstruction(gb, "RES 2, L");
        case 0x96: return simpleInstruction(gb, "RES 2, (HL)");
        case 0x97: return simpleInstruction(gb, "RES 2, A");
        case 0x98: return simpleInstruction(gb, "RES 3, B");
        case 0x99: return simpleInstruction(gb, "RES 3, C");
        case 0x9A: return simpleInstruction(gb, "RES 3, D");
        case 0x9B: return simpleInstruction(gb, "RES 3, E");
        case 0x9C: return simpleInstruction(gb, "RES 3, H");
        case 0x9D: return simpleInstruction(gb, "RES 3, L");
        case 0x9E: return simpleInstruction(gb, "RES 3, (HL)");
        case 0x9F: return simpleInstruction(gb, "RES 3, A");
        case 0xA0: return simpleInstruction(gb, "RES 4, B");
        case 0xA1: return simpleInstruction(gb, "RES 4, C");
        case 0xA2: return simpleInstruction(gb, "RES 4, D");
        case 0xA3: return simpleInstruction(gb, "RES 4, E");
        case 0xA4: return simpleInstruction(gb, "RES 4, H");
        case 0xA5: return simpleInstruction(gb, "RES 4, L");
        case 0xA6: return simpleInstruction(gb, "RES 4, (HL)");
        case 0xA7: return simpleInstruction(gb, "RES 4, A");
        case 0xA8: return simpleInstruction(gb, "RES 5, B");
        case 0xA9: return simpleInstruction(gb, "RES 5, C");
        case 0xAA: return simpleInstruction(gb, "RES 5, D");
        case 0xAB: return simpleInstruction(gb, "RES 5, E");
        case 0xAC: return simpleInstruction(gb, "RES 5, H");
        case 0xAD: return simpleInstruction(gb, "RES 5, L");
        case 0xAE: return simpleInstruction(gb, "RES 5, (HL)");
        case 0xAF: return simpleInstruction(gb, "RES 5, A");
        case 0xB0: return simpleInstruction(gb, "RES 6, B");
        case 0xB1: return simpleInstruction(gb, "RES 6, C");
        case 0xB2: return simpleInstruction(gb, "RES 6, D");
        case 0xB3: return simpleInstruction(gb, "RES 6, E");
        case 0xB4: return simpleInstruction(gb, "RES 6, H");
        case 0xB5: return simpleInstruction(gb, "RES 6, L");
        case 0xB6: return simpleInstruction(gb, "RES 6, (HL)");
        case 0xB7: return simpleInstruction(gb, "RES 6, A");
        case 0xB8: return simpleInstruction(gb, "RES 7, B");
        case 0xB9: return simpleInstruction(gb, "RES 7, C");
        case 0xBA: return simpleInstruction(gb, "RES 7, D");
        case 0xBB: return simpleInstruction(gb, "RES 7, E");
        case 0xBC: return simpleInstruction(gb, "RES 7, H");
        case 0xBD: return simpleInstruction(gb, "RES 7, L");
        case 0xBE: return simpleInstruction(gb, "RES 7, (HL)");
        case 0xBF: return simpleInstruction(gb, "RES 7, A");
        case 0xC0: return simpleInstruction(gb, "SET 0, B");
        case 0xC1: return simpleInstruction(gb, "SET 0, C");
        case 0xC2: return simpleInstruction(gb, "SET 0, D");
        case 0xC3: return simpleInstruction(gb, "SET 0, E");
        case 0xC4: return simpleInstruction(gb, "SET 0, H");
        case 0xC5: return simpleInstruction(gb, "SET 0, L");
        case 0xC6: return simpleInstruction(gb, "SET 0, (HL)");
        case 0xC7: return simpleInstruction(gb, "SET 0, A");
        case 0xC8: return simpleInstruction(gb, "SET 1, B");
        case 0xC9: return simpleInstruction(gb, "SET 1, C");
        case 0xCA: return simpleInstruction(gb, "SET 1, D");
        case 0xCB: return simpleInstruction(gb, "SET 1, E");
        case 0xCC: return simpleInstruction(gb, "SET 1, H");
        case 0xCD: return simpleInstruction(gb, "SET 1, L");
        case 0xCE: return simpleInstruction(gb, "SET 1, (HL)");
        case 0xCF: return simpleInstruction(gb, "SET 1, A");
        case 0xD0: return simpleInstruction(gb, "SET 2, B");
        case 0xD1: return simpleInstruction(gb, "SET 2, C");
        case 0xD2: return simpleInstruction(gb, "SET 2, D");
        case 0xD3: return simpleInstruction(gb, "SET 2, E");
        case 0xD4: return simpleInstruction(gb, "SET 2, H");
        case 0xD5: return simpleInstruction(gb, "SET 2, L");
        case 0xD6: return simpleInstruction(gb, "SET 2, (HL)");
        case 0xD7: return simpleInstruction(gb, "SET 2, A");
        case 0xD8: return simpleInstruction(gb, "SET 3, B");
        case 0xD9: return simpleInstruction(gb, "SET 3, C");
        case 0xDA: return simpleInstruction(gb, "SET 3, D");
        case 0xDB: return simpleInstruction(gb, "SET 3, E");
        case 0xDC: return simpleInstruction(gb, "SET 3, H");
        case 0xDD: return simpleInstruction(gb, "SET 3, L");
        case 0xDE: return simpleInstruction(gb, "SET 3, (HL)");
        case 0xDF: return simpleInstruction(gb, "SET 3, A");
        case 0xE0: return simpleInstruction(gb, "SET 4, B");
        case 0xE1: return simpleInstruction(gb, "SET 4, C");
        case 0xE2: return simpleInstruction(gb, "SET 4, D");
        case 0xE3: return simpleInstruction(gb, "SET 4, E");
        case 0xE4: return simpleInstruction(gb, "SET 4, H");
        case 0xE5: return simpleInstruction(gb, "SET 4, L");
        case 0xE6: return simpleInstruction(gb, "SET 4, (HL)");
        case 0xE7: return simpleInstruction(gb, "SET 4, A");
        case 0xE8: return simpleInstruction(gb, "SET 5, B");
        case 0xE9: return simpleInstruction(gb, "SET 5, C");
        case 0xEA: return simpleInstruction(gb, "SET 5, D");
        case 0xEB: return simpleInstruction(gb, "SET 5, E");
        case 0xEC: return simpleInstruction(gb, "SET 5, H");
        case 0xED: return simpleInstruction(gb, "SET 5, L");
        case 0xEE: return simpleInstruction(gb, "SET 5, (HL)");
        case 0xEF: return simpleInstruction(gb, "SET 5, A");
        case 0xF0: return simpleInstruction(gb, "SET 6, B");
        case 0xF1: return simpleInstruction(gb, "SET 6, C");
        case 0xF2: return simpleInstruction(gb, "SET 6, D");
        case 0xF3: return simpleInstruction(gb, "SET 6, E");
        case 0xF4: return simpleInstruction(gb, "SET 6, H");
        case 0xF5: return simpleInstruction(gb, "SET 6, L");
        case 0xF6: return simpleInstruction(gb, "SET 6, (HL)");
        case 0xF7: return simpleInstruction(gb, "SET 6, A");
        case 0xF8: return simpleInstruction(gb, "SET 7, B");
        case 0xF9: return simpleInstruction(gb, "SET 7, C");
        case 0xFA: return simpleInstruction(gb, "SET 7, D");
        case 0xFB: return simpleInstruction(gb, "SET 7, E");
        case 0xFC: return simpleInstruction(gb, "SET 7, H");
        case 0xFD: return simpleInstruction(gb, "SET 7, L");
        case 0xFE: return simpleInstruction(gb, "SET 7, (HL)");
        case 0xFF: return simpleInstruction(gb, "SET 7, A");
    }
}

void printInstruction(GB* gb) {
#ifdef DEBUG_PRINT_ADDRESS
    printf("[0x%04x]", gb->PC);
#endif
#ifdef DEBUG_PRINT_FLAGS
    printFlags(gb);
#endif
#ifdef DEBUG_PRINT_CYCLES
    /* We print t-cycles */
    printf("[%ld]", gb->clock * 4);
#endif
#ifdef DEBUG_PRINT_JOYPAD_REG
    printf("[sel:%x|", (gb->MEM[R_P1_JOYP] >> 4) & 0x3);
    printf("sig:%x]", (gb->MEM[R_P1_JOYP] & 0b00001111));
#endif
#ifdef DEBUG_PRINT_TIMERS
    printf("[%x|%x|%x|%x]", gb->MEM[R_DIV], gb->MEM[R_TIMA], gb->MEM[R_TMA], gb->MEM[R_TAC]);
#endif
    printf(" %5s", "");

    switch (gb->MEM[gb->PC]) {
        case 0x00: return simpleInstruction(gb, "NOP");
        case 0x01: return d16(gb, "LD BC, d16");
        case 0x02: return simpleInstruction(gb, "LD (BC), A");
        case 0x03: return simpleInstruction(gb, "INC BC");
        case 0x04: return simpleInstruction(gb, "INC B");
        case 0x05: return simpleInstruction(gb, "DEC B");
        case 0x06: return d8(gb, "LD B, d8");
        case 0x07: return simpleInstruction(gb, "RLCA");
        case 0x08: return a16(gb, "LD a16, SP");
        case 0x09: return simpleInstruction(gb, "ADD HL, BC");
        case 0x0A: return simpleInstruction(gb, "LD A, (BC)");
        case 0x0B: return simpleInstruction(gb, "DEC BC");
        case 0x0C: return simpleInstruction(gb, "INC C");
        case 0x0D: return simpleInstruction(gb, "DEC C");
        case 0x0E: return d8(gb, "LD C, d8");
        case 0x0F: return simpleInstruction(gb, "RRCA");
        case 0x10: return simpleInstruction(gb, "STOP");
        case 0x11: return d16(gb, "LD DE, d16");
        case 0x12: return simpleInstruction(gb, "LD (DE), A");
        case 0x13: return simpleInstruction(gb, "INC DE");
        case 0x14: return simpleInstruction(gb, "INC D");
        case 0x15: return simpleInstruction(gb, "DEC D");
        case 0x16: return d8(gb, "LD D, d8");
        case 0x17: return simpleInstruction(gb, "RLA");
        case 0x18: return r8(gb, "JR r8");
        case 0x19: return simpleInstruction(gb, "ADD HL, DE");
        case 0x1A: return simpleInstruction(gb, "LD A, (DE)");
        case 0x1B: return simpleInstruction(gb, "DEC DE");
        case 0x1C: return simpleInstruction(gb, "INC E");
        case 0x1D: return simpleInstruction(gb, "DEC E");
        case 0x1E: return d8(gb, "LD E, D8");
        case 0x1F: return simpleInstruction(gb, "RRA");
        case 0x20: return r8(gb, "JR NZ, r8");
        case 0x21: return d16(gb, "LD HL, d16");
        case 0x22: return simpleInstruction(gb, "LD (HL+), A");
        case 0x23: return simpleInstruction(gb, "INC HL");
        case 0x24: return simpleInstruction(gb, "INC H");
        case 0x25: return simpleInstruction(gb, "DEC H");
        case 0x26: return d8(gb, "LD H, d8");
        case 0x27: return simpleInstruction(gb, "DAA");
        case 0x28: return r8(gb, "JR Z, r8");
        case 0x29: return simpleInstruction(gb, "ADD HL, HL");
        case 0x2A: return simpleInstruction(gb, "LD A, (HL+)");
        case 0x2B: return simpleInstruction(gb, "DEC HL");
        case 0x2C: return simpleInstruction(gb, "INC L");
        case 0x2D: return simpleInstruction(gb, "DEC L");
        case 0x2E: return d8(gb, "LD L, d8");
        case 0x2F: return simpleInstruction(gb, "CPL");
        case 0x30: return r8(gb, "JR NC, r8");
        case 0x31: return d16(gb, "LD SP,d16");
        case 0x32: return simpleInstruction(gb, "LD (HL-), A");
        case 0x33: return simpleInstruction(gb, "INC SP");
        case 0x34: return simpleInstruction(gb, "INC (HL)");
        case 0x35: return simpleInstruction(gb, "DEC (HL)");
        case 0x36: return d8(gb, "LD (HL), d8");
        case 0x37: return simpleInstruction(gb, "SCF");
        case 0x38: return r8(gb, "JR C, r8");
        case 0x39: return simpleInstruction(gb, "ADD HL, SP");
        case 0x3A: return simpleInstruction(gb, "LD A, (HL-)");
        case 0x3B: return simpleInstruction(gb, "DEC SP");
        case 0x3C: return simpleInstruction(gb, "INC A");
        case 0x3D: return simpleInstruction(gb, "DEC A");
        case 0x3E: return d8(gb, "LD A, d8");
        case 0x3F: return simpleInstruction(gb, "CCF");
        case 0x40: return simpleInstruction(gb, "LD B, B");
        case 0x41: return simpleInstruction(gb, "LD B, C");
        case 0x42: return simpleInstruction(gb, "LD B, D");
        case 0x43: return simpleInstruction(gb, "LD B, E");
        case 0x44: return simpleInstruction(gb, "LD B, H");
        case 0x45: return simpleInstruction(gb, "LD B, L");
        case 0x46: return simpleInstruction(gb, "LD B, (HL)");
        case 0x47: return simpleInstruction(gb, "LD B, A");
        case 0x48: return simpleInstruction(gb, "LD C, B");
        case 0x49: return simpleInstruction(gb, "LD C, C");
        case 0x4A: return simpleInstruction(gb, "LD C, D");
        case 0x4B: return simpleInstruction(gb, "LD C, E");
        case 0x4C: return simpleInstruction(gb, "LD C, H");
        case 0x4D: return simpleInstruction(gb, "LD C, L");
        case 0x4E: return simpleInstruction(gb, "LD C, (HL)");
        case 0x4F: return simpleInstruction(gb, "LD C, A");
        case 0x50: return simpleInstruction(gb, "LD D, B");
        case 0x51: return simpleInstruction(gb, "LD D, C");
        case 0x52: return simpleInstruction(gb, "LD D, D");
        case 0x53: return simpleInstruction(gb, "LD D, E");
        case 0x54: return simpleInstruction(gb, "LD D, H");
        case 0x55: return simpleInstruction(gb, "LD D, L");
        case 0x56: return simpleInstruction(gb, "LD D, (HL)");
        case 0x57: return simpleInstruction(gb, "LD D, A");
        case 0x58: return simpleInstruction(gb, "LD E, B");
        case 0x59: return simpleInstruction(gb, "LD E, C");
        case 0x5A: return simpleInstruction(gb, "LD E, D");
        case 0x5B: return simpleInstruction(gb, "LD E, E");
        case 0x5C: return simpleInstruction(gb, "LD E, H");
        case 0x5D: return simpleInstruction(gb, "LD E, L");
        case 0x5E: return simpleInstruction(gb, "LD E, (HL)");
        case 0x5F: return simpleInstruction(gb, "LD E, A");
        case 0x60: return simpleInstruction(gb, "LD H, B");
        case 0x61: return simpleInstruction(gb, "LD H, C");
        case 0x62: return simpleInstruction(gb, "LD H, D");
        case 0x63: return simpleInstruction(gb, "LD H, E");
        case 0x64: return simpleInstruction(gb, "LD H, H");
        case 0x65: return simpleInstruction(gb, "LD H, L");
        case 0x66: return simpleInstruction(gb, "LD H, (HL)");
        case 0x67: return simpleInstruction(gb, "LD H, A");
        case 0x68: return simpleInstruction(gb, "LD L, B");
        case 0x69: return simpleInstruction(gb, "LD L, C");
        case 0x6A: return simpleInstruction(gb, "LD L, D");
        case 0x6B: return simpleInstruction(gb, "LD L, E");
        case 0x6C: return simpleInstruction(gb, "LD L, H");
        case 0x6D: return simpleInstruction(gb, "LD L, L");
        case 0x6E: return simpleInstruction(gb, "LD L, (HL)");
        case 0x6F: return simpleInstruction(gb, "LD L, A");
        case 0x70: return simpleInstruction(gb, "LD (HL), B");
        case 0x71: return simpleInstruction(gb, "LD (HL), C");
        case 0x72: return simpleInstruction(gb, "LD (HL), D");
        case 0x73: return simpleInstruction(gb, "LD (HL), E");
        case 0x74: return simpleInstruction(gb, "LD (HL), H");
        case 0x75: return simpleInstruction(gb, "LD (HL), L");
        case 0x76: return simpleInstruction(gb, "HALT");
        case 0x77: return simpleInstruction(gb, "LD (HL), A");
        case 0x78: return simpleInstruction(gb, "LD A, B");
        case 0x79: return simpleInstruction(gb, "LD A, C");
        case 0x7A: return simpleInstruction(gb, "LD A, D");
        case 0x7B: return simpleInstruction(gb, "LD A, E");
        case 0x7C: return simpleInstruction(gb, "LD A, H");
        case 0x7D: return simpleInstruction(gb, "LD A, L");
        case 0x7E: return simpleInstruction(gb, "LD A, (HL)");
        case 0x7F: return simpleInstruction(gb, "LD A, A");
        case 0x80: return simpleInstruction(gb, "ADD A, B");
        case 0x81: return simpleInstruction(gb, "ADD A, C");
        case 0x82: return simpleInstruction(gb, "ADD A, D");
        case 0x83: return simpleInstruction(gb, "ADD A, E");
        case 0x84: return simpleInstruction(gb, "ADD A, H");
        case 0x85: return simpleInstruction(gb, "ADD A, L");
        case 0x86: return simpleInstruction(gb, "ADD A, (HL)");
        case 0x87: return simpleInstruction(gb, "ADD A, A");
        case 0x88: return simpleInstruction(gb, "ADC A, B");
        case 0x89: return simpleInstruction(gb, "ADC A, C");
        case 0x8A: return simpleInstruction(gb, "ADC A, D");
        case 0x8B: return simpleInstruction(gb, "ADC A, E");
        case 0x8C: return simpleInstruction(gb, "ADC A, H");
        case 0x8D: return simpleInstruction(gb, "ADC A, L");
        case 0x8E: return simpleInstruction(gb, "ADC A, (HL)");
        case 0x8F: return simpleInstruction(gb, "ADC A, A");
        case 0x90: return simpleInstruction(gb, "SUB B");
        case 0x91: return simpleInstruction(gb, "SUB C");
        case 0x92: return simpleInstruction(gb, "SUB D");
        case 0x93: return simpleInstruction(gb, "SUB E");
        case 0x94: return simpleInstruction(gb, "SUB H");
        case 0x95: return simpleInstruction(gb, "SUB L");
        case 0x96: return simpleInstruction(gb, "SUB (HL)");
        case 0x97: return simpleInstruction(gb, "SUB A");
        case 0x98: return simpleInstruction(gb, "SBC A, B");
        case 0x99: return simpleInstruction(gb, "SBC A, C");
        case 0x9A: return simpleInstruction(gb, "SBC A, D");
        case 0x9B: return simpleInstruction(gb, "SBC A, E");
        case 0x9C: return simpleInstruction(gb, "SBC A, H");
        case 0x9D: return simpleInstruction(gb, "SBC A, L");
        case 0x9E: return simpleInstruction(gb, "SBC A, (HL)");
        case 0x9F: return simpleInstruction(gb, "SBC A, A");
        case 0xA0: return simpleInstruction(gb, "AND B");
        case 0xA1: return simpleInstruction(gb, "AND C");
        case 0xA2: return simpleInstruction(gb, "AND D");
        case 0xA3: return simpleInstruction(gb, "AND E");
        case 0xA4: return simpleInstruction(gb, "AND H");
        case 0xA5: return simpleInstruction(gb, "AND L");
        case 0xA6: return simpleInstruction(gb, "AND (HL)");
        case 0xA7: return simpleInstruction(gb, "AND A");
        case 0xA8: return simpleInstruction(gb, "XOR B");
        case 0xA9: return simpleInstruction(gb, "XOR C");
        case 0xAA: return simpleInstruction(gb, "XOR D");
        case 0xAB: return simpleInstruction(gb, "XOR E");
        case 0xAC: return simpleInstruction(gb, "XOR H");
        case 0xAD: return simpleInstruction(gb, "XOR L");
        case 0xAE: return simpleInstruction(gb, "XOR (HL)");
        case 0xAF: return simpleInstruction(gb, "XOR A");
        case 0xB0: return simpleInstruction(gb, "OR B");
        case 0xB1: return simpleInstruction(gb, "OR C");
        case 0xB2: return simpleInstruction(gb, "OR D");
        case 0xB3: return simpleInstruction(gb, "OR E");
        case 0xB4: return simpleInstruction(gb, "OR H");
        case 0xB5: return simpleInstruction(gb, "OR L");
        case 0xB6: return simpleInstruction(gb, "OR (HL)");
        case 0xB7: return simpleInstruction(gb, "OR A");
        case 0xB8: return simpleInstruction(gb, "CP B");
        case 0xB9: return simpleInstruction(gb, "CP C");
        case 0xBA: return simpleInstruction(gb, "CP D");
        case 0xBB: return simpleInstruction(gb, "CP E");
        case 0xBC: return simpleInstruction(gb, "CP H");
        case 0xBD: return simpleInstruction(gb, "CP L");
        case 0xBE: return simpleInstruction(gb, "CP (HL)");
        case 0xBF: return simpleInstruction(gb, "CP A");
        case 0xC0: return simpleInstruction(gb, "RET NZ");
        case 0xC1: return simpleInstruction(gb, "POP BC");
        case 0xC2: return a16(gb, "JP NZ, a16");
        case 0xC3: return a16(gb, "JP a16");
        case 0xC4: return a16(gb, "CALL NZ, a16");
        case 0xC5: return simpleInstruction(gb, "PUSH BC");
        case 0xC6: return d8(gb, "ADD A, d8");
        case 0xC7: return simpleInstruction(gb, "RST 0x00");
        case 0xC8: return simpleInstruction(gb, "RET Z");
        case 0xC9: return simpleInstruction(gb, "RET");
        case 0xCA: return a16(gb, "JP Z, a16");
        case 0xCB: return simpleInstruction(gb, "PREFIX CB");
        case 0xCC: return a16(gb, "CALL Z, a16");
        case 0xCD: return a16(gb, "CALL a16");
        case 0xCE: return d8(gb, "ADC A, d8");
        case 0xCF: return simpleInstruction(gb, "RST 0x08");
        case 0xD0: return simpleInstruction(gb, "RET NC");
        case 0xD1: return simpleInstruction(gb, "POP DE");
        case 0xD2: return a16(gb, "JP NC, a16");
        case 0xD4: return a16(gb, "CALL NC, a16");
        case 0xD5: return simpleInstruction(gb, "PUSH DE");
        case 0xD6: return d8(gb, "SUB d8");
        case 0xD7: return simpleInstruction(gb, "RST 0x10");
        case 0xD8: return simpleInstruction(gb, "REC C");
        case 0xD9: return simpleInstruction(gb, "RETI");
        case 0xDA: return a16(gb, "JP C, a16");
        case 0xDC: return a16(gb, "CALL C, a16");
        case 0xDE: return d8(gb, "SBC A, d8");
        case 0xDF: return simpleInstruction(gb, "RST 0x18");
        case 0xE0: return d8(gb, "LD (0xFF00 + d8), A");
        case 0xE1: return simpleInstruction(gb, "POP HL");
        case 0xE2: return simpleInstruction(gb, "LD (0xFF00 + C), A");
        case 0xE5: return simpleInstruction(gb, "PUSH HL");
        case 0xE6: return d8(gb, "AND d8");
        case 0xE7: return simpleInstruction(gb, "RST 0x20");
        case 0xE8: return r8(gb, "ADD SP, r8");
        case 0xE9: return simpleInstruction(gb, "JP (HL)");
        case 0xEA: return a16(gb, "LD (a16), A");
        case 0xEE: return d8(gb, "XOR d8");
        case 0xEF: return simpleInstruction(gb, "RST 0x28");
        case 0xF0: return d8(gb, "LD A, (0xFF00 + d8)");
        case 0xF1: return simpleInstruction(gb, "POP AF");
        case 0xF2: return simpleInstruction(gb, "LD A, (0xFF00 + C)");
        case 0xF3: return simpleInstruction(gb, "DI");
        case 0xF5: return simpleInstruction(gb, "PUSH AF");
        case 0xF6: return d8(gb, "OR d8");
        case 0xF7: return simpleInstruction(gb, "RST 0x30");
        case 0xF8: return r8(gb, "LD HL, SP + r8");
        case 0xF9: return simpleInstruction(gb, "LD SP, HL");
        case 0xFA: return a16(gb, "LD A, (a16)");
        case 0xFB: return simpleInstruction(gb, "EI");
        case 0xFE: return d8(gb, "CP d8");
        case 0xFF: return simpleInstruction(gb, "RST 0x38");
        default: return simpleInstruction(gb, "????");
    }
}

void printRegisters(GB* gb) {
    printf("[A%02x|B%02x|C%02x|D%02x|E%02x|H%02x|L%02x|SP%04x]\n",
            gb->GPR[R8_A], gb->GPR[R8_B], gb->GPR[R8_C],
            gb->GPR[R8_D], gb->GPR[R8_E], gb->GPR[R8_H],
            gb->GPR[R8_L], (uint16_t)(gb->GPR[R8_SP_HIGH] << 8) + gb->GPR[R8_SP_LOW]);
}
