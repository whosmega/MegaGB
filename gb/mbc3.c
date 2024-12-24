#include <gb/debug.h>
#include <gb/mbc.h>
#include <gb/mbc3.h>
#include <time.h>

void mbc3_allocate(GB* gb, bool externalRam, bool rtc) {
    MBC_3* mbc = (MBC_3*)malloc(sizeof(MBC_3));

    if (mbc == NULL) {
        log_fatal(gb, "Error while allocating memory for MBC3\n");
        return;
    }

    mbc->latchRegister = 0x1; 
    mbc->ram_rtcBankNumber = 0;
    mbc->romBankNumber = 0;
    mbc->ram_rtcEnabled = false;
    
    mbc->selectedRTCRegister = 0;
    mbc->selectedRAMBank = 0;
    mbc->selectedROMBank = 1;

    mbc->ramBanks = NULL;
    mbc->rtcSupported = false;
   
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
		mbc->romBankNumber = byte & 0b01111111;
		/* Mask unused bits depending on rom size -> Supported upto 2 MB */
		uint8_t bankNumber = byte & ((1 << (gb->cartridge->romSize + 1)) - 1);
		if (bankNumber == 0) bankNumber = 1;
		mbc->selectedROMBank = bankNumber;
    } else if (addr >= 0x4000 && addr <= 0x5FFF) {
		byte &= 0xF;
		
		if (byte <= 0x03) {
			if (mbc->ramBanks == NULL) return;
			if (gb->cartridge->extRamSize == EXT_RAM_8KB) byte = 0;

			mbc->ram_rtcBankNumber = byte;
			mbc->selectedRAMBank = byte;
		} else if (byte >= 0x08 && byte <= 0x0C) {
			if (!mbc->rtcSupported) return;
			mbc->ram_rtcBankNumber = byte;
			mbc->selectedRTCRegister = byte;
		}

		/* All other writes are ignored and do nothing */
    } else if (addr >= 0x6000 && addr <= 0x7FFF) {
		/* Used to latch time onto registers */
		if (mbc->latchRegister == 0 && byte == 1) {
			/* Latch */
		}

		mbc->latchRegister = byte;
    }
}

uint8_t mbc3_readExternalRAM(GB* gb, uint16_t addr) {
    /* Can be a read to RAM or RTC register */
	MBC_3* mbc = (MBC_3*)gb->memController;

	if (mbc->ram_rtcBankNumber < 0x04) {
		/* RAM */
		if (mbc->ramBanks == NULL) return 0xFF;
		return mbc->ramBanks[mbc->selectedRAMBank * 0x2000 + addr];
	} else {
		/* RTC */
		if (!mbc->rtcSupported) return 0;
		return 0;
	}
}

void mbc3_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte) {
	MBC_3* mbc = (MBC_3*)gb->memController;

	if (mbc->ram_rtcBankNumber < 0x04) {
		/* RAM */
		if (mbc->ramBanks == NULL) return;

		mbc->ramBanks[mbc->selectedRAMBank * 0x2000 + addr] = byte;
	} else {
		/* RTC */
		if (!mbc->rtcSupported) return;

	}
}

uint8_t mbc3_readROM_N0(GB* gb, uint16_t addr) {
	return gb->cartridge->allocated[addr];
}

uint8_t mbc3_readROM_NN(GB* gb, uint16_t addr) {
	MBC_3* mbc = (MBC_3*)gb->memController;

    return gb->cartridge->allocated[mbc->selectedROMBank * 0x4000 + addr];
}
