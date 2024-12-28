#include <gb/cartridge.h>
#include <gb/gb.h>
#include <gb/cpu.h>
#include <gb/debug.h>
#include <gb/display.h>
#include <gb/mbc.h>
#include <gb/gui.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_timer.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <sys/time.h>

static void initGB(GB* gb) {
    gb->cartridge = NULL;
    gb->emuMode = EMU_DMG;
    gb->wram = NULL;
    gb->vram = NULL;
    gb->selectedVRAMBank = 0;
    gb->selectedWRAMBank = 0;
    gb->memController = NULL;
    gb->memControllerType = MBC_NONE;
    gb->run = false;
    gb->paused = false;

    gb->scheduleInterruptEnable = false;
    gb->haltMode = false;
    gb->scheduleHaltBug = false;
    gb->scheduleDMA = false;

    gb->clock = 0;
    gb->lastTIMASync = 0;
    gb->lastDIVSync = 0;

	gb->doingSpeedSwitch = false;
	gb->isDoubleSpeedMode = false;
	gb->scheduleGDMA = false;
	gb->scheduleHDMA = false;
	gb->doingGDMA = false;
	gb->doingHDMA = false;
	gb->stepHDMA = false;
	gb->ghdmaSource = 0;
	gb->ghdmaDestination = 0;
	gb->ghdmaLength = 0;
	gb->ghdmaIndex = 0;

	gb->imgui_main_context = NULL;
	gb->imgui_secondary_context = NULL;
	gb->imgui_secondary_sdl_window = NULL;
	gb->imgui_secondary_sdl_renderer = NULL;
	gb->imgui_gui_state = NULL;

    gb->sdl_window = NULL;
    gb->sdl_renderer = NULL;
    gb->ticksAtLastRender = 0;
    gb->ticksAtStartup = 0;

    gb->ppuMode = PPU_MODE_2;
    gb->hblankDuration = 0;
    gb->ppuEnabled = true;
    gb->skipFrame = false;
    gb->firstTileInScanline = true;
    gb->doOptionalPush = false;
    gb->currentFetcherTask = 0;
    gb->fetcherTileAddress = 0;
    gb->fetcherTileAttributes = 0;
    gb->fetcherX = 0;
    gb->fetcherY = 0;
    gb->fetcherTileRowLow = 0;
    gb->fetcherTileRowHigh = 0;
    gb->nextRenderPixelX = 0;
    gb->nextPushPixelX = 0;
    gb->pauseDotClock = 0;
    gb->pixelsToDiscard = 0;
    gb->currentBackgroundCRAMIndex = 0;
    gb->currentSpriteCRAMIndex = 0;
    gb->windowYCounter = 0;
    gb->lyWasWY = false;
    gb->renderingWindow = false;
    gb->renderingSprites = false;
    gb->spriteData = NULL;
    gb->preservedFetcherTileLow = 0;
    gb->preservedFetcherTileHigh = 0;
    gb->preservedFetcherTileAttributes = 0;
    gb->spriteSize = 0;
    gb->isLastSpriteOverlap = false;
    gb->lastSpriteOverlapPushIndex = 0;
    gb->lastSpriteOverlapX = 0;
    gb->doingDMA = false;
    gb->mCyclesSinceDMA = 0;
    gb->scheduled_dmaSource = 0;
    gb->dmaSource = 0;
    gb->scheduled_dmaTimer = 0;

    /* Initialise OAM Buffer */
    memset(&gb->oamDataBuffer, 0xFF, 50);
    gb->spritesInScanline = 0;

    /* Initialise FIFO */
    clearFIFO(&gb->BackgroundFIFO);
    clearFIFO(&gb->OAMFIFO);

    /* bit 3-0 in joypad register is set to 1 on boot (0xCF) */
    gb->joypadSelectedMode = JOYPAD_SELECT_DIRECTION_ACTION;
    gb->joypadActionBuffer = 0xF;
    gb->joypadDirectionBuffer = 0xF;
}

