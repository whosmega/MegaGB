#include "../include/cartridge.h"
#include "../include/vm.h"
#include "../include/cpu.h"
#include "../include/debug.h"
#include "../include/clock.h"
#include "../include/display.h"
#include "../include/mbc.h"
#include <string.h>


static void initVM(VM* vm) {
    vm->cartridge = NULL;
    vm->memController = NULL;
    vm->memControllerType = MBC_NONE;
    vm->run = false;

    vm->sdl_window = NULL;
    vm->sdl_renderer = NULL;

    /* Set registers & flags to GBC specifics */
    resetGBC(vm);
}


static void bootROM(VM* vm) {
    /* This is only a temporary boot rom function,
     * the original boot rom will be in binary and will
     * be mapped over correctly when the cpu is complete
     *
     *
     * The boot procedure is only minimal and is incomplete */

    uint8_t logo[0x30] = {0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D,
                          0x00, 0x0B, 0x03, 0x73, 0x00, 0x83,
                          0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08,
                          0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
                          0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD,
                          0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x64,
                          0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC,
                          0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E};
#ifndef DEBUG_NO_CARTRIDGE_VERIFICATION    
    bool logoVerified = memcmp(&vm->cartridge->logoChecksum, &logo, 0x18) == 0;
    
    if (!logoVerified) {
        log_fatal(vm, "Logo Verification Failed");
    }
        
    int checksum = 0;
    for (int i = 0x134; i <= 0x14C; i++) {
        checksum = checksum - vm->cartridge->allocated[i] - 1;
    }

    if ((checksum & 0xFF) != vm->cartridge->headerChecksum) {
        log_fatal(vm, "Header Checksum Doesn't Match, it is possibly corrupted");
    }
#endif
    /* Map the cartridge rom to the GBC rom space 
     * occupying bank 0 and 1, a total of 32 KB*/
    memcpy(&vm->MEM[ROM_N0_16KB], vm->cartridge->allocated, 0x8000);
}

static void run(VM* vm) {
    while (vm->run) {
        /* Handle Events */
        handleSDLEvents(vm);
        /* Run the next CPU instruction */
        dispatch(vm);
    }
}

void startEmulator(Cartridge* cartridge) {
    VM vm;
    initVM(&vm);

    /* Start up SDL */
    int status = initSDL(&vm);
    if (status != 0) {
        /* An error occurred and SDL wasnt started
         *
         * This is fatal as our emulator cannot run without it
         * and we immediately quit */
        log_fatal(&vm, "Error Starting SDL2");
        return;
    }
    /* Load the cartridge */
    vm.cartridge = cartridge;
    vm.cartridge->inserted = true;
    
#ifdef DEBUG_PRINT_CARTRIDGE_INFO
    printCartridge(cartridge); 
#endif
#ifdef DEBUG_LOGGING
    printf("Booting into ROM\n");
#endif
    bootROM(&vm);
#ifdef DEBUG_LOGGING
    printf("Setting up Memory Bank Controller\n");
#endif
    mbc_allocate(&vm);
 
    /* We are now ready to run */
    vm.run = true;
    
    run(&vm);
    stopEmulator(&vm);
}

void stopEmulator(VM* vm) {
#ifdef DEBUG_LOGGING
    printf("Stopping Emulator Now\n");
    printf("Cleaning allocations\n");
#endif

    /* Free up all SDL allocations and stop it */
    freeSDL(vm);
    /* Free up MBC allocations */
    mbc_free(vm);
    /* Reset VM */
    initVM(vm);
}
