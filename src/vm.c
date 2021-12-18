#include "../include/cartridge.h"
#include "../include/vm.h"
#include "../include/cpu.h"
#include "../include/debug.h"
#include "../include/display.h"
#include "../include/mbc.h"
#include <string.h>
#include <pthread.h>

static void initVM(VM* vm) {
    vm->cartridge = NULL;
    vm->gtkApp = NULL;
    vm->conditionFalse = false;
    vm->cpuThreadID = 0;
    vm->displayThreadID = 0;
    vm->memController = NULL;
    vm->memControllerType = MBC_NONE;
    vm->stopping = false;

    vm->cpuThreadStatus = THREAD_DEAD;
    vm->displayThreadStatus = THREAD_DEAD;
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

void startEmulator(Cartridge* cartridge) {
    VM vm;
    initVM(&vm);
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
#ifdef DEBUT_LOGGING
    printf("Setting up Memory Bank Controller\n");
#endif
    mbc_allocate(&vm);
#ifdef DEBUG_LOGGING
    printf("Starting Display Worker Thread\n");
#endif
    pthread_create(&vm.displayThreadID, NULL, startDisplay, (void*)&vm);
#ifdef DEBUG_LOGGING
    printf("Starting CPU Worker Thread\n");
#endif
    sleep(1);
    pthread_create(&vm.cpuThreadID, NULL, runCPU, (void*)&vm);

    /* We wait till the window is closed then we shut down the emulator */
    pthread_join(vm.displayThreadID, NULL);
    /* Reset all fields to null and stop all running threads
     *
     * We dont care about the cartridge */
    stopEmulator(&vm);
}

void stopEmulator(VM* vm) {
    if (vm->stopping) return;
    vm->stopping = true;
#ifdef DEBUG_LOGGING
    printf("Stopping Emulator Now\n");
#endif
    /* Stop the CPU and display */
    if (vm->displayThreadStatus == THREAD_RUNNING) {
#ifdef DEBUG_LOGGING
        printf("Stopping Display Worker Thread\n");
#endif
        pthread_cancel(vm->displayThreadID);
    }

    if (vm->cpuThreadStatus == THREAD_RUNNING) {
#ifdef DEBUG_LOGGING
        printf("Stopping CPU Worker Thread\n");
#endif
        pthread_cancel(vm->cpuThreadID);
    }
    
    /* Wait till the cleanup handlers have finished */
    while (vm->displayThreadStatus == THREAD_RUNNING ||
           vm->cpuThreadStatus == THREAD_RUNNING);

#ifdef DEBUG_LOGGING
    printf("Cleaning allocations\n");
#endif
    mbc_free(vm);
    /* Reset VM */
    initVM(vm);
}