static void initGBCartridge(GB* gb, Cartridge* cartridge) {
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

    gb->cartridge = cartridge;
    gb->cartridge->inserted = true;

    if (cartridge->cgbCode == CGB_MODE || cartridge->cgbCode == CGB_DMG_MODE) {
        gb->emuMode = EMU_CGB;
    } else if (cartridge->cgbCode == DMG_MODE) {
        gb->emuMode = EMU_DMG;
    }

    if (gb->emuMode == EMU_CGB) {
        gb->lockVRAM = false;
        gb->lockOAM = true;
        gb->lockPalettes = false;
        gb->cyclesSinceLastFrame = 0;
        gb->cyclesSinceLastMode = 0;

        /* CGB needs WRAM, VRAM and CRAM banks allocated */
        gb->wram = (uint8_t*)malloc(0x1000 * 8);            /* 8 WRAM banks */
        gb->vram = (uint8_t*)malloc(0x2000 * 2);            /* 2 VRAM banks */
        gb->bgColorRAM = (uint8_t*)malloc(64);
        gb->spriteColorRAM = (uint8_t*)malloc(64);

        if (gb->wram == NULL || gb->vram == NULL || gb->bgColorRAM == NULL ||
                gb->spriteColorRAM == NULL) {

            log_fatal(gb, "[FATAL] Could not allocate space for CGB WRAM/VRAM/CRAM\n");
            return;
        }

        /* Set registers & flags to GBC specifics */
        resetGBC(gb);

        /* Set WRAM/VRAM banks if in CGB mode
         * Because the default value of SVBK is 0xF9, which means bank 1 is selected
         * by default
         * Same goes for VBK, bank 0 will be selected by default*/
        gb->selectedVRAMBank = 0;
        gb->selectedWRAMBank = 1;
    } else if (gb->emuMode == EMU_DMG) {
        gb->wram = (uint8_t*)malloc(0x1000 * 2);        /* 2 WRAM banks */
        gb->vram = (uint8_t*)malloc(0x2000 * 1);        /* 1 VRAM bank */

        /* These values are constant throughout */
        gb->selectedVRAMBank = 0;
        gb->selectedWRAMBank = 1;

        if (gb->wram == NULL || gb->vram == NULL) {
            log_fatal(gb, "[FATAL] Could not allocate space for DMG WRAM/VRAM\n");
            return;
        }

        /* When the PPU first starts up, it takes 4 cycles less on the first frame,
         * it also doesnt lock OAM */
        gb->lockOAM = false;
        gb->lockPalettes = false;
        gb->lockVRAM = false;
        gb->cyclesSinceLastFrame = 4;		/* 4 on DMG */
        gb->cyclesSinceLastMode = 4;		/* ^^^^^^^^ */
        gb->bgColorRAM = NULL;
        gb->spriteColorRAM = NULL;

        resetGB(gb);
    }
}

static void bootROM(GB* gb) {
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
    bool logoVerified = memcmp(&gb->cartridge->logoChecksum, &logo, 0x18) == 0;

    if (!logoVerified) {
        log_fatal(gb, "Logo Verification Failed");
    }

    int checksum = 0;
    for (int i = 0x134; i <= 0x14C; i++) {
        checksum = checksum - gb->cartridge->allocated[i] - 1;
    }

    if ((checksum & 0xFF) != gb->cartridge->headerChecksum) {
        log_fatal(gb, "Header Checksum Doesn't Match, it is possibly corrupted");
    }
#endif
}

/* Utility */
unsigned long clock_u() {
    /* Function to get time with microsecod precision */
    struct timeval t;
    gettimeofday(&t, NULL);

    return (t.tv_sec * 1e6 + t.tv_usec);
}

/* Timer */

void incrementTIMA(GB* gb) {
    uint8_t old = gb->IO[R_TIMA];

    if (old == 0xFF) {
        /* Overflow */
        gb->IO[R_TIMA] = gb->IO[R_TMA];
        requestInterrupt(gb, INTERRUPT_TIMER);
    } else {
        gb->IO[R_TIMA]++;
    }
}

void syncTimer(GB* gb) {
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

    unsigned int cycles = gb->clock;
    unsigned int cyclesElapsedDIV = cycles - gb->lastDIVSync;


    /* Sync DIV (DIV does not tick during speed switch)*/
    if (!gb->doingSpeedSwitch && cyclesElapsedDIV >= T_CYCLES_PER_DIV) {
        /* 'Rewind' the last timer sync in case the timer should have been
         * incremented on an earlier cycle */
        gb->lastDIVSync = cycles - (cyclesElapsedDIV - T_CYCLES_PER_DIV);
        gb->IO[R_DIV]++;
    }

    /* Sync TIMA */
    unsigned int cyclesElapsedTIMA = cycles - gb->lastTIMASync;
    uint8_t timerControl    =  gb->IO[R_TAC];
    uint8_t timerEnabled    =  (timerControl >> 2) & 1;
    uint8_t timerFrequency  =  timerControl & 0b00000011;

    if (timerEnabled) {
        /* Cycle table contains number of cycles per increment for its corresponding freq
		 * (as per single speed mode) */
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

            gb->lastTIMASync = cycles - rem;

            for (int i = 0; i < increments; i++) {
                incrementTIMA(gb);
            }
        }
    }
}

