#include "../include/cartridge.h"
#include "../include/vm.h"
#include "../include/cpu.h"
#include "../include/debug.h"
#include "../include/display.h"
#include "../include/mbc.h"

#include <stdint.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>

static void initVM(VM* vm) {
    vm->cartridge = NULL;
    vm->emuMode = EMU_DMG;
    vm->memController = NULL;
    vm->memControllerType = MBC_NONE;
    vm->run = false;
    vm->paused = false;

    vm->scheduleInterruptEnable = false;
	vm->haltMode = false;
	vm->scheduleHaltBug = false;
    vm->scheduleDMA = false;

    vm->clock = 0;
    vm->lastTIMASync = 0;
    vm->lastDIVSync = 0;

	vm->ticksAtLastRender = 0;
	vm->ticksAtStartup = 0;	
 
	vm->ppuMode = PPU_MODE_2;
    vm->hblankDuration = 0;
	vm->ppuEnabled = true;
	vm->skipFrame = false;
    vm->firstTileInScanline = true;
    vm->doOptionalPush = false;
	vm->currentFetcherTask = 0;
    vm->fetcherTileAddress = 0;
    vm->fetcherTileAttributes = 0;
    vm->fetcherX = 0;
    vm->fetcherY = 0;
    vm->fetcherTileRowLow = 0;
    vm->fetcherTileRowHigh = 0;
    vm->nextRenderPixelX = 0;
    vm->nextPushPixelX = 0;
    vm->pauseDotClock = 0;
    vm->pixelsToDiscard = 0;
    vm->currentBackgroundCRAMIndex = 0;
    vm->currentSpriteCRAMIndex = 0;
    vm->windowYCounter = 0;
    vm->lyWasWY = false;
    vm->renderingWindow = false;
    vm->renderingSprites = false;
    vm->spriteData = NULL;
    vm->preservedFetcherTileLow = 0;
    vm->preservedFetcherTileHigh = 0;
    vm->preservedFetcherTileAttributes = 0;
    vm->spriteSize = 0;
    vm->isLastSpriteOverlap = false;
    vm->lastSpriteOverlapPushIndex = 0;
    vm->lastSpriteOverlapX = 0;
    vm->doingDMA = false;
    vm->mCyclesSinceDMA = 0;
    vm->dmaSource = 0;
    
    /* Initialise OAM Buffer */
    memset(&vm->oamDataBuffer, 0xFF, 50);
    vm->spritesInScanline = 0;

	/* Initialise FIFO */
	clearFIFO(&vm->BackgroundFIFO);
    clearFIFO(&vm->OAMFIFO);
}

