#include <stdio.h>
#include <gb/debug.h>

void log_fatal(GB* gb, const char* string) {
    printf("[FATAL]");
    printf(" %s", string);
    printf("\n");

    stopGBEmulator(gb);
    exit(99);
}

void log_warning(GB* gb, const char* string) {
    printf("[WARNING]");
    printf(" %s", string);
    printf("\n");
}

static uint16_t read2Bytes(GB* gb) {
    uint8_t b1 = readAddr(gb, gb->PC + 1);
    uint8_t b2 = readAddr(gb, gb->PC + 2);
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

static void simpleInstruction(GB* gb, char* ins, char* output) {
    sprintf(output, "%s", ins);
}

static void d16(GB* gb, char* fins, char* output) {
    sprintf(output, fins, read2Bytes(gb));
}

static void d8(GB* gb, char* fins, char* output) {
    sprintf(output, fins, readAddr(gb, gb->PC + 1));
}

static void a16(GB* gb, char* fins, char* output) {
    sprintf(output, fins, read2Bytes(gb));
}

static void r8(GB* gb, char* fins, char* output) {
    sprintf(output, fins, (int8_t)readAddr(gb, gb->PC + 1));
}

void disassembleCBInstruction(GB* gb, uint8_t byte, char* output) {
	switch (byte) {
        case 0x00: return simpleInstruction(gb, "RLC B", output);
        case 0x01: return simpleInstruction(gb, "RLC C", output);
        case 0x02: return simpleInstruction(gb, "RLC D", output);
        case 0x03: return simpleInstruction(gb, "RLC E", output);
        case 0x04: return simpleInstruction(gb, "RLC H", output);
        case 0x05: return simpleInstruction(gb, "RLC L", output);
        case 0x06: return simpleInstruction(gb, "RLC (HL)", output);
        case 0x07: return simpleInstruction(gb, "RLC A", output);
        case 0x08: return simpleInstruction(gb, "RRC B", output);
        case 0x09: return simpleInstruction(gb, "RRC C", output);
        case 0x0A: return simpleInstruction(gb, "RRC D", output);
        case 0x0B: return simpleInstruction(gb, "RRC E", output);
        case 0x0C: return simpleInstruction(gb, "RRC H", output);
        case 0x0D: return simpleInstruction(gb, "RRC L", output);
        case 0x0E: return simpleInstruction(gb, "RRC (HL)", output);
        case 0x0F: return simpleInstruction(gb, "RRC A", output);
        case 0x10: return simpleInstruction(gb, "RL B", output);
        case 0x11: return simpleInstruction(gb, "RL C", output);
        case 0x12: return simpleInstruction(gb, "RL D", output);
        case 0x13: return simpleInstruction(gb, "RL E", output);
        case 0x14: return simpleInstruction(gb, "RL H", output);
        case 0x15: return simpleInstruction(gb, "RL L", output);
        case 0x16: return simpleInstruction(gb, "RL (HL)", output);
        case 0x17: return simpleInstruction(gb, "RL A", output);
        case 0x18: return simpleInstruction(gb, "RR B", output);
        case 0x19: return simpleInstruction(gb, "RR C", output);
        case 0x1A: return simpleInstruction(gb, "RR D", output);
        case 0x1B: return simpleInstruction(gb, "RR E", output);
        case 0x1C: return simpleInstruction(gb, "RR H", output);
        case 0x1D: return simpleInstruction(gb, "RR L", output);
        case 0x1E: return simpleInstruction(gb, "RR (HL)", output);
        case 0x1F: return simpleInstruction(gb, "RR A", output);
        case 0x20: return simpleInstruction(gb, "SLA B", output);
        case 0x21: return simpleInstruction(gb, "SLA C", output);
        case 0x22: return simpleInstruction(gb, "SLA D", output);
        case 0x23: return simpleInstruction(gb, "SLA E", output);
        case 0x24: return simpleInstruction(gb, "SLA H", output);
        case 0x25: return simpleInstruction(gb, "SLA L", output);
        case 0x26: return simpleInstruction(gb, "SLA (HL)", output);
        case 0x27: return simpleInstruction(gb, "SLA A", output);
        case 0x28: return simpleInstruction(gb, "SRA B", output);
        case 0x29: return simpleInstruction(gb, "SRA C", output);
        case 0x2A: return simpleInstruction(gb, "SRA D", output);
        case 0x2B: return simpleInstruction(gb, "SRA E", output);
        case 0x2C: return simpleInstruction(gb, "SRA H", output);
        case 0x2D: return simpleInstruction(gb, "SRA L", output);
        case 0x2E: return simpleInstruction(gb, "SRA (HL)", output);
        case 0x2F: return simpleInstruction(gb, "SRA A", output);
        case 0x30: return simpleInstruction(gb, "SWAP B", output);
        case 0x31: return simpleInstruction(gb, "SWAP C", output);
        case 0x32: return simpleInstruction(gb, "SWAP D", output);
        case 0x33: return simpleInstruction(gb, "SWAP E", output);
        case 0x34: return simpleInstruction(gb, "SWAP H", output);
        case 0x35: return simpleInstruction(gb, "SWAP L", output);
        case 0x36: return simpleInstruction(gb, "SWAP (HL)", output);
        case 0x37: return simpleInstruction(gb, "SWAP A", output);
        case 0x38: return simpleInstruction(gb, "SRL B", output);
        case 0x39: return simpleInstruction(gb, "SRL C", output);
        case 0x3A: return simpleInstruction(gb, "SRL D", output);
        case 0x3B: return simpleInstruction(gb, "SRL E", output);
        case 0x3C: return simpleInstruction(gb, "SRL H", output);
        case 0x3D: return simpleInstruction(gb, "SRL L", output);
        case 0x3E: return simpleInstruction(gb, "SRL (HL)", output);
        case 0x3F: return simpleInstruction(gb, "SRL A", output);
        case 0x40: return simpleInstruction(gb, "BIT 0, B", output);
        case 0x41: return simpleInstruction(gb, "BIT 0, C", output);
        case 0x42: return simpleInstruction(gb, "BIT 0, D", output);
        case 0x43: return simpleInstruction(gb, "BIT 0, E", output);
        case 0x44: return simpleInstruction(gb, "BIT 0, H", output);
        case 0x45: return simpleInstruction(gb, "BIT 0, L", output);
        case 0x46: return simpleInstruction(gb, "BIT 0, (HL)", output);
        case 0x47: return simpleInstruction(gb, "BIT 0, A", output);
        case 0x48: return simpleInstruction(gb, "BIT 1, B", output);
        case 0x49: return simpleInstruction(gb, "BIT 1, C", output);
        case 0x4A: return simpleInstruction(gb, "BIT 1, D", output);
        case 0x4B: return simpleInstruction(gb, "BIT 1, E", output);
        case 0x4C: return simpleInstruction(gb, "BIT 1, H", output);
        case 0x4D: return simpleInstruction(gb, "BIT 1, L", output);
        case 0x4E: return simpleInstruction(gb, "BIT 1, (HL)", output);
        case 0x4F: return simpleInstruction(gb, "BIT 1, A", output);
        case 0x50: return simpleInstruction(gb, "BIT 2, B", output);
        case 0x51: return simpleInstruction(gb, "BIT 2, C", output);
        case 0x52: return simpleInstruction(gb, "BIT 2, D", output);
        case 0x53: return simpleInstruction(gb, "BIT 2, E", output);
        case 0x54: return simpleInstruction(gb, "BIT 2, H", output);
        case 0x55: return simpleInstruction(gb, "BIT 2, L", output);
        case 0x56: return simpleInstruction(gb, "BIT 2, (HL)", output);
        case 0x57: return simpleInstruction(gb, "BIT 2, A", output);
        case 0x58: return simpleInstruction(gb, "BIT 3, B", output);
        case 0x59: return simpleInstruction(gb, "BIT 3, C", output);
        case 0x5A: return simpleInstruction(gb, "BIT 3, D", output);
        case 0x5B: return simpleInstruction(gb, "BIT 3, E", output);
        case 0x5C: return simpleInstruction(gb, "BIT 3, H", output);
        case 0x5D: return simpleInstruction(gb, "BIT 3, L", output);
        case 0x5E: return simpleInstruction(gb, "BIT 3, (HL)", output);
        case 0x5F: return simpleInstruction(gb, "BIT 3, A", output);
        case 0x60: return simpleInstruction(gb, "BIT 4, B", output);
        case 0x61: return simpleInstruction(gb, "BIT 4, C", output);
        case 0x62: return simpleInstruction(gb, "BIT 4, D", output);
        case 0x63: return simpleInstruction(gb, "BIT 4, E", output);
        case 0x64: return simpleInstruction(gb, "BIT 4, H", output);
        case 0x65: return simpleInstruction(gb, "BIT 4, L", output);
        case 0x66: return simpleInstruction(gb, "BIT 4, (HL)", output);
        case 0x67: return simpleInstruction(gb, "BIT 4, A", output);
        case 0x68: return simpleInstruction(gb, "BIT 5, B", output);
        case 0x69: return simpleInstruction(gb, "BIT 5, C", output);
        case 0x6A: return simpleInstruction(gb, "BIT 5, D", output);
        case 0x6B: return simpleInstruction(gb, "BIT 5, E", output);
        case 0x6C: return simpleInstruction(gb, "BIT 5, H", output);
        case 0x6D: return simpleInstruction(gb, "BIT 5, L", output);
        case 0x6E: return simpleInstruction(gb, "BIT 5, (HL)", output);
        case 0x6F: return simpleInstruction(gb, "BIT 5, A", output);
        case 0x70: return simpleInstruction(gb, "BIT 6, B", output);
        case 0x71: return simpleInstruction(gb, "BIT 6, C", output);
        case 0x72: return simpleInstruction(gb, "BIT 6, D", output);
        case 0x73: return simpleInstruction(gb, "BIT 6, E", output);
        case 0x74: return simpleInstruction(gb, "BIT 6, H", output);
        case 0x75: return simpleInstruction(gb, "BIT 6, L", output);
        case 0x76: return simpleInstruction(gb, "BIT 6, (HL)", output);
        case 0x77: return simpleInstruction(gb, "BIT 6, A", output);
        case 0x78: return simpleInstruction(gb, "BIT 7, B", output);
        case 0x79: return simpleInstruction(gb, "BIT 7, C", output);
        case 0x7A: return simpleInstruction(gb, "BIT 7, D", output);
        case 0x7B: return simpleInstruction(gb, "BIT 7, E", output);
        case 0x7C: return simpleInstruction(gb, "BIT 7, H", output);
        case 0x7D: return simpleInstruction(gb, "BIT 7, L", output);
        case 0x7E: return simpleInstruction(gb, "BIT 7, (HL)", output);
        case 0x7F: return simpleInstruction(gb, "BIT 7, A", output);
        case 0x80: return simpleInstruction(gb, "RES 0, B", output);
        case 0x81: return simpleInstruction(gb, "RES 0, C", output);
        case 0x82: return simpleInstruction(gb, "RES 0, D", output);
        case 0x83: return simpleInstruction(gb, "RES 0, E", output);
        case 0x84: return simpleInstruction(gb, "RES 0, H", output);
        case 0x85: return simpleInstruction(gb, "RES 0, L", output);
        case 0x86: return simpleInstruction(gb, "RES 0, (HL)", output);
        case 0x87: return simpleInstruction(gb, "RES 0, A", output);
        case 0x88: return simpleInstruction(gb, "RES 1, B", output);
        case 0x89: return simpleInstruction(gb, "RES 1, C", output);
        case 0x8A: return simpleInstruction(gb, "RES 1, D", output);
        case 0x8B: return simpleInstruction(gb, "RES 1, E", output);
        case 0x8C: return simpleInstruction(gb, "RES 1, H", output);
        case 0x8D: return simpleInstruction(gb, "RES 1, L", output);
        case 0x8E: return simpleInstruction(gb, "RES 1, (HL)", output);
        case 0x8F: return simpleInstruction(gb, "RES 1, A", output);
        case 0x90: return simpleInstruction(gb, "RES 2, B", output);
        case 0x91: return simpleInstruction(gb, "RES 2, C", output);
        case 0x92: return simpleInstruction(gb, "RES 2, D", output);
        case 0x93: return simpleInstruction(gb, "RES 2, E", output);
        case 0x94: return simpleInstruction(gb, "RES 2, H", output);
        case 0x95: return simpleInstruction(gb, "RES 2, L", output);
        case 0x96: return simpleInstruction(gb, "RES 2, (HL)", output);
        case 0x97: return simpleInstruction(gb, "RES 2, A", output);
        case 0x98: return simpleInstruction(gb, "RES 3, B", output);
        case 0x99: return simpleInstruction(gb, "RES 3, C", output);
        case 0x9A: return simpleInstruction(gb, "RES 3, D", output);
        case 0x9B: return simpleInstruction(gb, "RES 3, E", output);
        case 0x9C: return simpleInstruction(gb, "RES 3, H", output);
        case 0x9D: return simpleInstruction(gb, "RES 3, L", output);
        case 0x9E: return simpleInstruction(gb, "RES 3, (HL)", output);
        case 0x9F: return simpleInstruction(gb, "RES 3, A", output);
        case 0xA0: return simpleInstruction(gb, "RES 4, B", output);
        case 0xA1: return simpleInstruction(gb, "RES 4, C", output);
        case 0xA2: return simpleInstruction(gb, "RES 4, D", output);
        case 0xA3: return simpleInstruction(gb, "RES 4, E", output);
        case 0xA4: return simpleInstruction(gb, "RES 4, H", output);
        case 0xA5: return simpleInstruction(gb, "RES 4, L", output);
        case 0xA6: return simpleInstruction(gb, "RES 4, (HL)", output);
        case 0xA7: return simpleInstruction(gb, "RES 4, A", output);
        case 0xA8: return simpleInstruction(gb, "RES 5, B", output);
        case 0xA9: return simpleInstruction(gb, "RES 5, C", output);
        case 0xAA: return simpleInstruction(gb, "RES 5, D", output);
        case 0xAB: return simpleInstruction(gb, "RES 5, E", output);
        case 0xAC: return simpleInstruction(gb, "RES 5, H", output);
        case 0xAD: return simpleInstruction(gb, "RES 5, L", output);
        case 0xAE: return simpleInstruction(gb, "RES 5, (HL)", output);
        case 0xAF: return simpleInstruction(gb, "RES 5, A", output);
        case 0xB0: return simpleInstruction(gb, "RES 6, B", output);
        case 0xB1: return simpleInstruction(gb, "RES 6, C", output);
        case 0xB2: return simpleInstruction(gb, "RES 6, D", output);
        case 0xB3: return simpleInstruction(gb, "RES 6, E", output);
        case 0xB4: return simpleInstruction(gb, "RES 6, H", output);
        case 0xB5: return simpleInstruction(gb, "RES 6, L", output);
        case 0xB6: return simpleInstruction(gb, "RES 6, (HL)", output);
        case 0xB7: return simpleInstruction(gb, "RES 6, A", output);
        case 0xB8: return simpleInstruction(gb, "RES 7, B", output);
        case 0xB9: return simpleInstruction(gb, "RES 7, C", output);
        case 0xBA: return simpleInstruction(gb, "RES 7, D", output);
        case 0xBB: return simpleInstruction(gb, "RES 7, E", output);
        case 0xBC: return simpleInstruction(gb, "RES 7, H", output);
        case 0xBD: return simpleInstruction(gb, "RES 7, L", output);
        case 0xBE: return simpleInstruction(gb, "RES 7, (HL)", output);
        case 0xBF: return simpleInstruction(gb, "RES 7, A", output);
        case 0xC0: return simpleInstruction(gb, "SET 0, B", output);
        case 0xC1: return simpleInstruction(gb, "SET 0, C", output);
        case 0xC2: return simpleInstruction(gb, "SET 0, D", output);
        case 0xC3: return simpleInstruction(gb, "SET 0, E", output);
        case 0xC4: return simpleInstruction(gb, "SET 0, H", output);
        case 0xC5: return simpleInstruction(gb, "SET 0, L", output);
        case 0xC6: return simpleInstruction(gb, "SET 0, (HL)", output);
        case 0xC7: return simpleInstruction(gb, "SET 0, A", output);
        case 0xC8: return simpleInstruction(gb, "SET 1, B", output);
        case 0xC9: return simpleInstruction(gb, "SET 1, C", output);
        case 0xCA: return simpleInstruction(gb, "SET 1, D", output);
        case 0xCB: return simpleInstruction(gb, "SET 1, E", output);
        case 0xCC: return simpleInstruction(gb, "SET 1, H", output);
        case 0xCD: return simpleInstruction(gb, "SET 1, L", output);
        case 0xCE: return simpleInstruction(gb, "SET 1, (HL)", output);
        case 0xCF: return simpleInstruction(gb, "SET 1, A", output);
        case 0xD0: return simpleInstruction(gb, "SET 2, B", output);
        case 0xD1: return simpleInstruction(gb, "SET 2, C", output);
        case 0xD2: return simpleInstruction(gb, "SET 2, D", output);
        case 0xD3: return simpleInstruction(gb, "SET 2, E", output);
        case 0xD4: return simpleInstruction(gb, "SET 2, H", output);
        case 0xD5: return simpleInstruction(gb, "SET 2, L", output);
        case 0xD6: return simpleInstruction(gb, "SET 2, (HL)", output);
        case 0xD7: return simpleInstruction(gb, "SET 2, A", output);
        case 0xD8: return simpleInstruction(gb, "SET 3, B", output);
        case 0xD9: return simpleInstruction(gb, "SET 3, C", output);
        case 0xDA: return simpleInstruction(gb, "SET 3, D", output);
        case 0xDB: return simpleInstruction(gb, "SET 3, E", output);
        case 0xDC: return simpleInstruction(gb, "SET 3, H", output);
        case 0xDD: return simpleInstruction(gb, "SET 3, L", output);
        case 0xDE: return simpleInstruction(gb, "SET 3, (HL)", output);
        case 0xDF: return simpleInstruction(gb, "SET 3, A", output);
        case 0xE0: return simpleInstruction(gb, "SET 4, B", output);
        case 0xE1: return simpleInstruction(gb, "SET 4, C", output);
        case 0xE2: return simpleInstruction(gb, "SET 4, D", output);
        case 0xE3: return simpleInstruction(gb, "SET 4, E", output);
        case 0xE4: return simpleInstruction(gb, "SET 4, H", output);
        case 0xE5: return simpleInstruction(gb, "SET 4, L", output);
        case 0xE6: return simpleInstruction(gb, "SET 4, (HL)", output);
        case 0xE7: return simpleInstruction(gb, "SET 4, A", output);
        case 0xE8: return simpleInstruction(gb, "SET 5, B", output);
        case 0xE9: return simpleInstruction(gb, "SET 5, C", output);
        case 0xEA: return simpleInstruction(gb, "SET 5, D", output);
        case 0xEB: return simpleInstruction(gb, "SET 5, E", output);
        case 0xEC: return simpleInstruction(gb, "SET 5, H", output);
        case 0xED: return simpleInstruction(gb, "SET 5, L", output);
        case 0xEE: return simpleInstruction(gb, "SET 5, (HL)", output);
        case 0xEF: return simpleInstruction(gb, "SET 5, A", output);
        case 0xF0: return simpleInstruction(gb, "SET 6, B", output);
        case 0xF1: return simpleInstruction(gb, "SET 6, C", output);
        case 0xF2: return simpleInstruction(gb, "SET 6, D", output);
        case 0xF3: return simpleInstruction(gb, "SET 6, E", output);
        case 0xF4: return simpleInstruction(gb, "SET 6, H", output);
        case 0xF5: return simpleInstruction(gb, "SET 6, L", output);
        case 0xF6: return simpleInstruction(gb, "SET 6, (HL)", output);
        case 0xF7: return simpleInstruction(gb, "SET 6, A", output);
        case 0xF8: return simpleInstruction(gb, "SET 7, B", output);
        case 0xF9: return simpleInstruction(gb, "SET 7, C", output);
        case 0xFA: return simpleInstruction(gb, "SET 7, D", output);
        case 0xFB: return simpleInstruction(gb, "SET 7, E", output);
        case 0xFC: return simpleInstruction(gb, "SET 7, H", output);
        case 0xFD: return simpleInstruction(gb, "SET 7, L", output);
        case 0xFE: return simpleInstruction(gb, "SET 7, (HL)", output);
        case 0xFF: return simpleInstruction(gb, "SET 7, A", output);
    }
}

void disassembleInstruction(GB* gb, uint8_t byte, char* output) {
	switch (byte) {
        case 0x00: return simpleInstruction(gb, "NOP", output);
        case 0x01: return d16(gb, "LD BC, 0x%04x", output);
        case 0x02: return simpleInstruction(gb, "LD (BC), A", output);
        case 0x03: return simpleInstruction(gb, "INC BC", output);
        case 0x04: return simpleInstruction(gb, "INC B", output);
        case 0x05: return simpleInstruction(gb, "DEC B", output);
        case 0x06: return d8(gb, "LD B, 0x%02x", output);
        case 0x07: return simpleInstruction(gb, "RLCA", output);
        case 0x08: return a16(gb, "LD 0x%04x, SP", output);
        case 0x09: return simpleInstruction(gb, "ADD HL, BC", output);
        case 0x0A: return simpleInstruction(gb, "LD A, (BC)", output);
        case 0x0B: return simpleInstruction(gb, "DEC BC", output);
        case 0x0C: return simpleInstruction(gb, "INC C", output);
        case 0x0D: return simpleInstruction(gb, "DEC C", output);
        case 0x0E: return d8(gb, "LD C, 0x%02x", output);
        case 0x0F: return simpleInstruction(gb, "RRCA", output);
        case 0x10: return simpleInstruction(gb, "STOP", output);
        case 0x11: return d16(gb, "LD DE, 0x%04x", output);
        case 0x12: return simpleInstruction(gb, "LD (DE), A", output);
        case 0x13: return simpleInstruction(gb, "INC DE", output);
        case 0x14: return simpleInstruction(gb, "INC D", output);
        case 0x15: return simpleInstruction(gb, "DEC D", output);
        case 0x16: return d8(gb, "LD D, 0x%02x", output);
        case 0x17: return simpleInstruction(gb, "RLA", output);
        case 0x18: return r8(gb, "JR %d", output);
        case 0x19: return simpleInstruction(gb, "ADD HL, DE", output);
        case 0x1A: return simpleInstruction(gb, "LD A, (DE)", output);
        case 0x1B: return simpleInstruction(gb, "DEC DE", output);
        case 0x1C: return simpleInstruction(gb, "INC E", output);
        case 0x1D: return simpleInstruction(gb, "DEC E", output);
        case 0x1E: return d8(gb, "LD E, 0x%02x", output);
        case 0x1F: return simpleInstruction(gb, "RRA", output);
        case 0x20: return r8(gb, "JR NZ, %d", output);
        case 0x21: return d16(gb, "LD HL, 0x%04x", output);
        case 0x22: return simpleInstruction(gb, "LD (HL+), A", output);
        case 0x23: return simpleInstruction(gb, "INC HL", output);
        case 0x24: return simpleInstruction(gb, "INC H", output);
        case 0x25: return simpleInstruction(gb, "DEC H", output);
        case 0x26: return d8(gb, "LD H, 0x%02x", output);
        case 0x27: return simpleInstruction(gb, "DAA", output);
        case 0x28: return r8(gb, "JR Z, %d", output);
        case 0x29: return simpleInstruction(gb, "ADD HL, HL", output);
        case 0x2A: return simpleInstruction(gb, "LD A, (HL+)", output);
        case 0x2B: return simpleInstruction(gb, "DEC HL", output);
        case 0x2C: return simpleInstruction(gb, "INC L", output);
        case 0x2D: return simpleInstruction(gb, "DEC L", output);
        case 0x2E: return d8(gb, "LD L, 0x%02x", output);
        case 0x2F: return simpleInstruction(gb, "CPL", output);
        case 0x30: return r8(gb, "JR NC, %d", output);
        case 0x31: return d16(gb, "LD SP, %0x%04x", output);
        case 0x32: return simpleInstruction(gb, "LD (HL-), A", output);
        case 0x33: return simpleInstruction(gb, "INC SP", output);
        case 0x34: return simpleInstruction(gb, "INC (HL)", output);
        case 0x35: return simpleInstruction(gb, "DEC (HL)", output);
        case 0x36: return d8(gb, "LD (HL), 0x%02x", output);
        case 0x37: return simpleInstruction(gb, "SCF", output);
        case 0x38: return r8(gb, "JR C, %d", output);
        case 0x39: return simpleInstruction(gb, "ADD HL, SP", output);
        case 0x3A: return simpleInstruction(gb, "LD A, (HL-)", output);
        case 0x3B: return simpleInstruction(gb, "DEC SP", output);
        case 0x3C: return simpleInstruction(gb, "INC A", output);
        case 0x3D: return simpleInstruction(gb, "DEC A", output);
        case 0x3E: return d8(gb, "LD A, 0x%02x", output);
        case 0x3F: return simpleInstruction(gb, "CCF", output);
        case 0x40: return simpleInstruction(gb, "LD B, B", output);
        case 0x41: return simpleInstruction(gb, "LD B, C", output);
        case 0x42: return simpleInstruction(gb, "LD B, D", output);
        case 0x43: return simpleInstruction(gb, "LD B, E", output);
        case 0x44: return simpleInstruction(gb, "LD B, H", output);
        case 0x45: return simpleInstruction(gb, "LD B, L", output);
        case 0x46: return simpleInstruction(gb, "LD B, (HL)", output);
        case 0x47: return simpleInstruction(gb, "LD B, A", output);
        case 0x48: return simpleInstruction(gb, "LD C, B", output);
        case 0x49: return simpleInstruction(gb, "LD C, C", output);
        case 0x4A: return simpleInstruction(gb, "LD C, D", output);
        case 0x4B: return simpleInstruction(gb, "LD C, E", output);
        case 0x4C: return simpleInstruction(gb, "LD C, H", output);
        case 0x4D: return simpleInstruction(gb, "LD C, L", output);
        case 0x4E: return simpleInstruction(gb, "LD C, (HL)", output);
        case 0x4F: return simpleInstruction(gb, "LD C, A", output);
        case 0x50: return simpleInstruction(gb, "LD D, B", output);
        case 0x51: return simpleInstruction(gb, "LD D, C", output);
        case 0x52: return simpleInstruction(gb, "LD D, D", output);
        case 0x53: return simpleInstruction(gb, "LD D, E", output);
        case 0x54: return simpleInstruction(gb, "LD D, H", output);
        case 0x55: return simpleInstruction(gb, "LD D, L", output);
        case 0x56: return simpleInstruction(gb, "LD D, (HL)", output);
        case 0x57: return simpleInstruction(gb, "LD D, A", output);
        case 0x58: return simpleInstruction(gb, "LD E, B", output);
        case 0x59: return simpleInstruction(gb, "LD E, C", output);
        case 0x5A: return simpleInstruction(gb, "LD E, D", output);
        case 0x5B: return simpleInstruction(gb, "LD E, E", output);
        case 0x5C: return simpleInstruction(gb, "LD E, H", output);
        case 0x5D: return simpleInstruction(gb, "LD E, L", output);
        case 0x5E: return simpleInstruction(gb, "LD E, (HL)", output);
        case 0x5F: return simpleInstruction(gb, "LD E, A", output);
        case 0x60: return simpleInstruction(gb, "LD H, B", output);
        case 0x61: return simpleInstruction(gb, "LD H, C", output);
        case 0x62: return simpleInstruction(gb, "LD H, D", output);
        case 0x63: return simpleInstruction(gb, "LD H, E", output);
        case 0x64: return simpleInstruction(gb, "LD H, H", output);
        case 0x65: return simpleInstruction(gb, "LD H, L", output);
        case 0x66: return simpleInstruction(gb, "LD H, (HL)", output);
        case 0x67: return simpleInstruction(gb, "LD H, A", output);
        case 0x68: return simpleInstruction(gb, "LD L, B", output);
        case 0x69: return simpleInstruction(gb, "LD L, C", output);
        case 0x6A: return simpleInstruction(gb, "LD L, D", output);
        case 0x6B: return simpleInstruction(gb, "LD L, E", output);
        case 0x6C: return simpleInstruction(gb, "LD L, H", output);
        case 0x6D: return simpleInstruction(gb, "LD L, L", output);
        case 0x6E: return simpleInstruction(gb, "LD L, (HL)", output);
        case 0x6F: return simpleInstruction(gb, "LD L, A", output);
        case 0x70: return simpleInstruction(gb, "LD (HL), B", output);
        case 0x71: return simpleInstruction(gb, "LD (HL), C", output);
        case 0x72: return simpleInstruction(gb, "LD (HL), D", output);
        case 0x73: return simpleInstruction(gb, "LD (HL), E", output);
        case 0x74: return simpleInstruction(gb, "LD (HL), H", output);
        case 0x75: return simpleInstruction(gb, "LD (HL), L", output);
        case 0x76: return simpleInstruction(gb, "HALT", output);
        case 0x77: return simpleInstruction(gb, "LD (HL), A", output);
        case 0x78: return simpleInstruction(gb, "LD A, B", output);
        case 0x79: return simpleInstruction(gb, "LD A, C", output);
        case 0x7A: return simpleInstruction(gb, "LD A, D", output);
        case 0x7B: return simpleInstruction(gb, "LD A, E", output);
        case 0x7C: return simpleInstruction(gb, "LD A, H", output);
        case 0x7D: return simpleInstruction(gb, "LD A, L", output);
        case 0x7E: return simpleInstruction(gb, "LD A, (HL)", output);
        case 0x7F: return simpleInstruction(gb, "LD A, A", output);
        case 0x80: return simpleInstruction(gb, "ADD A, B", output);
        case 0x81: return simpleInstruction(gb, "ADD A, C", output);
        case 0x82: return simpleInstruction(gb, "ADD A, D", output);
        case 0x83: return simpleInstruction(gb, "ADD A, E", output);
        case 0x84: return simpleInstruction(gb, "ADD A, H", output);
        case 0x85: return simpleInstruction(gb, "ADD A, L", output);
        case 0x86: return simpleInstruction(gb, "ADD A, (HL)", output);
        case 0x87: return simpleInstruction(gb, "ADD A, A", output);
        case 0x88: return simpleInstruction(gb, "ADC A, B", output);
        case 0x89: return simpleInstruction(gb, "ADC A, C", output);
        case 0x8A: return simpleInstruction(gb, "ADC A, D", output);
        case 0x8B: return simpleInstruction(gb, "ADC A, E", output);
        case 0x8C: return simpleInstruction(gb, "ADC A, H", output);
        case 0x8D: return simpleInstruction(gb, "ADC A, L", output);
        case 0x8E: return simpleInstruction(gb, "ADC A, (HL)", output);
        case 0x8F: return simpleInstruction(gb, "ADC A, A", output);
        case 0x90: return simpleInstruction(gb, "SUB B", output);
        case 0x91: return simpleInstruction(gb, "SUB C", output);
        case 0x92: return simpleInstruction(gb, "SUB D", output);
        case 0x93: return simpleInstruction(gb, "SUB E", output);
        case 0x94: return simpleInstruction(gb, "SUB H", output);
        case 0x95: return simpleInstruction(gb, "SUB L", output);
        case 0x96: return simpleInstruction(gb, "SUB (HL)", output);
        case 0x97: return simpleInstruction(gb, "SUB A", output);
        case 0x98: return simpleInstruction(gb, "SBC A, B", output);
        case 0x99: return simpleInstruction(gb, "SBC A, C", output);
        case 0x9A: return simpleInstruction(gb, "SBC A, D", output);
        case 0x9B: return simpleInstruction(gb, "SBC A, E", output);
        case 0x9C: return simpleInstruction(gb, "SBC A, H", output);
        case 0x9D: return simpleInstruction(gb, "SBC A, L", output);
        case 0x9E: return simpleInstruction(gb, "SBC A, (HL)", output);
        case 0x9F: return simpleInstruction(gb, "SBC A, A", output);
        case 0xA0: return simpleInstruction(gb, "AND B", output);
        case 0xA1: return simpleInstruction(gb, "AND C", output);
        case 0xA2: return simpleInstruction(gb, "AND D", output);
        case 0xA3: return simpleInstruction(gb, "AND E", output);
        case 0xA4: return simpleInstruction(gb, "AND H", output);
        case 0xA5: return simpleInstruction(gb, "AND L", output);
        case 0xA6: return simpleInstruction(gb, "AND (HL)", output);
        case 0xA7: return simpleInstruction(gb, "AND A", output);
        case 0xA8: return simpleInstruction(gb, "XOR B", output);
        case 0xA9: return simpleInstruction(gb, "XOR C", output);
        case 0xAA: return simpleInstruction(gb, "XOR D", output);
        case 0xAB: return simpleInstruction(gb, "XOR E", output);
        case 0xAC: return simpleInstruction(gb, "XOR H", output);
        case 0xAD: return simpleInstruction(gb, "XOR L", output);
        case 0xAE: return simpleInstruction(gb, "XOR (HL)", output);
        case 0xAF: return simpleInstruction(gb, "XOR A", output);
        case 0xB0: return simpleInstruction(gb, "OR B", output);
        case 0xB1: return simpleInstruction(gb, "OR C", output);
        case 0xB2: return simpleInstruction(gb, "OR D", output);
        case 0xB3: return simpleInstruction(gb, "OR E", output);
        case 0xB4: return simpleInstruction(gb, "OR H", output);
        case 0xB5: return simpleInstruction(gb, "OR L", output);
        case 0xB6: return simpleInstruction(gb, "OR (HL)", output);
        case 0xB7: return simpleInstruction(gb, "OR A", output);
        case 0xB8: return simpleInstruction(gb, "CP B", output);
        case 0xB9: return simpleInstruction(gb, "CP C", output);
        case 0xBA: return simpleInstruction(gb, "CP D", output);
        case 0xBB: return simpleInstruction(gb, "CP E", output);
        case 0xBC: return simpleInstruction(gb, "CP H", output);
        case 0xBD: return simpleInstruction(gb, "CP L", output);
        case 0xBE: return simpleInstruction(gb, "CP (HL)", output);
        case 0xBF: return simpleInstruction(gb, "CP A", output);
        case 0xC0: return simpleInstruction(gb, "RET NZ", output);
        case 0xC1: return simpleInstruction(gb, "POP BC", output);
        case 0xC2: return a16(gb, "JP NZ, 0x%04x", output);
        case 0xC3: return a16(gb, "JP 0x%04x", output);
        case 0xC4: return a16(gb, "CALL NZ, 0x%04x", output);
        case 0xC5: return simpleInstruction(gb, "PUSH BC", output);
        case 0xC6: return d8(gb, "ADD A, 0x%02x", output);
        case 0xC7: return simpleInstruction(gb, "RST 0x00", output);
        case 0xC8: return simpleInstruction(gb, "RET Z", output);
        case 0xC9: return simpleInstruction(gb, "RET", output);
        case 0xCA: return a16(gb, "JP Z, 0x%04x", output);
        case 0xCB: return simpleInstruction(gb, "PREFIX CB", output);
        case 0xCC: return a16(gb, "CALL Z, 0x%04x", output);
        case 0xCD: return a16(gb, "CALL 0x%04x", output);
        case 0xCE: return d8(gb, "ADC A, 0x%02x", output);
        case 0xCF: return simpleInstruction(gb, "RST 0x08", output);
        case 0xD0: return simpleInstruction(gb, "RET NC", output);
        case 0xD1: return simpleInstruction(gb, "POP DE", output);
        case 0xD2: return a16(gb, "JP NC, 0x%04x", output);
        case 0xD4: return a16(gb, "CALL NC, 0x%04x", output);
        case 0xD5: return simpleInstruction(gb, "PUSH DE", output);
        case 0xD6: return d8(gb, "SUB 0x%02x", output);
        case 0xD7: return simpleInstruction(gb, "RST 0x10", output);
        case 0xD8: return simpleInstruction(gb, "REC C", output);
        case 0xD9: return simpleInstruction(gb, "RETI", output);
        case 0xDA: return a16(gb, "JP C, 0x%04x", output);
        case 0xDC: return a16(gb, "CALL C, 0x%04x", output);
        case 0xDE: return d8(gb, "SBC A, 0x%02x", output);
        case 0xDF: return simpleInstruction(gb, "RST 0x18", output);
        case 0xE0: return d8(gb, "LD (0xFF%02x), A", output);
        case 0xE1: return simpleInstruction(gb, "POP HL", output);
        case 0xE2: return simpleInstruction(gb, "LD (0xFF00+C), A", output);
        case 0xE5: return simpleInstruction(gb, "PUSH HL", output);
        case 0xE6: return d8(gb, "AND 0x%02x", output);
        case 0xE7: return simpleInstruction(gb, "RST 0x20", output);
        case 0xE8: return r8(gb, "ADD SP, %d", output);
        case 0xE9: return simpleInstruction(gb, "JP (HL)", output);
        case 0xEA: return a16(gb, "LD (0x%04x), A", output);
        case 0xEE: return d8(gb, "XOR 0x%02x", output);
        case 0xEF: return simpleInstruction(gb, "RST 0x28", output);
        case 0xF0: return d8(gb, "LD A, (0xFF%02x)", output);
        case 0xF1: return simpleInstruction(gb, "POP AF", output);
        case 0xF2: return simpleInstruction(gb, "LD A, (0xFF00 + C)", output);
        case 0xF3: return simpleInstruction(gb, "DI", output);
        case 0xF5: return simpleInstruction(gb, "PUSH AF", output);
        case 0xF6: return d8(gb, "OR 0x%02x", output);
        case 0xF7: return simpleInstruction(gb, "RST 0x30", output);
        case 0xF8: return r8(gb, "LD HL, SP+%d", output);
        case 0xF9: return simpleInstruction(gb, "LD SP, HL", output);
        case 0xFA: return a16(gb, "LD A, (0x%04x)", output);
        case 0xFB: return simpleInstruction(gb, "EI", output);
        case 0xFE: return d8(gb, "CP 0x%02x", output);
        case 0xFF: return simpleInstruction(gb, "RST 0x38", output);
        default: return simpleInstruction(gb, "????", output);
    }
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
    printf("[sel:%x|", (gb->IO[R_P1_JOYP] >> 4) & 0x3);
    printf("sig:%x]", (gb->IO[R_P1_JOYP] & 0b00001111));
#endif
#ifdef DEBUG_PRINT_TIMERS
    printf("[%x|%x|%x|%x]", gb->IO[R_DIV], gb->IO[R_TIMA], gb->IO[R_TMA], gb->IO[R_TAC]);
#endif
    printf(" %5s", "");
#ifdef DEBUG_PRINT_OPCODE
	printf("0x%02x ", byte);
#endif
   
	char disasm[30];
	disassembleCBInstruction(gb, byte, (char*)&disasm);
	printf("%s\n", disasm);
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
    printf("[sel:%x|", (gb->IO[R_P1_JOYP] >> 4) & 0x3);
    printf("sig:%x]", (gb->IO[R_P1_JOYP] & 0b00001111));
#endif
#ifdef DEBUG_PRINT_TIMERS
    printf("[%x|%x|%x|%x]", gb->IO[R_DIV], gb->IO[R_TIMA], gb->IO[R_TMA], gb->IO[R_TAC]);
#endif
    printf(" %5s", "");
#ifdef DEBUG_PRINT_OPCODE
	printf("0x%02x ", readAddr(gb, gb->PC));
#endif
    char disasm[30];
	disassembleInstruction(gb, readAddr(gb, gb->PC), (char*)&disasm);
	printf("%s\n", disasm);
}

void printRegisters(GB* gb) {
    printf("[A%02x|B%02x|C%02x|D%02x|E%02x|H%02x|L%02x|SP%04x]\n",
            gb->GPR[R8_A], gb->GPR[R8_B], gb->GPR[R8_C],
            gb->GPR[R8_D], gb->GPR[R8_E], gb->GPR[R8_H],
            gb->GPR[R8_L], (uint16_t)(gb->GPR[R8_SP_HIGH] << 8) + gb->GPR[R8_SP_LOW]);
}
