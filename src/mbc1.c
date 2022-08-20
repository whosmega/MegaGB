#include "../include/debug.h"
#include "../include/mbc.h"
#include "../include/mbc1.h"

static inline void mbc1_switchRAMBank(GB* gb, MBC_1* mbc, int bankNumber) {
    /* Each bank is 8KB = 0x2000
     * Checks arent done by this function */
}

static void mbc1_switchROMBankingMode(GB* gb, MBC_1* mbc) {
    mbc->bankMode = BANK_MODE_ROM;

    /* RAM banking is disabled and bank 0 of RAM is locked to
     * the external RAM address */
    if (mbc->ramBanks != NULL) {
        if (gb->cartridge->extRamSize >= EXT_RAM_8KB) {
            /* Atleast 1 bank of external ram exists,
            * switch to the first one */
            mbc1_switchRAMBank(gb, mbc, 0);
        }
    }

    /* Reset the first rom bank to 0 */
    switchRestrictedROMBank(gb, 0);
}

static void mbc1_switchRAMBankingMode(GB* gb, MBC_1* mbc) {
    mbc->bankMode = BANK_MODE_RAM;

    if (gb->cartridge->extRamSize == EXT_RAM_32KB) {
        /* If this cartridge has external ram which is big enough to be switchable,
         * we can switch the bank to the one in the secondary bank cartridge only if the
         * ram is enabled for use */

        if (mbc->ramEnabled) {
            mbc1_switchRAMBank(gb, mbc, mbc->secondaryBankNumber);
        }
    }

    /* MBC1 Remaps the 0x0000-0x3fff area too in this mode for 1MB+ ROMs
     * depending on the value of the the secondary banking register */

    if (gb->cartridge->romSize >= ROM_1MB) {
        switchRestrictedROMBank(gb, mbc->secondaryBankNumber * 0x20);
    }
}

void mbc1_allocate(GB* gb, bool externalRam) {
    /* Allocates MBC1 */
    MBC_1* mbc = (MBC_1*)malloc(sizeof(MBC_1));

    if (mbc == NULL) {
        log_fatal(gb, "Error while allocating memory for MBC\n");
        return;
    }

    mbc->bankMode = BANK_MODE_ROM;                  /* Default banking mode */
    mbc->ramBanks = NULL;
    mbc->romBankNumber = 1;
    mbc->secondaryBankNumber = 0;
    mbc->ramEnabled = false;                        /* External RAM is disabled by default */

    if (externalRam) {
        switch (gb->cartridge->extRamSize) {
            case EXT_RAM_0: break;          /* Dont allocate */
            case EXT_RAM_8KB: mbc->ramBanks = (uint8_t*)malloc(0x2000); break;  /* Allocate 1 bank */
            case EXT_RAM_32KB: mbc->ramBanks = (uint8_t*)malloc(0x2000 * 4); break;  /* 4 banks */
            default: log_fatal(gb, "External banks not supported with MBC1"); break;
        }

        if (mbc->ramBanks == NULL) {
            log_fatal(gb, "Error while allocating memory for MBC Ram Banks\n");
            return;
        }
    }

    gb->memController = (void*)mbc;
    gb->memControllerType = MBC_TYPE_1;

    mbc1_switchROMBankingMode(gb, mbc);
}

uint8_t mbc1_readROM(GB *gb, uint16_t addr) {
    return gb->cartridge->allocated[addr];
}

void mbc1_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte) {
    MBC_1* mbc = (MBC_1*)gb->memController;

    if (mbc->ramBanks == NULL) {
        // log_fatal(gb, "Attempt to write to external RAM when none is present");
        return;
    }

    if (!mbc->ramEnabled) {
        log_warning(gb, "It is recommended to enable RAM when writing");
        return;
    }

    /* We write the value in our external RAM array
     *
     * 0xA000 is the start address of the external RAM
     * so we subtract it from the full address to get a
     * relative ram address */

    if (mbc->bankMode == BANK_MODE_ROM) {
        /* In ROM mode, only bank 0 of ram can be used */
        mbc->ramBanks[addr - 0xA000] = byte;
    } else {
        /* Otherwise if its RAM bank mode, we use the secondary bank register to
         * figure out the ram bank number */
        mbc->ramBanks[(addr - 0xA000) + (mbc->secondaryBankNumber * 0x2000)] = byte;
    }
}

uint8_t mbc1_readExternalRAM(GB* gb, uint16_t addr) {
    MBC_1* mbc = (MBC_1*)gb->memController;

    if (mbc->ramBanks == NULL) {
        // log_fatal(gb, "Attempt to read to external RAM when none is present");
        return 0;
    }

    if (!mbc->ramEnabled) {
        log_warning(gb, "It is recommended to enable RAM when read");
        return 0;
    }

    if (mbc->bankMode == BANK_MODE_ROM) {
        /* RAM is locked to only the first bank */
        return mbc->ramBanks[addr - 0xA000];
    } else {
        return mbc->ramBanks[(addr - 0xA000) + (mbc->secondaryBankNumber * 0x2000)];
    }
}

