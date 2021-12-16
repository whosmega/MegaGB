#include "../include/mbc.h"
#include "../include/vm.h"
#include "../include/debug.h"
#include <stddef.h>
#include <stdint.h>

/* Bank Switching */
static void switchROMBank(VM* vm, int bankNumber) {
    /* This function only does the switching part, the checking
     * and decoding is done by MBCs separately */

    uint8_t* allocated = vm->cartridge->allocated;
    uint8_t* bank = &allocated[bankNumber * 0x4000];    /* Size of each bank is 16 KiB */

    memcpy(&vm->MEM[ROM_NN_16KB], bank, 0x4000);             /* Write the contents of the bank */

#ifdef DEBUG_LOGGING
    printf("MBC : Switched ROM Bank to 0x%x\n", bankNumber);
#endif
}

/* This function switches the restricted ROM bank which is normally 
 * considered 'unbankable', but it needs to be remapped in special 
 * cases */

static inline void switchRestrictedROMBank(VM* vm, int bankNumber) {
    memcpy(&vm->MEM[ROM_N0_16KB], &vm->cartridge->allocated, 0x4000);   
}

static inline void mbc1_switchRAMBank(VM* vm, MBC_1* mbc, int bankNumber) {
    /* Each bank is 8KB = 0x2000
     * Checks arent done by this function */
    memcpy(&vm->MEM[RAM_NN_8KB], &mbc->ramBanks[bankNumber], 0x2000);
}

static void mbc1_switchROMBankingMode(VM* vm, MBC_1* mbc) {
    mbc->bankMode = BANK_MODE_ROM;
            
    /* RAM banking is disabled and bank 0 of RAM is locked to 
     * the external RAM address */
    if (mbc->ramBanks != NULL) {
        if (vm->cartridge->extRamSize >= EXT_RAM_8KB) {
            /* Atleast 1 bank of external ram exists, 
            * switch to the first one */
            mbc1_switchRAMBank(vm, mbc, 0);
        }
    }
    
    /* Reset the first rom bank to 0 */
    switchRestrictedROMBank(vm, 0);
}

static void mbc1_switchRAMBankingMode(VM* vm, MBC_1* mbc) {
    mbc->bankMode = BANK_MODE_RAM;

    if (vm->cartridge->extRamSize == EXT_RAM_32KB) {
        /* If this cartridge has external ram which is big enough to be switchable, 
         * we can switch the bank to the one in the secondary bank cartridge only if the 
         * ram is enabled for use */

        if (mbc->ramEnabled) {
            mbc1_switchRAMBank(vm, mbc, mbc->secondaryBankNumber);
        }
    }

    /* MBC1 Remaps the 0x0000-0x3fff area too in this mode for 1MB+ ROMs
     * depending on the value of the the secondary banking register */
    
    if (vm->cartridge->romSize >= ROM_1MB) {
        switchRestrictedROMBank(vm, mbc->secondaryBankNumber * 0x20);
    }
}

static void mbc1_allocate(VM* vm, bool externalRam) {
    /* Allocates MBC1 */
    MBC_1* mbc = (MBC_1*)malloc(sizeof(MBC_1));

    if (mbc == NULL) {
        log_fatal(vm, "Error while allocating memory for MBC\n");
        return;
    }

    mbc->bankMode = BANK_MODE_ROM;                  /* Default banking mode */
    mbc->ramBanks = NULL;
    mbc->romBankNumber = 1;
    mbc->secondaryBankNumber = 0;
    mbc->ramEnabled = false;                        /* External RAM is disabled by default */
     
    if (externalRam) {
        switch (vm->cartridge->extRamSize) {
            case EXT_RAM_0: break;          /* Dont allocate */
            case EXT_RAM_8KB: mbc->ramBanks = (uint8_t*)malloc(0x2000); break;  /* Allocate 1 bank */
            case EXT_RAM_32KB: mbc->ramBanks = (uint8_t*)malloc(0x2000 * 4); break;  /* 4 banks */
            default: log_fatal(vm, "External banks not supported with MBC1"); break;
        }

        if (mbc->ramBanks == NULL) {
            log_fatal(vm, "Error while allocating memory for MBC Ram Banks\n");
            return;
        }
    }

    vm->memController = (void*)mbc;
    vm->memControllerType = MBC_TYPE_1;

    mbc1_switchROMBankingMode(vm, mbc);
}

static void mbc1_writeExternalRAM(VM* vm, uint16_t addr, uint8_t byte) {
    MBC_1* mbc = (MBC_1*)vm->memController;

    if (mbc->ramBanks == NULL)
        log_fatal(vm, "Attempt to write to external RAM when none is present");
        
    if (!mbc->ramEnabled) log_warning(vm, "It is recommended to enable/disable RAM when needed");

    /* Copy the same change to the mirror of the RAM inside our virtual MBC */
    
    if (mbc->bankMode == BANK_MODE_ROM) {
        /* In ROM mode, only bank 0 of ram can be used */
        mbc->ramBanks[addr - 0xA000] = byte;
    } else {
        /* Otherwise if its RAM bank mode, we use the secondary bank register to
         * figure out the ram bank number */
        mbc->ramBanks[(addr - 0xA000) + (mbc->secondaryBankNumber * 0x2000)] = byte;
    }
    
    /* And we finally write it in the main external ram address too */
    vm->MEM[addr] = byte;
}