static void initVMCartridge(VM* vm, Cartridge* cartridge) {
    /* UPDATE : The following is only true for DMG */
	/* The STAT register is supposed to start with mode VBLANK,
	 * (DMG has the first 4 cycles in vblank)
	 * but we start with an initial value that has mode 2
	 *
	 * we also dont use the switch mode function because there are special conditions 
	 * when the ppu is first started, 
	 *
	 * -> OAM isnt locked
	 * -> cycle counters shouldnt be set to 0 but 4 for reasons described below 
	 * -> No STAT checks are necessary because the inital value has already been set */
    
    vm->cartridge = cartridge;
    vm->cartridge->inserted = true;

    if (cartridge->cgbCode == CGB_MODE || cartridge->cgbCode == CGB_DMG_MODE) {
        vm->emuMode = EMU_CGB;
    } else if (cartridge->cgbCode == DMG_MODE) {
        vm->emuMode = EMU_DMG;
    }

    if (vm->emuMode == EMU_CGB) {
	    vm->lockVRAM = false;
	    vm->lockOAM = true;
	    vm->lockPalettes = false;
        vm->cyclesSinceLastFrame = 0;
        vm->cyclesSinceLastMode = 0;

        /* CGB needs WRAM, VRAM and CRAM banks allocated */
        vm->wramBanks = (uint8_t*)malloc(sizeof(uint8_t) * 0x1000 * 7);
        vm->vramBank = (uint8_t*)malloc(sizeof(uint8_t) * 0x2000);
        vm->bgColorRAM = (uint8_t*)malloc(sizeof(uint8_t) * 64);
        vm->spriteColorRAM = (uint8_t*)malloc(sizeof(uint8_t) * 64);
        
        if (vm->wramBanks == NULL || vm->vramBank == NULL || vm->bgColorRAM == NULL ||
            vm->spriteColorRAM == NULL) {

            log_fatal(vm, "[FATAL] Could not allocate space for CGB WRAM/VRAM/CRAM\n");
        }

        /* Set registers & flags to GBC specifics */
        resetGBC(vm);
    
	    /* Set WRAM/VRAM banks if in CGB mode 
	    * Because the default value of SVBK is 0xFF, which means bank 7 is selected 
	    * by default 
	    * Same goes for VBK, bank 1 will be selected by default*/
	    switchCGB_WRAM(vm, 1, 7);
	    switchCGB_VRAM(vm, 0, 1);
    } else if (vm->emuMode == EMU_DMG) {
        /* When the PPU first starts up, it takes 4 cycles less on the first frame,
	     * it also doesnt lock OAM */
        vm->lockOAM = false;
        vm->lockPalettes = false;
        vm->lockVRAM = false;
        vm->cyclesSinceLastFrame = 4;		/* 4 on DMG */
	    vm->cyclesSinceLastMode = 4;		/* ^^^^^^^^ */ 
        vm->wramBanks = NULL;
        vm->bgColorRAM = NULL;
        vm->spriteColorRAM = NULL;
        vm->vramBank = NULL;

        resetGB(vm);
    }
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

/* Utility */
unsigned long clock_u() {
	/* Function to get time with microsecod precision */
	struct timeval t;
	gettimeofday(&t, NULL);

	return (t.tv_sec * 1e6 + t.tv_usec);
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

/* CGB Specific WRAM & VRAM banking */

void switchCGB_WRAM(VM* vm, uint8_t oldBankNumber, uint8_t bankNumber) {
	/* Switch WRAM bank (0xD000-0xDFFF) from oldBankNumber to bankNumber */

	/* Copy contents of the old bank number to its respective bank buffer */
	/* We do oldBankNumber - 1 because bank 0 is not stored in this buffer,
	 * the first ram number that gets stored here is 1 */
	memcpy(&vm->wramBanks[0x1000 * (oldBankNumber - 1)], &vm->MEM[WRAM_NN_4KB], 0x1000);

	/* If old bank buffer is the same as the one to be switched to, copying 
	 * the buffer to the address will cause a previous version of the ram to be loaded
	 * so we put this check in place */

	if (oldBankNumber != bankNumber) {	
		memcpy(&vm->MEM[WRAM_NN_4KB], &vm->wramBanks[0x1000 * (bankNumber - 1)], 0x1000);
	}
}

void switchCGB_VRAM(VM* vm, uint8_t oldBankNumber, uint8_t bankNumber) {
	/* There are only 2 VRAM banks in total so we can basically just swap them */
	if (oldBankNumber != bankNumber) {
		/* Swap */
		for (int i = 0; i < 0x2000; i++) {
			uint8_t b1 = vm->MEM[VRAM_N0_8KB + i];
			uint8_t b2 = vm->vramBank[i];

			vm->MEM[VRAM_N0_8KB + i] = b2;
			vm->vramBank[i] = b1;
		}
	}
}

void syncTimer(VM* vm) {
    /* This function should be called after every instruction dispatch at minimum
     * it fully syncs the timer despite the length of the interval
     *
     * This is mainly an optimisation because display needs to always be updated
     * and is therefore called after every cycle but it isnt the case for timer
     * It only needs to be updated tzo request interrupts or provide 
     * correct values when registers are queried / modified 
     *
     * Sometimes the timers should have had been incremented a few cycles 
     * earlier, in that case we calculate the extra cycles and reduce the 
     * lastSync value to match the older cycle and maintain the frequency
     *
     * Because we have 2 timers with different frequencies, 
     * we need 2 different variables to keep the values 
	 *
	 * lastDIVSync and lastTIMASync store the cycle at which
	 * the last successful sync happened
	 * */
	
	unsigned int cycles = vm->clock;
    unsigned int cyclesElapsedDIV = cycles - vm->lastDIVSync;
    

    /* Sync DIV */
    if (cyclesElapsedDIV >= T_CYCLES_PER_DIV) {
        /* 'Rewind' the last timer sync in case the timer should have been 
         * incremented on an earlier cycle */
        vm->lastDIVSync = cycles - (cyclesElapsedDIV - T_CYCLES_PER_DIV);
        vm->MEM[R_DIV]++;
    }

    /* Sync TIMA */
    unsigned int cyclesElapsedTIMA = cycles - vm->lastTIMASync;
    uint8_t timerControl    =  vm->MEM[R_TAC];
    uint8_t timerEnabled    =  (timerControl >> 2) & 1;
    uint8_t timerFrequency  =  timerControl & 0b00000011;

    if (timerEnabled) {
		/* Cycle table contains number of cycles per increment for its corresponding freq */
        int cycleTable[] = {
			1024,		// 4096 Hz
			16,			// 262144 Hz
			64,			// 65536 Hz
			256			// 16384 Hz
		};
        unsigned int cyc = cycleTable[timerFrequency];
		
        if (cyclesElapsedTIMA >= cyc) {
			/* The least amount of cycles per increment for the timer 
			 * is 16 cycles, which means it has to be often incremented more than
			 * once per instruction 
			 *
			 * This is a good reason to sync it every cycle update but 
			 * it still isnt as significant because the only times it really matters is
			 * when the instructions read its value, we always sync the timer just before
			 * the read so it gets covered. Interrupt timing also isnt a problem because 
			 * interrupts are only checked once per instruction dispatch, and the timer
			 * is synced right before that happens */
			int rem = cyclesElapsedTIMA % cyc;
			int increments = (int)(cyclesElapsedTIMA / cyc);

            vm->lastTIMASync = cycles - rem;

			for (int i = 0; i < increments; i++) {
				incrementTIMA(vm);
			}
        }
    }
}

/* DMA Transfers */
void startDMATransfer(VM* vm, uint8_t byte) {
    if (byte > 0xDF) {
#ifdef DEBUG_LOGGING
        printf("[WARNING] Starting DMA Transfer with address > DFXX, wrapping to DFXX\n");
#endif
        byte = 0xDF;
    }

    uint16_t address = byte * 0x100;

    vm->dmaSource = address;
    vm->mCyclesSinceDMA = 0;

    /* Schedule DMA to begin at the end of this dispatch */
    vm->scheduleDMA = true;
}

void syncDMA(VM* vm) {
    /* DMA Transfers take 160 machine cycles to complete = 640 T-Cycles
     *
     * It needs to be done sequentially sprite by sprite as it is possible to do dma 
     * transfers during mode 2, which can cause the values to be read by the ppu in real time 
     *
     * Calling this function once does 4 tcycles or 1 mcycle of syncing */

    vm->mCyclesSinceDMA++;

    if (vm->mCyclesSinceDMA % 4 == 0) {
        /* it takes 4 M-Cycles to load 1 sprite, as there are 40 OAM entries and 160 M-Cycles 
         * in total */
        
        uint8_t currentSpriteIndex = (vm->mCyclesSinceDMA / 4) - 1;
        uint8_t addressLow = currentSpriteIndex * 4;
        memcpy(&vm->MEM[OAM_N0_160B + addressLow], &vm->MEM[vm->dmaSource + addressLow], 4);
    }

    if (vm->mCyclesSinceDMA == 160) {
        vm->dmaSource = 0;
        vm->mCyclesSinceDMA = 0;
        vm->doingDMA = false;
    }
}

/* ------------------ */ 

static void run(VM* vm) {
	/* We do input polling every 1000 cpu ticks */
	vm->ticksAtStartup = clock_u();

    while (vm->run) {
		for (int i = 0; (i < 500) && vm->run; i++) {
			/* Run the next CPU instruction */
			dispatch(vm);
		}
    }
}

void cyclesSync_4(VM* vm) {
    /* This function is called millions of times by the CPU
     * in a second and therefore it needs to be optimised 
     *
     * So we dont update all hardware but only the ones that need to
     * always be upto date like the display 
	 *
	 */
    vm->clock += 4;
	
    syncDisplay(vm, 4);

    if (vm->doingDMA) syncDMA(vm);
}

/* ---------------------------------------- */ 

void startEmulator(Cartridge* cartridge) {
    VM vm;
    initVM(&vm);
    initVMCartridge(&vm, cartridge);

#ifdef DEBUG_PRINT_CARTRIDGE_INFO
    printCartridge(cartridge); 
#endif
#ifdef DEBUG_LOGGING
    printf("Emulation mode: %s\n", vm.emuMode == EMU_CGB ? "Gameboy Color" : vm.emuMode == EMU_DMG ? "Gameboy" : "");
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

void pauseEmulator(VM* vm) {
    if (vm->paused) return;
    vm->paused = true;

    while (true) {
        if (!vm->paused) break;
    }
}

void unpauseEmulator(VM* vm) {
    if (!vm->paused) return;

    vm->paused = false;
}

void stopEmulator(VM* vm) {
#ifdef DEBUG_LOGGING
    double totalElapsed = (clock_u() - vm->ticksAtStartup) / 1e6;
    printf("Time Elapsed : %g\n", totalElapsed);
    printf("Stopping Emulator Now\n");
    printf("Cleaning allocations\n");
#endif

    /* Free up MBC allocations */
    mbc_free(vm);
    
    if (vm->emuMode == EMU_CGB) {
        /* Free memory allocated specifically for CGB */
        free(vm->vramBank);
        free(vm->wramBanks);
        free(vm->bgColorRAM);
        free(vm->spriteColorRAM);
    }

    /* Reset VM */
    initVM(vm);
}
