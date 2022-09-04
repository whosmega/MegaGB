#ifndef megagbc_mbc1_h
#define megagbc_mbc1_h
#include "../include/gb.h"

typedef struct {
    /* External RAM Bank Storage
     *
     * MBC1 supports upto 4 8kib banks for RAM */
    uint8_t* ramBanks; 

    /* Registers */
    bool ramEnabled;
    uint8_t romBankNumber;       /* Used to store lower 5 bits of bank number */
    uint8_t secondaryBankNumber; /* Used to store upper 2 bits of rom bank number or
                                    ram bank number */

    uint8_t selectedROM0Bank;
    uint8_t selectedROMBank;
    uint8_t selectedRAMBank;
    BANK_MODE bankMode;          /* Banking Mode */
} MBC_1;

void mbc1_allocate(GB* gb, bool externalRam);
uint8_t mbc1_readROM_N0(GB* gb, uint16_t addr);
uint8_t mbc1_readROM_NN(GB* gb, uint16_t addr);
void mbc1_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte);
uint8_t mbc1_readExternalRAM(GB* gb, uint16_t addr);
void mbc1_free(GB* gb);
void mbc1_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte);

#endif
