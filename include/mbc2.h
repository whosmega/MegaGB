#ifndef megagbc_mbc2_h
#define megagbc_mbc2_h
#include "../include/vm.h"

typedef struct {
    uint8_t builtInRAM[0x200];              /* Built in 512x4 bit RAM */

    bool ramEnabled;
} MBC_2;

void mbc2_allocate(VM* vm);
void mbc2_writeBuiltInRAM(VM* vm, uint16_t addr, uint8_t byte);
uint8_t mbc2_readBuiltInRAM(VM* vm, uint16_t addr);
void mbc2_free(VM* vm);
void mbc2_interceptROMWrite(VM* vm, uint16_t addr, uint8_t byte);

#endif