static void mbc1_free(VM* vm) {
    MBC_1* mbc = (MBC_1*)vm->memController;
    
    if (mbc->ramBanks != NULL) {
        free(mbc->ramBanks);
        mbc->ramBanks = NULL;
    }

    free(mbc);
    vm->memController = NULL;
}

static void mbc1_interceptROMWrite(VM* vm, uint16_t addr, uint8_t byte) {
    MBC_1* mbc = (MBC_1*)vm->memController;
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
    } else if (addr >= 0x2000 && addr <= 0x3fff) {
        /* ROM Bank Number
         *
         * Write to this address range is used to identify the first 5 bits */
        int8_t bankNumber = (byte & 0b00011111);
        
        /* We mask the bank number in case its larger than
         * what the rom contains 
         *
         * 32 KiB cartridges are the smallest and wont need any extra banks */
        switch (vm->cartridge->romSize) {
            case ROM_32KB:  log_fatal(vm, "Bruh why an MBC with a 32KB cart"); break;
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
        switchROMBank(vm, bankNumber);
    } else if (addr >= 0x4000 && addr <= 0x5fff) {
        /* Secondary bank number 
         *
         * In normal mode (mode 0) this register is used to set the upper 2 bits
         * of the bank number, the lower bits should already be stored 
         * in the mbc 
         *
         * In mode 1, this register does the same thing as mode 0 for ROM except 
         * it can now map 0x00, 0x20, 0x40 and 0x60 in the restricted bank area 
         * and also swap RAM banks */
        
        /* The following procedure is for mode 1
         *
         * Check if the rom is even big enough to require 
         * the upper 2 bits. 5 bits should be able to address upto 512KB of
         * ROM */
        if (vm->cartridge->romSize <= ROM_512KB) {
            log_warning(vm, "Attempt to set secondary bank register in memory controller with <= 512KB ROM size in mode 0 (doing this does nothing, and thats why whats the point of this instruction)");
            return;
        }
        uint8_t upperBankNumber = byte & 0b00000011;
        uint8_t fullBankNumber = (upperBankNumber << 5) + mbc->romBankNumber;
                
        /* The full bank number should be a 7 bit unsigned integer now */
        /* We mask the bank number 1 last time in case its larger than what 
         * the rom can contain 
         *
         * 1MB roms can fit in 6 bits, 2MB needs 7 bits and is max for MBC1*/
        switch (vm->cartridge->romSize) {
            case ROM_1MB: fullBankNumber &= 0b00111111; break;
            default: break;
        }

        switchROMBank(vm, fullBankNumber);

        /* Lets check if we are in mode 1, if so, we have more things to do */
        if (mbc->bankMode == BANK_MODE_RAM) {
            /* Check if we have some RAM Banks to switch */
            if (vm->cartridge->extRamSize == EXT_RAM_32KB) {
                mbc1_switchRAMBank(vm, mbc, upperBankNumber);
            }

            /* Check if we have some ROM banks to switch in the restricted area */
            if (vm->cartridge->romSize >= ROM_1MB) {
                switchRestrictedROMBank(vm, upperBankNumber * 0x20);
            }
        }

    } else if (addr >= 0x6000 && addr <= 0x7fff) {
        /* Switch banking mode to Simple ROM or Advanced ROM + RAM */
        if (byte == 0) {
            mbc1_switchROMBankingMode(vm, mbc);
        } else {
            mbc1_switchRAMBankingMode(vm, mbc);
        }
    }
}

void mbc_allocate(VM* vm) {
    /* Detect the correct MBC that needs to be used and allocate it */
    CARTRIDGE_TYPE type = vm->cartridge->cType;

    switch (type) {
        case CARTRIDGE_NONE: break;         /* No MBC */
        case CARTRIDGE_MBC1: mbc1_allocate(vm, false); break;
        case CARTRIDGE_MBC1_RAM:
        case CARTRIDGE_MBC1_RAM_BATTERY: mbc1_allocate(vm, true); break;
        default: log_fatal(vm, "MBC/External Hardware Not Supported"); break;
    }
}

void mbc_free(VM* vm) {
    switch (vm->memControllerType) {
        case MBC_NONE: break;
        case MBC_TYPE_1: mbc1_free(vm); break;

        default: break;
    }
}

void mbc_writeExternalRAM(VM* vm, uint16_t addr, uint8_t byte) {
    /* The address has already been identified as an external ram address
     * so we dont have to check */
    switch (vm->memControllerType) {
        case MBC_NONE: log_fatal(vm, "Attempt to write to external RAM without MBC"); break;
        case MBC_TYPE_1: mbc1_writeExternalRAM(vm, addr, byte); break;
        
        default: break;
    }
}

void mbc_interceptROMWrite(VM* vm, uint16_t addr, uint8_t byte) {
    switch (vm->memControllerType) {
        case MBC_NONE: 
            log_fatal(vm, "No MBC exists and write to ROM address doesn't make sense"); break;

        case MBC_TYPE_1: mbc1_interceptROMWrite(vm, addr, byte); break;
        default: break;
    }

}
