#ifndef MEGAGBC_DEBUG_H
#define MEGAGBC_DEBUG_H
#include "../include/vm.h"

// #define DEBUG_NO_CARTRIDGE_VERIFICATION
// #define DEBUG_REALTIME_PRINTING
// #define DEBUG_PRINT_CARTRIDGE_INFO
// #define DEBUG_LOGGING
// #define DEBUG_PRINT_REGISTERS
#define DEBUG_PRINT_SERIAL_OUTPUT

/* Stops running when encounters opcode 0x40, LD B, B */
// #define DEBUG_LDBB_BREAKPOINT

void printInstruction(VM* vm);
void printRegisters(VM* vm);
void printCBInstruction(VM* vm, uint8_t byte);
void log_fatal(VM* vm, const char* string);
void log_warning(VM* vm, const char* string);
#endif