/* DMA Transfers */
void scheduleDMATransfer(GB* gb, uint8_t byte) {
    if (byte > 0xDF) {
#ifdef DEBUG_LOGGING
        printf("[WARNING] Starting DMA Transfer with address > DFXX, wrapping to DFXX\n");
#endif
        byte = 0xDF;
    }

    uint16_t address = byte * 0x100;

    gb->scheduled_dmaSource = address;
    /* Schedule DMA to begin on the next mcycle (4 tcycles for the current, 4 for next */
    gb->scheduled_dmaTimer = 8;
    gb->scheduleDMA = true;
}

static void startDMATransfer(GB* gb) {
    gb->scheduleDMA = false;
    gb->scheduled_dmaTimer = 0;
    gb->mCyclesSinceDMA = 0;
    gb->dmaSource = gb->scheduled_dmaSource;

    gb->doingDMA = true;
}

static void syncDMA(GB* gb) {
    /* DMA Transfers take 160 machine cycles to complete = 640 T-Cycles
     *
     * It needs to be done sequentially sprite by sprite as it is possible to do dma
     * transfers during mode 2, which can cause the values to be read by the ppu in real time
     *
     * Calling this function once does 4 tcycles or 1 mcycle of syncing */

    gb->mCyclesSinceDMA++;

    if (gb->mCyclesSinceDMA % 4 == 0) {
        /* it takes 4 M-Cycles to load 1 sprite, as there are 40 OAM entries and 160 M-Cycles
         * in total */

        uint8_t currentSpriteIndex = (gb->mCyclesSinceDMA / 4) - 1;
        uint8_t addressLow = currentSpriteIndex * 4;

        for (int i = 0; i < 4; i++) {
            gb->OAM[addressLow + i] = readAddr(gb, gb->dmaSource + addressLow + i);
        }
    }

    if (gb->mCyclesSinceDMA == 160) {
        gb->dmaSource = 0;
        gb->mCyclesSinceDMA = 0;
        gb->doingDMA = false;
    }
}

/* GDMA/HDMA */

void scheduleGDMATransfer(GB *gb, uint16_t source, uint16_t dest, uint8_t length) {
	if (gb->doingHDMA) {
		printf("[WARNING] Doing GDMA and HDMA together\n");
	}
	/* Carry out scheduling for general purpose dma which will start at next cycle */
	gb->ghdmaDestination = dest;
	gb->ghdmaSource = source;
	gb->ghdmaLength = length;
	gb->ghdmaIndex = 0;
	gb->scheduleGDMA = true;
#ifdef DEBUG_GHDMA_LOGGING
	printf("Scheduled GDMA Transfer: S:0x%04x D:0x%04x L:%x\n", source, dest, length);
#endif
}

static void startGDMATransfer(GB* gb) {
	/* During GDMA Transfer, CPU is stopped, interrupts are stopped,
	 * timer, PPU and others continue as usual */
	gb->scheduleGDMA = false;
	gb->doingGDMA = true;
	if (gb->isDoubleSpeedMode) {
		int cycles = gb->ghdmaLength * 0x10;
		for (int i = 0; i < cycles; i++) {
			/* Fixed rate of 1 byte per cycle for 16 mcycles */
			uint8_t byte = readAddr(gb, gb->ghdmaSource+i);
			writeAddr(gb, gb->ghdmaDestination+i, byte);
			cyclesSync_4(gb);
			syncTimer(gb);
		}
	} else {
		int cycles = gb->ghdmaLength * 0x8;
		for (int i = 0; i < cycles; i++) {
			/* Fixed rate of 2 bytes per cycle for 8 mcycles */
			uint8_t byte = readAddr(gb, gb->ghdmaSource+i*2);
			writeAddr(gb, gb->ghdmaDestination+i*2, byte);
			byte = readAddr(gb, gb->ghdmaSource+i*2+1);
			writeAddr(gb, gb->ghdmaDestination+i*2+1, byte);

			cyclesSync_4(gb);
			syncTimer(gb);
		}
	}

	/* GDMA complete */
	gb->doingGDMA = false;
	gb->IO[R_HDMA5] = 0xFF;
#ifdef DEBUG_GHDMA_LOGGING
	printf("Successfully completed GDMA Transfer\n");
#endif
}

void scheduleHDMATransfer(GB *gb, uint16_t source, uint16_t dest, uint8_t length) {
	if (gb->doingHDMA) return;
	/* Carry out scheduling for HBlank dma which will be allowed to run from next cycle */
	gb->ghdmaDestination = dest;
	gb->ghdmaSource = source;
	gb->ghdmaLength = length;
	gb->ghdmaIndex = 0;
	gb->scheduleHDMA = true;
#ifdef DEBUG_GHDMA_LOGGING
	printf("Scheduled HDMA Transfer: S:0x%04x D:0x%04x L:%x\n", source, dest, length);
#endif
}

static void startHDMATransfer(GB* gb) {
	gb->scheduleHDMA = false;
	gb->doingHDMA = true;
}

static void stepHDMATransfer(GB* gb) {
	/* Should be called in HBlank when doing HDMA transfer, 
	 * Steps the HDMA by 1 block, during the process CPU is stopped, interrupts are stopped,
	 * PPU and Timer tick normally */

	/* Step acknowledged */
	gb->stepHDMA = false;

	/* If halting then skip step, this means it steps in next HBlank
	 * in which the CPU isnt halting */
	if (gb->haltMode) return;

	/* Step can be made, 1 block of 0x10 bytes is transferred, CPU & Interrupts are stopped
	 * PPU and Timer ticks as usual */

	const uint16_t offset = 0x10*gb->ghdmaIndex;
	if (gb->isDoubleSpeedMode) {
		int cycles = 0x10;
		for (int i = 0; i < cycles; i++) {
			/* 1 byte per 1 cycle for 16 cycles */
			uint8_t byte = readAddr(gb, gb->ghdmaSource+offset+i);
			writeAddr(gb, gb->ghdmaDestination+offset+i, byte);

			cyclesSync_4(gb);
			syncTimer(gb);
		}
	} else {
		int cycles = 0x8;
		for (int i = 0; i < cycles; i++) {
			/* 2 bytes per 1 cycle for 8 cycles */
			uint8_t byte = readAddr(gb, gb->ghdmaSource+offset+i*2);
			writeAddr(gb, gb->ghdmaDestination+offset+i*2, byte);
			byte = readAddr(gb, gb->ghdmaSource+offset+i*2+1);
			writeAddr(gb, gb->ghdmaDestination+offset+i*2+1, byte);

			cyclesSync_4(gb);
			syncTimer(gb);
		}
	}

	gb->ghdmaIndex += 1;

	if (gb->ghdmaIndex >= gb->ghdmaLength) {
		/* HDMA Transfer Complete */
		gb->doingHDMA = false;
		gb->IO[R_HDMA5] = 0xFF;
#ifdef DEBUG_GHDMA_LOGGING
		printf("Successfully completed hdma\n");
#endif
		return;
	}

#ifdef DEBUG_GHDMA_LOGGING
	printf("Stepped HDMA: Index:%d Length:%d\n", gb->ghdmaIndex, gb->ghdmaLength);
#endif
	gb->IO[R_HDMA5] = 0x80 | (gb->ghdmaLength - gb->ghdmaIndex);
}

void cancelHDMATransfer(GB* gb) {
	/* Cancel an ongoing HDMA transfer */
	gb->doingHDMA = false;
	gb->IO[R_HDMA5] = 0x80 | (gb->ghdmaLength - gb->ghdmaIndex);
#ifdef DEBUG_GHDMA_LOGGING
	printf("Cancelled HDMA\n");
#endif
}

/* ------------------ */

static void run(GB* gb) {
    /* We do input polling every 1000 cpu ticks */
    gb->ticksAtStartup = clock_u();

    for (;gb->run;) {
        /* Handle Events */
        dispatch(gb);
    }
}

