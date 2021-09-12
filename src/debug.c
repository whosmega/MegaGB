#include <stdio.h>
#include "../include/debug.h"

static uint16_t read2Bytes(VM* vm) {
    uint8_t b1 = vm->MEM[vm->PC + 1];
    uint8_t b2 = vm->MEM[vm->PC + 2];
    uint16_t D16 = (b2 << 8) | b1;
    return D16;
}

static void printFlags(VM* vm) {
    uint8_t flagState = vm->GPR[R8_F];
    
    printf("[Z%d", flagState >> 7);
    printf(" N%d", (flagState >> 6) & 1);
    printf(" H%d", (flagState >> 5) & 1);
    printf(" C%d]", (flagState >> 4) & 1);
}

static void simpleInstruction(VM* vm, char* ins) {
    printf("%s\n", ins);;
}

static void d16(VM* vm, char* ins) {
    printf("%s (%d)\n", ins, read2Bytes(vm));
}

static void d8(VM* vm, char* ins) {
    printf("%s (%d)\n", ins, vm->MEM[vm->PC + 1]);
}

static void a16(VM* vm, char* ins) {
    printf("%s (0x%x)\n", ins, read2Bytes(vm));
}

static void r8(VM* vm, char* ins) {
    printf("%s (%d)\n", ins, (int8_t)vm->MEM[vm->PC + 1]);
}

void printInstruction(VM* vm) {
    printf("[0x%04x]", vm->PC);
    printFlags(vm);
    printf(" %10s", "");
 
    switch (vm->MEM[vm->PC]) {
        case 0x00: return simpleInstruction(vm, "NOP");
        case 0x01: return d16(vm, "LD BC, d16");
        case 0x02: return simpleInstruction(vm, "LD (BC), A");
        case 0x03: return simpleInstruction(vm, "INC BC");
        case 0x04: return simpleInstruction(vm, "INC B");
        case 0x05: return simpleInstruction(vm, "DEB B");
        case 0x06: return d8(vm, "LD B, d8");
        case 0x07: return simpleInstruction(vm, "RLCA");
        case 0x08: return a16(vm, "LD a16, SP");
        case 0x09: return simpleInstruction(vm, "ADD HL, BC");
        case 0x0A: return simpleInstruction(vm, "LD A, (BC)");
        case 0x0B: return simpleInstruction(vm, "DEC BC");
        case 0x0C: return simpleInstruction(vm, "INC C");
        case 0x0D: return simpleInstruction(vm, "DEC C");
        case 0x0E: return d8(vm, "LD C, d8");
        case 0x0F: return simpleInstruction(vm, "RRCA");
        case 0x10: return simpleInstruction(vm, "STOP");
        case 0x11: return d16(vm, "LD DE, d16");
        case 0x12: return simpleInstruction(vm, "LD (DE), A");
        case 0x13: return simpleInstruction(vm, "INC DE");
        case 0x14: return simpleInstruction(vm, "INC D");
        case 0x15: return simpleInstruction(vm, "DEC D");
        case 0x16: return d8(vm, "LD D, d8");
        case 0x17: return simpleInstruction(vm, "RLA");
        case 0x18: return r8(vm, "JR r8");
        case 0x19: return simpleInstruction(vm, "ADD HL, DE");
        case 0x1A: return simpleInstruction(vm, "LD A, (DE)");
        case 0x1B: return simpleInstruction(vm, "DEC DE");
        case 0x1C: return simpleInstruction(vm, "INC E");
        case 0x1D: return simpleInstruction(vm, "DEC E");
        case 0x1E: return d8(vm, "LD E, D8");
        case 0x1F: return simpleInstruction(vm, "RRA");
        case 0x20: return r8(vm, "JR NZ, r8");
        case 0x21: return d16(vm, "LD HL, d16");
        case 0x22: return simpleInstruction(vm, "LD (HL+), A");
        case 0x23: return simpleInstruction(vm, "INC HL");
        case 0x24: return simpleInstruction(vm, "INC H");
        case 0x25: return simpleInstruction(vm, "DEC H");
        case 0x26: return d8(vm, "LD H, d8");
        case 0x27: return simpleInstruction(vm, "DAA");
        case 0x28: return r8(vm, "JR Z, r8");
        case 0x29: return simpleInstruction(vm, "ADD HL, HL");
        case 0x2A: return simpleInstruction(vm, "LD A, (HL+)");
        case 0x2B: return simpleInstruction(vm, "DEC HL");
        case 0x2C: return simpleInstruction(vm, "INC L");
        case 0x2D: return simpleInstruction(vm, "DEC L");
        case 0x2E: return d8(vm, "LD L, d8");
        case 0x2F: return simpleInstruction(vm, "CPL");
        case 0x30: return r8(vm, "JR NC, r8");
        case 0x31: return d16(vm, "LD SP,d16");
        case 0x32: return simpleInstruction(vm, "LD (HL-), A");
        case 0x33: return simpleInstruction(vm, "INC SP");
        case 0x34: return simpleInstruction(vm, "INC (HL)");
        case 0x35: return simpleInstruction(vm, "DEC (HL)");
        case 0x36: return d8(vm, "LD (HL), d8");
        case 0x37: return simpleInstruction(vm, "SCF");
        case 0x38: return r8(vm, "JR C, r8");
        case 0x39: return simpleInstruction(vm, "ADD HL, SP");
        case 0x3A: return simpleInstruction(vm, "LD A, (HL-)");
        case 0x3B: return simpleInstruction(vm, "DEC SP");
        case 0x3C: return simpleInstruction(vm, "INC A");
        case 0x3D: return simpleInstruction(vm, "DEC A");
        case 0x3E: return d8(vm, "LD A, d8");
        case 0x3F: return simpleInstruction(vm, "CCF");
        case 0x40: return simpleInstruction(vm, "LD B, B");
        case 0x41: return simpleInstruction(vm, "LD B, C");
        case 0x42: return simpleInstruction(vm, "LD B, D");
        case 0x43: return simpleInstruction(vm, "LD B, E");
        case 0x44: return simpleInstruction(vm, "LD B, H");
        case 0x45: return simpleInstruction(vm, "LD B, L");
        case 0x46: return simpleInstruction(vm, "LD B, (HL)");
        case 0x47: return simpleInstruction(vm, "LD B, A");
        case 0x48: return simpleInstruction(vm, "LD C, B");
        case 0x49: return simpleInstruction(vm, "LD C, C");
        case 0x4A: return simpleInstruction(vm, "LD C, D");
        case 0x4B: return simpleInstruction(vm, "LD C, E");
        case 0x4C: return simpleInstruction(vm, "LD C, H");
        case 0x4D: return simpleInstruction(vm, "LD C, L");
        case 0x4E: return simpleInstruction(vm, "LD C, (HL)");
        case 0x4F: return simpleInstruction(vm, "LD C, A");
        case 0xC3: return a16(vm, "JP a16");
        default: return simpleInstruction(vm, "????");
    }
}

