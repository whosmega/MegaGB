#include "gb/cartridge.h"
#include <gb/mbc.h>
#include <gb/gb.h>
#include <gb/mbc1.h>
#include <gb/mbc2.h>
#include <gb/mbc3.h>
#include <gb/mbc5.h>
#include <gb/debug.h>
#include <stddef.h>
#include <stdint.h>

/* Bank Switching */
void switchROMBank(GB* gb, int bankNumber) {
    /* This function only does the switching part, the checking
     * and decoding is done by MBCs separately */

    uint8_t* allocated = gb->cartridge->allocated;
    uint8_t* bank = &allocated[bankNumber * 0x4000];    /* Size of each bank is 16 KiB */

#ifdef DEBUG_LOGGING
    printf("MBC : Switched ROM Bank to 0x%x\n", bankNumber);
#endif
}

void mbc_allocate(GB* gb) {
    /* Detect the correct MBC that needs to be used and allocate it */
    CARTRIDGE_TYPE type = gb->cartridge->cType;
    switch (type) {
        case CARTRIDGE_NONE: break;         /* No MBC */

        case CARTRIDGE_MBC1: mbc1_allocate(gb, false); break;
        case CARTRIDGE_MBC1_RAM:
        case CARTRIDGE_MBC1_RAM_BATTERY: mbc1_allocate(gb, true); break;

        // case CARTRIDGE_MBC2:
        // case CARTRIDGE_MBC2_BATTERY: mbc2_allocate(gb); break;

		case CARTRIDGE_MBC3:	mbc3_allocate(gb, false, false); break;
		case CARTRIDGE_MBC3_RAM:
		case CARTRIDGE_MBC3_RAM_BATTERY:	mbc3_allocate(gb, true, false); break;
		case CARTRIDGE_MBC3_TIMER_BATTERY:  mbc3_allocate(gb, false, false); break;
		case CARTRIDGE_MBC3_TIMER_RAM_BATTERY:	mbc3_allocate(gb, true, false); break;

		case CARTRIDGE_MBC5:
		case CARTRIDGE_MBC5_RUMBLE:		mbc5_allocate(gb, false); break;
		case CARTRIDGE_MBC5_RAM:
		case CARTRIDGE_MBC5_RUMBLE_RAM:
		case CARTRIDGE_MBC5_RAM_BATTERY:
		case CARTRIDGE_MBC5_RUMBLE_RAM_BATTERY:		mbc5_allocate(gb, true); break;

        default: log_fatal(gb, "MBC/External Hardware Not Supported"); break;
    }
}

void mbc_free(GB* gb) {
    switch (gb->memControllerType) {
        case MBC_NONE: break;
        case MBC_TYPE_1: mbc1_free(gb); break;
        // case MBC_TYPE_2: mbc2_free(gb); break;
		case MBC_TYPE_3: mbc3_free(gb); break;
		case MBC_TYPE_5: mbc5_free(gb); break;
        default: break;
    }
}

uint8_t mbc_readROM_N0(GB* gb, uint16_t addr) {
    /* This is used only when an MBC has been identified */
    switch (gb->memControllerType) {
        case MBC_TYPE_1: return mbc1_readROM_N0(gb, addr);
        // case MBC_TYPE_2: return mbc2_readROM(gb, addr);
		case MBC_TYPE_3: return mbc3_readROM_N0(gb, addr);
		case MBC_TYPE_5: return mbc5_readROM_N0(gb, addr);
        default: return 0xFF;
    }
}

uint8_t mbc_readROM_NN(GB* gb, uint16_t addr) {
    switch (gb->memControllerType) {
        case MBC_TYPE_1: return mbc1_readROM_NN(gb, addr);
		case MBC_TYPE_3: return mbc3_readROM_NN(gb, addr);
		case MBC_TYPE_5: return mbc5_readROM_NN(gb, addr);
        default: return 0xFF;
    }
}

void mbc_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte) {
    /* The address has already been identified as an external ram address
     * so we dont have to check */
    switch (gb->memControllerType) {
        case MBC_NONE: log_warning(gb, "Attempt to write to external RAM without MBC"); break;
        case MBC_TYPE_1: mbc1_writeExternalRAM(gb, addr, byte); break;
//        case MBC_TYPE_2: mbc2_writeBuiltInRAM(gb, addr, byte); break;
		case MBC_TYPE_3: mbc3_writeExternalRAM(gb, addr, byte); break;
		case MBC_TYPE_5: mbc5_writeExternalRAM(gb, addr, byte); break;
        default: break;
    }
}

uint8_t mbc_readExternalRAM(GB* gb, uint16_t addr) {
    switch (gb->memControllerType) {
        case MBC_NONE: log_warning(gb, "Attempt to read from external RAM without MBC"); break;
        case MBC_TYPE_1: return mbc1_readExternalRAM(gb, addr);
//        case MBC_TYPE_2: return mbc2_readBuiltInRAM(gb, addr);
		case MBC_TYPE_3: return mbc3_readExternalRAM(gb, addr);
		case MBC_TYPE_5: return mbc5_readExternalRAM(gb, addr);
        default: return 0xFF;
    }

    return 0xFF;
}

void mbc_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte) {
    switch (gb->memControllerType) {
        case MBC_NONE:
            log_warning(gb, "No MBC exists and write to ROM address doesn't make sense"); break;

        case MBC_TYPE_1: mbc1_interceptROMWrite(gb, addr, byte); break;
//        case MBC_TYPE_2: mbc2_interceptROMWrite(gb, addr, byte); break;
		case MBC_TYPE_3: mbc3_interceptROMWrite(gb, addr, byte); break;
		case MBC_TYPE_5: mbc5_interceptROMWrite(gb, addr, byte); break;
        default: break;
    }

}
