#ifndef gb_mbc5_h
#define gb_mbc5_h
#include <gb/gb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* External RAM Bank Storage
     *
     * MBC5 supports 8Kib/32Kib/128Kib of RAM */
    uint8_t* ramBanks;

	/* Registers */
    uint8_t selectedROMBank;
    uint8_t selectedRAMBank; 
    bool ramEnabled; 
} MBC_5;

void mbc5_allocate(GB* gb, bool externalRam);
uint8_t mbc5_readROM_N0(GB* gb, uint16_t addr);
uint8_t mbc5_readROM_NN(GB* gb, uint16_t addr);
void mbc5_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte);
uint8_t mbc5_readExternalRAM(GB* gb, uint16_t addr);
void mbc5_free(GB* gb);
void mbc5_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif
