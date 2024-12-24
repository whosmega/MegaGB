#include <gb/mbc.h>
#include <gb/mbc2.h>
#include <gb/debug.h>

void mbc2_allocate(GB* gb) {
    MBC_2* mbc = (MBC_2*)malloc(sizeof(MBC_2));
    mbc->ramEnabled = false;

    /* Built in RAM is left uninitialised */

    gb->memController = (void*)mbc;
    gb->memControllerType = MBC_TYPE_2;
}

void mbc2_free(GB* gb) {
    MBC_2* mbc = (MBC_2*)gb->memController;

    free(mbc);
    gb->memController = NULL;
}

void mbc2_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte) {
    MBC_2* mbc = (MBC_2*)gb->memController;

    if (addr >= 0x0000 && addr <= 0x3FFF) {
        /* If bit 8 of the address is clear, the value written is
         * used to enable/disable ram
         *
         * If bit 8 is set, it is used to do a bank switch
         * The bank number is determined with the lower 4 bits
         * of the value written */

        if ((addr >> 8) & 0x1) {
            /* Bit 8 is set, do a bank switch */
            uint8_t bankNumber = byte & 0xF;

            /* We mask the bank number to fit any smaller rom sizes
             *
             * Max MBC2 supports is 256KB ROM which can be addressed by
             * the entire 4 bits */

            switch (gb->cartridge->romSize) {
                case ROM_32KB:  log_warning(gb, "Why're u bank switching with a 32KB ROM"); return;
                case ROM_64KB:  bankNumber &= 0b00000011; break;
                case ROM_128KB: bankNumber &= 0b00000111; break;
                case ROM_256KB: break;                  /* No masking needed */
                default: log_warning(gb, "ROM is too big for MBC2, it can only be partially mapped");
                         break;
            }

            /* BankNumber 0 gets translated to 1 */
            if (bankNumber == 0) bankNumber = 1;

            switchROMBank(gb, bankNumber);
        } else {
            /* Enable/Disable RAM
             *
             * 0x0A = Enable
             * Anything Else = Disable */

            mbc->ramEnabled = byte == 0x0A;

#ifdef DEBUG_LOGGING
            printf("MBC : RAM %s\n", mbc->ramEnabled ? "Enabled" : "Disabled");
#endif
        }
    } else {
        log_fatal(gb, "MBC : Attempt to write to an undefined MBC Register");
        return;
    }
}

uint8_t mbc2_readROM(GB *gb, uint16_t addr) {
    return 0xFF;
}

void mbc2_writeBuiltInRAM(GB* gb, uint16_t addr, uint8_t byte) {
    MBC_2* mbc = (MBC_2*)gb->memController;
    if (!mbc->ramEnabled) log_warning(gb, "It is recommended to enable RAM when writing");

    /* Only the bottom 9 bits are used to address the memory, the upper bits
     * dont matter and are thus 'echoed', for our purpose we just mask it */

    addr &= 0b0000000111111111;

    /* In this RAM only the lower 4 bits are supposed to be stored/retrieved
     * but we can just dump the whole byte too since upper bits are undefined
     * anyway */
    mbc->builtInRAM[addr] = byte;
}

uint8_t mbc2_readBuiltInRAM(GB* gb, uint16_t addr) {
    MBC_2* mbc = (MBC_2*)gb->memController;
    if (!mbc->ramEnabled) log_warning(gb, "It is recommended to enable RAM when reading");

    /* Just like writing to ram, we make up for echoing memory by masking
     * the bits */

    addr &= 0b0000000111111111;

    return mbc->builtInRAM[addr];
}
