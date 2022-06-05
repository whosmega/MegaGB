#include "../include/cartridge.h"
#include "../include/vm.h"
#include "../include/cpu.h"
#include "../include/debug.h"
#include "../include/display.h"
#include "../include/mbc.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_timer.h>
#include <bits/time.h>
#include <time.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <sys/time.h>

static void initVM(VM* vm) {
    vm->cartridge = NULL;
    vm->memController = NULL;
    vm->memControllerType = MBC_NONE;
    vm->run = false;

    vm->scheduleInterruptEnable = false;
	vm->haltMode = false;
	vm->scheduleHaltBug = false;

    vm->clock = 0;
    vm->lastTIMASync = 0;
    vm->lastDIVSync = 0;

    vm->sdl_window = NULL;
    vm->sdl_renderer = NULL;
	vm->ticksAtLastRender = 0;
	vm->ticksAtStartup = 0;
	
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

	vm->lockVRAM = false;
	vm->lockOAM = true;
	vm->lockPalettes = false;
	vm->ppuMode = PPU_MODE_2;
    vm->hblankDuration = 0;
	vm->ppuEnabled = true;
	vm->skipFrame = false;
	vm->fetcherState = FETCHER_SLEEP;
	vm->currentFetcherTask = 0;
    vm->fetcherTileAddress = 0;
    vm->fetcherTileAttributes = 0;
    vm->fetcherX = 0;
    vm->fetcherY = 0;
    vm->fetcherTileRowLow = 0;
    vm->fetcherTileRowHigh = 0;
    vm->lastRenderedPixelX = 0;
	/* When the PPU first starts up, it takes 4 cycles less on the first frame,
	 * it also doesnt lock OAM */
    vm->cyclesSinceLastFrame = 0;		/* 4 on DMG */
	vm->cyclesSinceLastMode = 0;		/* ^^^^^^^^ */
    vm->currentCRAMIndex = 0;

	/* Initialise FIFO */
	clearFIFO(&vm->BackgroundFIFO);

	/* bit 3-0 in joypad register is set to 1 on boot (0xCF) */
	vm->joypadSelectedMode = JOYPAD_SELECT_DIRECTION_ACTION;
	vm->joypadActionBuffer = 0xF;
	vm->joypadDirectionBuffer = 0xF;

    /* Set registers & flags to GBC specifics */
    resetGBC(vm);

	/* Set WRAM/VRAM banks if in CGB mode 
	 * Because the default value of SVBK is 0xFF, which means bank 7 is selected 
	 * by default 
	 * Same goes for VBK, bank 1 will be selected by default*/
	switchCGB_WRAM(vm, 1, 7);
	switchCGB_VRAM(vm, 0, 1);

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

/* ------------------ */ 

static void run(VM* vm) {
	/* We do input polling every 1000 cpu ticks */
	vm->ticksAtStartup = clock_u();

    while (vm->run) {
		/* Handle Events */
        handleSDLEvents(vm);

		for (int i = 0; (i < 1000) && vm->run; i++) {
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
}

/* SDL */

int initSDL(VM* vm) {
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_CreateWindowAndRenderer(WIDTH_PX * DISPLAY_SCALING, HEIGHT_PX * DISPLAY_SCALING, SDL_WINDOW_SHOWN,
                                &vm->sdl_window, &vm->sdl_renderer);

    if (!vm->sdl_window) return 1;          /* Failed to create screen */

    SDL_SetWindowTitle(vm->sdl_window, "MegaGBC");
	SDL_RenderSetScale(vm->sdl_renderer, DISPLAY_SCALING, DISPLAY_SCALING);
    return 0;
}

void handleSDLEvents(VM *vm) {
    /* We listen for events like keystrokes and window closing */
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
			/* We reset the corresponding bit for every scancode 
			 * in the buffer for keydown and set for keyup */
			switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_W:
					/* Joypad Up */
					vm->joypadDirectionBuffer &= ~(1 << 2);
					break;
				case SDL_SCANCODE_A:
					/* Joypad Left */
					vm->joypadDirectionBuffer &= ~(1 << 1);
					break;
				case SDL_SCANCODE_S:
					/* Joypad Down */
					vm->joypadDirectionBuffer &= ~(1 << 3);
					break;
				case SDL_SCANCODE_D:
					/* Joypad Right */
					vm->joypadDirectionBuffer &= ~(1 << 0);
					break;
				case SDL_SCANCODE_Z:
					/* B */
					vm->joypadActionBuffer &= ~(1 << 1);
					break;
				case SDL_SCANCODE_X:
					/* A */
					vm->joypadActionBuffer &= ~(1 << 0);
					break;
				case SDL_SCANCODE_I:
					/* Start */
					vm->joypadActionBuffer &= ~(1 << 3);
					break;
				case SDL_SCANCODE_O:
					/* Select */
					vm->joypadActionBuffer &= ~(1 << 2);
					break;
				default: return;
			}

			updateJoypadRegBuffer(vm, vm->joypadSelectedMode);

			if (vm->joypadSelectedMode != JOYPAD_SELECT_NONE) {
				/* Request joypad interrupt if atleast 1 of the modes are selected */
				requestInterrupt(vm, INTERRUPT_JOYPAD);
			}
		} else if (event.type == SDL_KEYUP && event.key.repeat == 0) {
			switch (event.key.keysym.scancode) {
				case SDL_SCANCODE_W:
					/* Joypad Up */
					vm->joypadDirectionBuffer |= 1 << 2;
					break;
				case SDL_SCANCODE_A:
					/* Joypad Left */
					vm->joypadDirectionBuffer |= 1 << 1;
					break;
				case SDL_SCANCODE_S:
					/* Joypad Down */
					vm->joypadDirectionBuffer |= 1 << 3;
					break;
				case SDL_SCANCODE_D:
					/* Joypad Right */
					vm->joypadDirectionBuffer |= 1 << 0;
					break;
				case SDL_SCANCODE_Z:
					/* B */
					vm->joypadActionBuffer |= 1 << 1;
					break;
				case SDL_SCANCODE_X:
					/* A */
					vm->joypadActionBuffer |= 1 << 0;
					break;
				case SDL_SCANCODE_I:
					/* Start */
					vm->joypadActionBuffer |= 1 << 3;
					break;
				case SDL_SCANCODE_O:
					/* Select */
					vm->joypadActionBuffer |= 1 << 2;
					break;
				default: return;			 
			}
			updateJoypadRegBuffer(vm, vm->joypadSelectedMode);

		} else if (event.type == SDL_QUIT) {
            vm->run = false;
        }
    }
}

