#include "../include/mbc.h"
#include "../include/gb.h"
#include "../include/mbc1.h"
#include "../include/mbc2.h"
#include "../include/debug.h"
#include <stddef.h>
#include <stdint.h>

/* Bank Switching */
void switchROMBank(GB* gb, int bankNumber) {
    /* This function only does the switching part, the checking
     * and decoding is done by MBCs separately */

    uint8_t* allocated = gb->cartridge->allocated;
    uint8_t* bank = &allocated[bankNumber * 0x4000];    /* Size of each bank is 16 KiB */

    memcpy(&gb->MEM[ROM_NN_16KB], bank, 0x4000);             /* Write the contents of the bank */

#ifdef DEBUG_LOGGING
    printf("MBC : Switched ROM Bank to 0x%x\n", bankNumber);
#endif
}

/* This function switches the restricted ROM bank which is normally
 * considered 'unbankable', but it needs to be remapped in special
 * cases */

void switchRestrictedROMBank(GB* gb, int bankNumber) {
    memcpy(&gb->MEM[ROM_N0_16KB], gb->cartridge->allocated, 0x4000);
}

void mbc_allocate(GB* gb) {
    /* Detect the correct MBC that needs to be used and allocate it */
    CARTRIDGE_TYPE type = gb->cartridge->cType;
    switch (type) {
        case CARTRIDGE_NONE: break;         /* No MBC */
        case CARTRIDGE_MBC1: mbc1_allocate(gb, false); break;
        case CARTRIDGE_MBC1_RAM:
        case CARTRIDGE_MBC1_RAM_BATTERY: mbc1_allocate(gb, true); break;
        case CARTRIDGE_MBC2:
        case CARTRIDGE_MBC2_BATTERY: mbc2_allocate(gb); break;
        default: log_fatal(gb, "MBC/External Hardware Not Supported"); break;
    }
}

void mbc_free(GB* gb) {
    switch (gb->memControllerType) {
        case MBC_NONE: break;
        case MBC_TYPE_1: mbc1_free(gb); break;
        case MBC_TYPE_2: mbc2_free(gb); break;
        default: break;
    }
}

void mbc_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte) {
    /* The address has already been identified as an external ram address
     * so we dont have to check */
    switch (gb->memControllerType) {
        case MBC_NONE: log_warning(gb, "Attempt to write to external RAM without MBC"); break;
        case MBC_TYPE_1: mbc1_writeExternalRAM(gb, addr, byte); break;
        case MBC_TYPE_2: mbc2_writeBuiltInRAM(gb, addr, byte); break;
        default: break;
    }
}

uint8_t mbc_readExternalRAM(GB* gb, uint16_t addr) {
    switch (gb->memControllerType) {
        case MBC_NONE: log_warning(gb, "Attempt to read from external RAM without MBC"); break;
        case MBC_TYPE_1: return mbc1_readExternalRAM(gb, addr);
        case MBC_TYPE_2: return mbc2_readBuiltInRAM(gb, addr);
        default: return 0xFF;
    }

    return 0xFF;
}

void mbc_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte) {
    switch (gb->memControllerType) {
        case MBC_NONE:
            log_warning(gb, "No MBC exists and write to ROM address doesn't make sense"); break;

        case MBC_TYPE_1: mbc1_interceptROMWrite(gb, addr, byte); break;
        case MBC_TYPE_2: mbc2_interceptROMWrite(gb, addr, byte); break;
        default: break;
    }

}
