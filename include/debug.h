#ifndef MEGAGBC_DEBUG_H
#define MEGAGBC_DEBUG_H
#include "../include/vm.h"

// #define NO_CARTRIDGE_VERIFICATION
#define REALTIME_PRINTING
// #define PRINT_CARTRIDGE_INFO

void printInstruction(VM* vm);

#endif
