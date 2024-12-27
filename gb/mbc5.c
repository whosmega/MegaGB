#include "gb/cartridge.h"
#include <gb/debug.h>
#include <gb/mbc.h>
#include <gb/mbc5.h>
#include <stdint.h>

void mbc5_allocate(GB* gb, bool externalRam) {
    MBC_5* mbc = (MBC_5*)malloc(sizeof(MBC_5));

    if (mbc == NULL) {
        log_fatal(gb, "Error while allocating memory for MBC5\n");
        return;
    }

	mbc->ramEnabled = false;
    mbc->selectedRAMBank = 0;
    mbc->selectedROMBank = 1;

    mbc->ramBanks = NULL;
   
    if (externalRam) {
        switch (gb->cartridge->extRamSize) {
            case EXT_RAM_0: break;
            case EXT_RAM_8KB: mbc->ramBanks = (uint8_t*)malloc(0x2000 * 1); break;
            case EXT_RAM_32KB: mbc->ramBanks = (uint8_t*)malloc(0x2000 * 4); break;
			case EXT_RAM_128KB: mbc->ramBanks = (uint8_t*)malloc(0x2000 * 16); break;
            default: log_fatal(gb, "External RAM Banks not supported with MBC5\n");
        }
    }


    gb->memController = (void*)mbc;
    gb->memControllerType = MBC_TYPE_5;
}

void mbc5_free(GB* gb) {
    MBC_5* mbc = (MBC_5*)gb->memController;

    if (mbc->ramBanks != NULL) {
        free(mbc->ramBanks);
        mbc->ramBanks = NULL;
    }

    free(mbc);

    gb->memController = NULL;
}

void mbc5_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte) {
    MBC_5* mbc = (MBC_5*)gb->memController;

    /* MBC Registers */

    if (addr >= 0x0000 && addr <= 0x1FFF) {
        /* RAM reading and writing enable/disable */
        if ((byte & 0xF) == 0xA) mbc->ramEnabled = true;
        else mbc->ramEnabled = false;
    } else if (addr >= 0x2000 && addr <= 0x2FFF) {
		/* Mask unused bits depending on rom size -> Supported upto 8 MB */
		uint8_t bankNumber = byte & ((1 << (gb->cartridge->romSize + 1)) - 1);
		mbc->selectedROMBank = bankNumber;
    } else if (addr >= 0x3000 && addr <= 0x3FFF) {
		/* 9th bit of ROM bank number */
		if (gb->cartridge->romSize != ROM_8MB) return;
		mbc->selectedROMBank |= (byte & 1) << 8;
    } else if (addr >= 0x4000 && addr <= 0x5FFF) {
		/* Selecting RAM Bank from 0x00-0x0F */
		uint8_t mask = 0;
		switch (gb->cartridge->extRamSize) {
			case EXT_RAM_32KB: {
				mask = 0b11;
				break;
			}
			case EXT_RAM_128KB: {
				mask = 0b1111;
				break;
			}
			default: break;
		}

		mbc->selectedRAMBank = byte & mask;
    }
}

uint8_t mbc5_readExternalRAM(GB* gb, uint16_t addr) {
	MBC_5* mbc = (MBC_5*)gb->memController;

	if (mbc->ramEnabled && mbc->ramBanks != NULL) {
		/* RAM */
		return mbc->ramBanks[mbc->selectedRAMBank * 0x2000 + addr];
	} else return 0xFF;
}

void mbc5_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte) {
	MBC_5* mbc = (MBC_5*)gb->memController;

	if (mbc->ramEnabled && mbc->ramBanks != NULL) {
		/* RAM */
		mbc->ramBanks[mbc->selectedRAMBank * 0x2000 + addr] = byte;
	} else return;
}

uint8_t mbc5_readROM_N0(GB* gb, uint16_t addr) {
	return gb->cartridge->allocated[addr];
}

uint8_t mbc5_readROM_NN(GB* gb, uint16_t addr) {
	MBC_5* mbc = (MBC_5*)gb->memController;

    return gb->cartridge->allocated[mbc->selectedROMBank * 0x4000 + addr];
}
