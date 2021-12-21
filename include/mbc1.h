#ifndef megagbc_mbc1_h
#define megagbc_mbc1_h
#include "../include/vm.h"

typedef struct {
    /* External RAM Bank Storage 
     *
     * MBC1 supports 4 8kib banks for RAM */
    uint8_t* ramBanks;
    
    /* Note : Selected RAM/ROM banks are kept in their 
     * associated memory locations in the VM Memory.
     * The cartridge structure located in VM contains 
     * the entire rom as a continuous array and the MBC maps
     * the correct bank from it */

    /* Registers */
    bool ramEnabled;
    uint8_t romBankNumber;       /* Used to store lower 5 bits of bank number */
    uint8_t secondaryBankNumber; /* Used to store upper 2 bits of rom bank number or 
                                    ram bank number */
    BANK_MODE bankMode;          /* Banking Mode */
} MBC_1;

void mbc1_allocate(VM* vm, bool externalRam);
void mbc1_writeExternalRAM(VM* vm, uint16_t addr, uint8_t byte);
uint8_t mbc1_readExternalRAM(VM* vm, uint16_t addr);
void mbc1_free(VM* vm);
void mbc1_interceptROMWrite(VM* vm, uint16_t addr, uint8_t byte);

#endif
