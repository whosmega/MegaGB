#ifndef gb_mbc3_h
#define gb_mbc3_h
#include <gb/gb.h>

typedef struct {
    /* External RAM Bank Storage
     *
     * MBC3 supports upto 4 8kib banks for RAM */
    uint8_t* ramBanks; 

    /* Registers */
    bool ram_rtcEnabled;         /* RAM/RTC Enable */
    uint8_t ram_rtcBankNumber;   /* Used to store bits of ram bank number or RTC Register mapping 
                                    This can also be read to determine whether RTC r/w is selected 
                                    or RAM bank as a value of 0x00-0x03 maps to ram banks and 
                                    0x08-0x0C maps to RTC registers */
    uint8_t latchRegister;

    /* Internal State */
    bool rtcSupported;
    uint8_t selectedRTCRegister;
    uint8_t selectedROMBank;
    uint8_t selectedRAMBank; 
    
    /* RTC Registers */
} MBC_3;

void mbc3_allocate(GB* gb, bool externalRam, bool rtc);
uint8_t mbc3_readROM_N0(GB* gb, uint16_t addr);
uint8_t mbc3_readROM_NN(GB* gb, uint16_t addr);
void mbc3_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte);
uint8_t mbc3_readExternalRAM(GB* gb, uint16_t addr);
void mbc3_free(GB* gb);
void mbc3_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte);

#endif