void cyclesSync_4(GB* gb) {
    /* This function is called millions of times by the CPU
     * in a second and therefore it needs to be optimised
     *
     * So we dont update all hardware but only the ones that need to
     * always be upto date like the display and DMA/HDMA
     *
     */
    gb->clock += 4;

    syncDisplay(gb);

    if (gb->doingDMA) syncDMA(gb);
    if (gb->scheduleDMA) {
        gb->scheduled_dmaTimer -= 4;

        /* The DMA has been scheduled to start 1 mcycle after the register write */
        if (gb->scheduled_dmaTimer == 0) startDMATransfer(gb);
    } else if (gb->scheduleGDMA) {
		/* GDMA can be started at the end of the register write cycle */
		startGDMATransfer(gb);
	} else if (gb->scheduleHDMA) {
		startHDMATransfer(gb);
	}

	if (gb->doingHDMA && gb->stepHDMA) {
		stepHDMATransfer(gb);
	}
}

/* SDL */

int initSDL(GB* gb) {
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_CreateWindowAndRenderer(WIDTH_PX * DISPLAY_SCALING, HEIGHT_PX * DISPLAY_SCALING + MENU_HEIGHT_PX, SDL_WINDOW_SHOWN,
            &gb->sdl_window, &gb->sdl_renderer);

    if (!gb->sdl_window) return 1;          /* Failed to create screen */
	SDL_SetWindowPosition(gb->sdl_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    SDL_SetWindowTitle(gb->sdl_window, "MegaGB");
    return 0;
}

void handleSDLEvents(GB* gb) {
    /* We listen for events like keystrokes and window closing */
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
		processEventsIMGUI(gb, &event);
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            /* We reset the corresponding bit for every scancode
             * in the buffer for keydown and set for keyup */
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_UP:
                    /* Joypad Up */
                    CLEAR_BIT(gb->joypadDirectionBuffer, 2);
                    break;
                case SDL_SCANCODE_LEFT:
                    /* Joypad Left */
                    CLEAR_BIT(gb->joypadDirectionBuffer, 1);
                    break;
                case SDL_SCANCODE_DOWN:
                    /* Joypad Down */
                    CLEAR_BIT(gb->joypadDirectionBuffer, 3);
                    break;
                case SDL_SCANCODE_RIGHT:
                    /* Joypad Right */
                    CLEAR_BIT(gb->joypadDirectionBuffer, 0);
                    break;
                case SDL_SCANCODE_Z:
                    /* B */
                    CLEAR_BIT(gb->joypadActionBuffer, 1);
                    break;
                case SDL_SCANCODE_X:
                    /* A */
                    CLEAR_BIT(gb->joypadActionBuffer, 0);
                    break;
                case SDL_SCANCODE_RETURN:
                    /* Start */
                    CLEAR_BIT(gb->joypadActionBuffer, 3);
                    break;
                case SDL_SCANCODE_TAB:
                    /* Select */
                    CLEAR_BIT(gb->joypadActionBuffer, 2);
                    break;
                case SDL_SCANCODE_SPACE:
                    if (!gb->paused) pauseGBEmulator(gb);
                    else unpauseGBEmulator(gb);
                    break;
                default: return;
            }

            updateJoypadRegBuffer(gb, gb->joypadSelectedMode);

            if (gb->joypadSelectedMode != JOYPAD_SELECT_NONE) {
                /* Request joypad interrupt if atleast 1 of the modes are selected */
                requestInterrupt(gb, INTERRUPT_JOYPAD);
            }
        } else if (event.type == SDL_KEYUP && event.key.repeat == 0) {
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_UP:
                    /* Joypad Up */
                    SET_BIT(gb->joypadDirectionBuffer, 2);
                    break;
                case SDL_SCANCODE_LEFT:
                    /* Joypad Left */
                    SET_BIT(gb->joypadDirectionBuffer, 1);
                    break;
                case SDL_SCANCODE_DOWN:
                    /* Joypad Down */
                    SET_BIT(gb->joypadDirectionBuffer, 3);
                    break;
                case SDL_SCANCODE_RIGHT:
                    /* Joypad Right */
                    SET_BIT(gb->joypadDirectionBuffer, 0);
                    break;
                case SDL_SCANCODE_Z:
                    /* B */
                    SET_BIT(gb->joypadActionBuffer, 1);
                    break;
                case SDL_SCANCODE_X:
                    /* A */
                    SET_BIT(gb->joypadActionBuffer, 0);
                    break;
                case SDL_SCANCODE_RETURN:
                    /* Start */
                    SET_BIT(gb->joypadActionBuffer, 3);
                    break;
                case SDL_SCANCODE_TAB:
                    /* Select */
                    SET_BIT(gb->joypadActionBuffer, 2);
                    break;
                default: return;
            }
            updateJoypadRegBuffer(gb, gb->joypadSelectedMode);

        } else if (event.type == SDL_QUIT) {
            gb->run = false;
        } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(gb->sdl_window)) {
			gb->run = false;
		}
    }
}

void freeSDL(GB* gb) {
    SDL_DestroyRenderer(gb->sdl_renderer);
    SDL_DestroyWindow(gb->sdl_window);
    SDL_Quit();
}
/* ---------------------------------------- */

/* Joypad */

void updateJoypadRegBuffer(GB* gb, JOYPAD_SELECT mode) {
    switch (mode) {
        case JOYPAD_SELECT_DIRECTION_ACTION:
            /* The program has selected action and direction both,
             * so my best guess is to just bitwise OR them and set the value */
            gb->IO[R_P1_JOYP] &= 0xF0;
            gb->IO[R_P1_JOYP] |= ~(~gb->joypadDirectionBuffer | ~gb->joypadActionBuffer) & 0xF;
            break;
        case JOYPAD_SELECT_ACTION:
            /* Select Action */
            gb->IO[R_P1_JOYP] &= 0xF0;
            gb->IO[R_P1_JOYP] |= gb->joypadActionBuffer & 0xF;

            break;
        case JOYPAD_SELECT_DIRECTION:
            /* Select Direction */
            gb->IO[R_P1_JOYP] &= 0xF0;
            gb->IO[R_P1_JOYP] |= gb->joypadDirectionBuffer & 0xF;

            break;
        case JOYPAD_SELECT_NONE:
            /* Select None */
            gb->IO[R_P1_JOYP] &= 0xF0;
            break;
    }
}

/* ---------------------------------------- */

void startGBEmulator(Cartridge* cartridge) {
    GB gb;
    initGB(&gb);
    initGBCartridge(&gb, cartridge);
    /* Start up SDL */
    int status = initSDL(&gb);
    if (status != 0) {
        /* An error occurred and SDL wasnt started
         *
         * This is fatal as our emulator cannot run without it
         * and we immediately quit */
        log_fatal(&gb, "Error Starting SDL2");
        return;
    }

	/* Start up IMGUI */
	int status2 = initIMGUI(&gb);
	if (status2 != 0) {
		log_fatal(&gb, "Error starting IMGUI");
		return;
	}

#ifdef DEBUG_PRINT_CARTRIDGE_INFO
    printCartridge(cartridge);
#endif
    char title[30];
    sprintf(title, "MegaGBC | %s", gb.cartridge->title);

    SDL_SetWindowTitle(gb.sdl_window, title);
#ifdef DEBUG_LOGGING
    printf("Emulation mode: %s\n", gb.emuMode == EMU_CGB ? "Gameboy Color" : gb.emuMode == EMU_DMG ? "Gameboy" : "");
    printf("Booting into ROM\n");
#endif
    bootROM(&gb);
#ifdef DEBUG_LOGGING
    printf("Setting up Memory Bank Controller\n");
#endif
    mbc_allocate(&gb);

    /* We are now ready to run */
    gb.run = true;

    run(&gb);
    stopGBEmulator(&gb);
}

void pauseGBEmulator(GB* gb) {
    if (gb->paused) return;
    gb->paused = true;

    while (true) {
        handleSDLEvents(gb);
		renderFrameIMGUI(gb);
        if (!gb->paused) break;
    }
}

void unpauseGBEmulator(GB* gb) {
    if (!gb->paused) return;

    gb->paused = false;
}

void stopGBEmulator(GB* gb) {
#ifdef DEBUG_LOGGING
    double totalElapsed = (clock_u() - gb->ticksAtStartup) / 1e6;
    unsigned long long ticksPerSec = round(gb->clock / totalElapsed);

    printf("Ticks Per Second : %llu, x%g faster than normal speed\n", ticksPerSec, (double)ticksPerSec/(double)T_CYCLES_PER_SEC);
    printf("Time Elapsed : %g\n", totalElapsed);
    printf("Stopping Emulator Now\n");
    printf("Cleaning allocations\n");
#endif

	/* Free up IMGUI allocations */
	freeIMGUI(gb);
    /* Free up all SDL allocations and stop it */
    freeSDL(gb);
    /* Free up MBC allocations */
    mbc_free(gb);

    free(gb->wram);
    free(gb->vram);

    if (gb->emuMode == EMU_CGB) {
        /* Free memory allocated specifically for CGB */
        free(gb->bgColorRAM);
        free(gb->spriteColorRAM);
    }

    /* Reset GB */
    initGB(gb);
}
