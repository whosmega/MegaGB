#include "../include/mbc.h"
#include "../include/vm.h"
#include "../include/debug.h"
#include <stddef.h>

static void mbc1_allocate(VM* vm, bool externalRam) {
    /* Allocates MBC1 */
    MBC_1* mbc = (MBC_1*)malloc(sizeof(MBC_1));

    if (mbc == NULL) {
        log_fatal(vm, "Error while allocating memory for MBC\n");
        return;
    }

    mbc->bankMode = BANK_MODE_ROM;                  /* Default banking mode */
    mbc->ramBanks = NULL;
    mbc->romBankNumber = 0;
    mbc->secondaryBankNumber = 0;
    mbc->ramEnabled = false;                        /* External RAM is disabled by default */
     
    if (externalRam) {
        /* 4 8Kib Banks */
        mbc->ramBanks = (uint8_t*)malloc(0x2000 * 4);

        if (mbc->ramBanks == NULL) {
            log_fatal(vm, "Error while allocating memory for MBC Ram Banks\n");
            return;
        }
    }

    vm->memController = (void*)mbc;
    vm->memControllerType = MBC_TYPE_1;
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

void mbc_allocate(VM* vm) {
    /* Detect the correct MBC that needs to be used and allocate it */
    CARTRIDGE_TYPE type = vm->cartridge->cType;

    switch (type) {
        case CARTRIDGE_NONE: break;         /* No MBC */
        case CARTRIDGE_MBC1: mbc1_allocate(vm, false); break;
        case CARTRIDGE_MBC1_RAM:
        case CARTRIDGE_MBC1_RAM_BATTERY: mbc1_allocate(vm, true); break;
        default: log_warning(vm, "MBC/External Hardware Not Supported"); break;
    }
}

void mbc_free(VM* vm) {
    /* mem controller is null when either there is no MBC or
     * it isnt supported */

    if (vm->memController != NULL) {
        switch (vm->memControllerType) {
            case MBC_NONE: break;
            case MBC_TYPE_1: mbc1_free(vm); break;

            default: break;
        }
    }
}

void mbc_interceptRead(VM* vm) {

}

void mbc_interceptWrite(VM* vm) {

}
