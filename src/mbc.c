#include "../include/mbc.h"
#include "../include/vm.h"
#include "../include/mbc1.h"
#include "../include/mbc2.h"
#include "../include/debug.h"
#include <stddef.h>
#include <stdint.h>

/* Bank Switching */
void switchROMBank(VM* vm, int bankNumber) {
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

void switchRestrictedROMBank(VM* vm, int bankNumber) {
    memcpy(&vm->MEM[ROM_N0_16KB], vm->cartridge->allocated, 0x4000);   
}

void mbc_allocate(VM* vm) {
    /* Detect the correct MBC that needs to be used and allocate it */
    CARTRIDGE_TYPE type = vm->cartridge->cType;
    switch (type) {
        case CARTRIDGE_NONE: break;         /* No MBC */
        case CARTRIDGE_MBC1: mbc1_allocate(vm, false); break;
        case CARTRIDGE_MBC1_RAM:
        case CARTRIDGE_MBC1_RAM_BATTERY: mbc1_allocate(vm, true); break;
        case CARTRIDGE_MBC2:
        case CARTRIDGE_MBC2_BATTERY: mbc2_allocate(vm); break;
        default: log_fatal(vm, "MBC/External Hardware Not Supported"); break;
    }
}

void mbc_free(VM* vm) {
    switch (vm->memControllerType) {
        case MBC_NONE: break;
        case MBC_TYPE_1: mbc1_free(vm); break;
        case MBC_TYPE_2: mbc2_free(vm); break;
        default: break;
    }
}

void mbc_writeExternalRAM(VM* vm, uint16_t addr, uint8_t byte) {
    /* The address has already been identified as an external ram address
     * so we dont have to check */
    switch (vm->memControllerType) {
        case MBC_NONE: log_fatal(vm, "Attempt to write to external RAM without MBC"); break;
        case MBC_TYPE_1: mbc1_writeExternalRAM(vm, addr, byte); break;
        case MBC_TYPE_2: mbc2_writeBuiltInRAM(vm, addr, byte); break;
        default: break;
    }
}

uint8_t mbc_readExternalRAM(VM* vm, uint16_t addr) {
    switch (vm->memControllerType) {
        case MBC_NONE: log_fatal(vm, "Attempt to read from external RAM without MBC"); break;
        case MBC_TYPE_1: return mbc1_readExternalRAM(vm, addr);
        case MBC_TYPE_2: return mbc2_readBuiltInRAM(vm, addr);
        default: return 0;
    }

    return 0;
}

void mbc_interceptROMWrite(VM* vm, uint16_t addr, uint8_t byte) {
    switch (vm->memControllerType) {
        case MBC_NONE: 
            log_fatal(vm, "No MBC exists and write to ROM address doesn't make sense"); break;

        case MBC_TYPE_1: mbc1_interceptROMWrite(vm, addr, byte); break;
        case MBC_TYPE_2: mbc2_interceptROMWrite(vm, addr, byte); break;
        default: break;
    }

}
