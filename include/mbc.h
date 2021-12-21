#ifndef megagbc_mbc_h
#define megagbc_mbc_h
#include <stdbool.h>
#include <stdint.h>
#include "../include/cartridge.h"

/* Forward Declare VM instead of including vm.h 
 * to avoid a circular include */

struct VM;

typedef enum {
    BANK_MODE_ROM,
    BANK_MODE_RAM
} BANK_MODE;

typedef enum {
    MBC_NONE,
    MBC_TYPE_1,
    MBC_TYPE_2,
    MBC_TYPE_3,
    MBC_TYPE_5,
    MBC_TYPE_6,
    MBC_TYPE_7
} MBC_TYPE;

void mbc_allocate(struct VM* vm);
void mbc_free(struct VM* vm);
void mbc_writeExternalRAM(struct VM* vm, uint16_t addr, uint8_t byte);
uint8_t mbc_readExternalRAM(struct VM* vm, uint16_t addr);
void mbc_interceptROMWrite(struct VM* vm, uint16_t addr, uint8_t byte);
void switchROMBank(struct VM* vm, int bankNumber);
void switchRestrictedROMBank(struct VM* vm, int bankNumber);


#endif