void mbc1_free(GB* gb) {
    MBC_1* mbc = (MBC_1*)gb->memController;

    if (mbc->ramBanks != NULL) {
        free(mbc->ramBanks);
        mbc->ramBanks = NULL;
    }

    free(mbc);
    gb->memController = NULL;
}

void mbc1_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte) {
    MBC_1* mbc = (MBC_1*)gb->memController;
    /* Check if we have to write to a register or switch banks */
    if (addr >= 0x0000 && addr <= 0x1fff) {
        /* Enable/Disable external ram
         *
         * Note : The value of this register doesnt matter in this emulator
         * because there is no danger of ram corruption or other stuff
         * This only exists for compatibibilty */

        /* Any value that has 0xA in the lower 4 bits enables ram,
         * and any other value disables it */
        if ((byte & 0xF) == 0xA) mbc->ramEnabled = true;
        else mbc->ramEnabled = false;

#ifdef DEBUG_LOGGING
        printf("MBC : %s RAM\n", mbc->ramEnabled ? "Enabled" : "Disabled");
#endif
    } else if (addr >= 0x2000 && addr <= 0x3fff) {
        /* ROM Bank Number
         *
         * Write to this address range is used to identify the first 5 bits */
        int8_t bankNumber = (byte & 0b00011111);

        /* We mask the bank number in case its larger than
         * what the rom contains
         *
         * 32 KiB cartridges are the smallest and wont need any extra banks */
        switch (gb->cartridge->romSize) {
            case ROM_32KB:  bankNumber &= 0b00000001; break;
            case ROM_64KB:  bankNumber &= 0b00000011; break;
            case ROM_128KB: bankNumber &= 0b00000111; break;
            case ROM_256KB: bankNumber &= 0b00001111; break;

            /* Any number larger than this doesnt need any masking */
            default: break;
        }

        /* Finally we make sure it isnt 0, 20, 40 or 60, and set it to 1 if it is */
        switch (bankNumber) {
            case 0x0:  bankNumber   =      0x1; break;
            case 0x20: bankNumber   =     0x21; break;
            case 0x40: bankNumber   =     0x41; break;
            case 0x60: bankNumber   =     0x61; break;
            default: break;
        }

        mbc->romBankNumber = bankNumber;
        switchROMBank(gb, bankNumber);
    } else if (addr >= 0x4000 && addr <= 0x5fff) {
        /* Secondary bank number
         *
         * In rom mode (mode 0) this register is used to set the upper 2 bits
         * of the bank number, the lower bits should already be stored
         * in the mbc
         *
         * In ram mode (mode 1), this register is used to switch between the 4 ram banks  */

        uint8_t upperBankNumber = byte & 0b00000011;
        if (mbc->bankMode == BANK_MODE_ROM) {
            /*
            * Check if the rom is even big enough to require
            * the upper 2 bits. 5 bits should be able to address upto 512KB of
            * ROM */
            if (gb->cartridge->romSize <= ROM_512KB) {
                /* ROM isnt big enough */
                return;
            }
            uint8_t fullBankNumber = (upperBankNumber << 5) + mbc->romBankNumber;

            /* The full bank number should be a 7 bit unsigned integer now */
            /* We mask the bank number 1 last time in case its larger than what
             * the rom can contain
             *
             * 1MB roms can fit in 6 bits, 2MB needs 7 bits and is max for MBC1*/
            switch (gb->cartridge->romSize) {
                case ROM_1MB: fullBankNumber &= 0b00111111; break;
                case ROM_2MB: fullBankNumber &= 0b01111111; break;
                default: break;
            }

            switchROMBank(gb, fullBankNumber);
        } else if (mbc->bankMode == BANK_MODE_RAM) {
            /* We have some RAM Banks to switch */
            if (gb->cartridge->extRamSize == EXT_RAM_32KB) {
                mbc1_switchRAMBank(gb, mbc, upperBankNumber);
            }
        }

    } else if (addr >= 0x6000 && addr <= 0x7fff) {
        /* Switch banking mode to Simple ROM or Advanced ROM + RAM */
        if (byte == 0) {
            mbc1_switchROMBankingMode(gb, mbc);
        } else {
            mbc1_switchRAMBankingMode(gb, mbc);
        }

#ifdef DEBUG_LOGGING
        printf("MBC : Switched to mode %d\n", byte ? 1 : 0);
#endif
    } else {
        log_fatal(gb, "MBC : Attempt to write to an undefined MBC register");
        return;
    }
}
