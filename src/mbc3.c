#include "../include/debug.h"
#include "../include/mbc.h"
#include "../include/mbc3.h"
#include <time.h>

void mbc3_allocate(GB* gb, bool externalRam, bool rtc) {
    MBC_3* mbc = (MBC_3*)malloc(sizeof(MBC_3));

    if (mbc == NULL) {
        log_fatal(gb, "Error while allocating memory for MBC3\n");
        return;
    }

    mbc->isLatched = false;
    mbc->latchRegister = 0x1; 
    mbc->ram_rtcBankNumber = 0;
    mbc->romBankNumber = 0;
    mbc->ram_rtcEnabled = false;
    
    mbc->selectedRTCRegister = 0;
    mbc->selectedRAMBank = 0;
    mbc->selectedROMBank = 1;

    mbc->ramBanks = NULL;
    mbc->rtcSupported = false;
    /* Initialise all RTC Registers to 0 */
    mbc->rtc = (RTC){0,0,0,0,0};
    mbc->rtc_latched = mbc->rtc;
   
    if (externalRam) {
        switch (gb->cartridge->extRamSize) {
            case EXT_RAM_0: break;
            case EXT_RAM_8KB: mbc->ramBanks = (uint8_t*)malloc(0x2000 * 1); break;
            case EXT_RAM_32KB: mbc->ramBanks = (uint8_t*)malloc(0x2000 * 4); break;
            default: log_fatal(gb, "External RAM Banks not supported with MBC3\n");
        }
    }

    if (rtc) {
        /* The counting will happen when this flag is set */
        mbc->rtcSupported = true;
        mbc->rtcLastSync = time(NULL);      /* RTC Starts ticking now */
    }

    gb->memController = (void*)mbc;
    gb->memControllerType = MBC_TYPE_3;
}

void mbc3_free(GB* gb) {
    MBC_3* mbc = (MBC_3*)gb->memController;

    if (mbc->ramBanks != NULL) {
        free(mbc->ramBanks);
        mbc->ramBanks = NULL;
    }

    free(mbc);

    gb->memController = NULL;
}

void mbc3_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte) {
    MBC_3* mbc = (MBC_3*)gb->memController;

    /* MBC Registers */

    if (addr >= 0x0000 && addr <= 0x1FFF) {
        /* RAM and RTC reading and writing enable/disable */
        if ((byte & 0xF) == 0xA) mbc->ram_rtcEnabled = true;
        else mbc->ram_rtcEnabled = false;
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {

    } else if (addr >= 0x4000 && addr <= 0x5FFF) {

    } else if (addr >= 0x6000 && addr <= 0x7FFF) {

    } else if (addr >= 0xA000 && addr <= 0xBFFF) {

    }
}

uint8_t mbc3_readExternalRAM(GB* gb, uint16_t addr) {
    return 0xFF;
}

void mbc3_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte) {

}

uint8_t mbc3_readROM_N0(GB* gb, uint16_t addr) {
    return 0xFF;
}

uint8_t mbc3_readROM_NN(GB* gb, uint16_t addr) {
    return 0xFF;
}
