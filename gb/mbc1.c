#include <gb/debug.h>
#include <gb/mbc.h>
#include <gb/mbc1.h>


void mbc1_allocate(GB* gb, bool externalRam) {
    /* Allocates MBC1 */
    MBC_1* mbc = (MBC_1*)malloc(sizeof(MBC_1));

    if (mbc == NULL) {
        log_fatal(gb, "Error while allocating memory for MBC\n");
        return;
    }

    mbc->bankMode = BANK_MODE_ROM;                  /* Default banking mode */
    mbc->ramBanks = NULL;
    mbc->romBankNumber = 0;
    mbc->secondaryBankNumber = 0;
    mbc->ramEnabled = false;                        /* External RAM is disabled by default */

    mbc->selectedRAMBank = 0;
    mbc->selectedROMBank = 1;
    mbc->selectedROM0Bank = 0;

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

static void syncMBC1(GB* gb, MBC_1* mbc) {
    /* Updates banking numbers for RAM/ROM depending on the values
     * of the banking registers and banking mode 
     *
     * This is done only when writes to MBC registers are made instead of 
     * being calculated dynamically at every rom/ram read to increase performance */

    switch (mbc->bankMode) {
        case BANK_MODE_ROM: {
            /* ROM Banking mode */
            uint8_t bankNumber = mbc->romBankNumber;

            /* If lower 5 bits are 0, they are treated as 1 */
            if (bankNumber == 0) bankNumber = 1;

            if (gb->cartridge->romSize >= ROM_1MB) {
                /* Use secondary banking number too */
                bankNumber |= mbc->secondaryBankNumber << 5;
            }
            /* Masks appropriate bits depending on the size of the ROM, (assuming enum starts from 0)
             * for example, 32 KB rom requires only 1 bit, 64 KB rom requires 2 bits, etc 
             * 5 bit bank number works only till 512 KB roms
             *
             * For roms >= 1MB, the 2 bit secondary bank register is used to
             * provide a 7 bit number, supporting upto 2 MB */
            uint8_t size = gb->cartridge->romSize > ROM_2MB ? ROM_2MB : gb->cartridge->romSize; 
            bankNumber &= (1 << (size + 1)) - 1;

            /* Switchable ROM Bank is calculated, ROM0 bank and RAM bank is locked to 0 */
            mbc->selectedROMBank = bankNumber;
            mbc->selectedROM0Bank = 0;
            mbc->selectedRAMBank = 0;
            break;
        }
        case BANK_MODE_RAM: {
            /* RAM Banking mode */
            uint8_t size = gb->cartridge->romSize > ROM_2MB ? ROM_2MB : gb->cartridge->romSize;
            uint8_t secondaryBankNumber = mbc->secondaryBankNumber;
            /* The 5 bit rom banking register still functions normally */
            uint8_t bankNumber = mbc->romBankNumber;
            if (bankNumber == 0) bankNumber = 1;
 
            if (gb->cartridge->romSize >= ROM_1MB) {
                /* ROM >= 1 MB is affected by the secondary banking register,
                 * bank number 0x00, 0x20, 0x40 and 0x60 are switchable using the 2 bit register 
                 * in addition to the 2 bit register acting as an extension to the rom bank number *
                 */

                uint8_t rom0Bank = secondaryBankNumber << 5;
                /* For 1 MB roms, we still need to mask the bank number, the highest
                 * it can support in rom0 slot is 0x20, for 2 MB roms or higher, no masking is 
                 * required */
                if (size == ROM_1MB) rom0Bank &= 0x3F;
                mbc->selectedROM0Bank = rom0Bank;

                bankNumber |= secondaryBankNumber << 5;
            } else mbc->selectedROM0Bank = 0;

            if (gb->cartridge->extRamSize >= EXT_RAM_32KB) {
                mbc->selectedRAMBank = secondaryBankNumber;
            } else mbc->selectedRAMBank = 0;

            /* Normally set the switchable rom bank after masking */
            bankNumber &= (1 << (size + 1)) - 1;
            mbc->selectedROMBank = bankNumber; 
            break;
        }
    }
}

uint8_t mbc1_readROM_N0(GB *gb, uint16_t addr) {
    MBC_1* mbc = (MBC_1*)gb->memController;

    return gb->cartridge->allocated[mbc->selectedROM0Bank * 0x4000 + addr];
}

uint8_t mbc1_readROM_NN(GB* gb, uint16_t addr) {
    MBC_1* mbc = (MBC_1*)gb->memController;

    return gb->cartridge->allocated[mbc->selectedROMBank * 0x4000 + addr];
}

void mbc1_writeExternalRAM(GB* gb, uint16_t addr, uint8_t byte) {
    MBC_1* mbc = (MBC_1*)gb->memController;

    if (!mbc->ramEnabled) return;
    if (mbc->ramBanks == NULL) return;
    mbc->ramBanks[(0x2000 * mbc->selectedRAMBank) + addr] = byte;
}

uint8_t mbc1_readExternalRAM(GB* gb, uint16_t addr) {
    MBC_1* mbc = (MBC_1*)gb->memController;

    if (mbc->ramBanks == NULL) return 0xFF;
    if (!mbc->ramEnabled) return 0xFF;
    return mbc->ramBanks[0x2000 * mbc->selectedRAMBank + addr];
}

void mbc1_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte) {
    MBC_1* mbc = (MBC_1*)gb->memController;

    /* Register writes */
    if (addr >= 0x0000 && addr <= 0x1FFF) {
        /* RAM Enable/Disable */
        mbc->ramEnabled = (byte & 0xF) == 0xA;
    } else if (addr >= 0x2000 && addr <= 0x3FFF) {
        /* 5 bit register */
        mbc->romBankNumber = byte & 0x1F;
        syncMBC1(gb, mbc);
    } else if (addr >= 0x4000 && addr <= 0x5FFF) {
        /* 2 bit register */
        mbc->secondaryBankNumber = byte & 0x3;
        syncMBC1(gb, mbc);
    } else if (addr >= 0x6000 && addr <= 0x7FFF) {
        /* 1 bit register */
        mbc->bankMode = byte & 0x1;
        syncMBC1(gb, mbc);
    }
}