void freeSDL(VM* vm) {
    SDL_DestroyRenderer(vm->sdl_renderer);
    SDL_DestroyWindow(vm->sdl_window);
    SDL_Quit();
}
/* ---------------------------------------- */

/* Joypad */

void updateJoypadRegBuffer(VM* vm, JOYPAD_SELECT mode) {
	switch (mode) {
		case JOYPAD_SELECT_DIRECTION_ACTION:
			/* The program has selected action and direction both,
			 * so my best guess is to just bitwise OR them and set the value */
			vm->MEM[R_P1_JOYP] &= 0xF0;
			vm->MEM[R_P1_JOYP] |= ~(~vm->joypadDirectionBuffer | ~vm->joypadActionBuffer) & 0xF;
			break;
		case JOYPAD_SELECT_ACTION:
			/* Select Action */
			vm->MEM[R_P1_JOYP] &= 0xF0;
			vm->MEM[R_P1_JOYP] |= vm->joypadActionBuffer & 0xF;

			break;
		case JOYPAD_SELECT_DIRECTION:
			/* Select Direction */
			vm->MEM[R_P1_JOYP] &= 0xF0;
			vm->MEM[R_P1_JOYP] |= vm->joypadDirectionBuffer & 0xF;

			break;
		case JOYPAD_SELECT_NONE:
			/* Select None */
			vm->MEM[R_P1_JOYP] &= 0xF0;
			break;
	}
}

/* ---------------------------------------- */ 

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
    double totalElapsed = (clock_u() - vm->ticksAtStartup) / 1e6;
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
