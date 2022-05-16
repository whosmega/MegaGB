#include "../include/cartridge.h"
#include "../include/vm.h"
#include "../include/cpu.h"
#include "../include/debug.h"
#include "../include/display.h"
#include "../include/mbc.h"

#include <time.h>
#include <string.h>


static void initVM(VM* vm) {
    vm->cartridge = NULL;
    vm->memController = NULL;
    vm->memControllerType = MBC_NONE;
    vm->run = false;
    vm->scheduleInterruptEnable = false;

    vm->cyclesSinceLastFrame = 0;
    vm->clock = 0;
    vm->lastTIMASync = 0;
    vm->lastDIVSync = 0;
	vm->emulatorStartTime = 0;

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

/* Timer */

void incrementTIMA(VM* vm) {
    uint8_t old = vm->MEM[R_TIMA];
    
    if (old == 0xFF) {
        /* Overflow */
        vm->MEM[R_TIMA] = vm->MEM[R_TMA];
        requestInterrupt(vm, INTERRUPT_TIMER);
    } else {
        vm->MEM[R_TIMA]++;
    }
}

void syncTimer(VM* vm) {
    /* This function should be called after every instruction dispatch at minimum
     * it fully syncs the timer despite the length of the interval
     *
     * This is mainly an optimisation because display needs to always be updated
     * and is therefore called after every cycle but it isnt the case for timer
     * It only needs to be updated to request interrupts or provide 
     * correct values when registers are queried / modified 
     *
     * Sometimes the timers should have had been incremented a few cycles 
     * earlier, in that case we calculate the extra cycles and reduce the 
     * lastSync value to match the older cycle and maintain the frequency
     *
     * Because we have 2 timers with different frequencies, 
     * we need 2 different variables to keep the values */
    
    unsigned int cyclesElapsedDIV = vm->clock - vm->lastDIVSync;
    vm->lastDIVSync = vm->clock;
    vm->lastTIMASync = vm->clock;

    /* Sync DIV */
    if (cyclesElapsedDIV >= M_CYCLES_PER_DIV) {
        /* 'Rewind' the last timer sync in case the timer should have been 
         * incremented on an earlier cycle */
        vm->lastDIVSync -= cyclesElapsedDIV - M_CYCLES_PER_DIV;
        vm->MEM[R_DIV]++;
    }

    /* Sync TIMA */
    unsigned int cyclesElapsedTIMA = vm->clock - vm->lastTIMASync;
    uint8_t timerControl    =  vm->MEM[R_TAC];
    uint8_t timerEnabled    =  (timerControl >> 2) & 1;
    uint8_t timerFrequency  =  timerControl & 0b00000011;

    if (timerEnabled) {
        int freqTable[] = {
			1024,	// 4096		t-cycles 
			65536,  // 262144	t-cycles
			16384,  // 85536	t-cycles
			4096    // 16384	t-cycles
		};
        unsigned int freq = freqTable[timerFrequency];
        
        if (cyclesElapsedTIMA >= freq) {
            vm->lastTIMASync -= cyclesElapsedTIMA - freq;
            incrementTIMA(vm);
        }
    }
}

/* ------------------ */ 

static void run(VM* vm) {
	/* We do input polling every 1000 cpu ticks */
	vm->emulatorStartTime = clock();

    while (vm->run) {
		/* Handle Events */
        handleSDLEvents(vm);

		for (int i = 0; (i < 1000) && vm->run; i++) {
			/* Run the next CPU instruction */
			dispatch(vm);
		}
    }
}

void cyclesSync(VM* vm) {
    /* This function is called millions of times by the CPU
     * in a second and therefore it needs to be optimised 
     *
     * So we dont update all hardware but only the ones that need to
     * always be upto date like the display 
	 *
	 * Each call only increments 1 m-cycle by default thus
	 * another function should be made to increment more than 1 cycles to fully 
	 * optimise this. That is rarely done however so thats gonna be on last priority
	 * */
    vm->clock++;

    syncDisplay(vm); 
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
 
	vm.emulatorStartTime = clock();
    /* We are now ready to run */
    vm.run = true;
	 
    run(&vm);
    stopEmulator(&vm);
}

void stopEmulator(VM* vm) {
#ifdef DEBUG_LOGGING
    double totalElapsed = (double)(clock() - vm->emulatorStartTime) / CLOCKS_PER_SEC;
	
    printf("Time Elapsed : %g\n", totalElapsed);
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
