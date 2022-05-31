#include "../include/vm.h"
#include "../include/display.h"
#include "../include/debug.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

static void lockToFramerate(VM* vm) {
	/* The emulator keeps its speed accurate by locking to the framerate
	 * Whatever has to be done (cpu execution, audio, rendering a frame) in 
	 * the interval equivalent to 1 frame render on the gameboy is done in 1 frame
	 * render on the emulator, the remaining time is waited for on the emulator to
	 * sync with the time on the gameboy */
	unsigned long ticksElapsed = (clock_u() - vm->ticksAtStartup) - vm->ticksAtLastRender;
		
	/* Ticks elapsed is the amount of time elapsed since last frame render (in microsec),
	 * which is lesser than the amount of time it would have taken on the real gameboy
	 * because the emulator goes very fast 
	 *
	 * In special cases where the emulator needs to be able to go slower than the 
	 * gba itself (for debugging), we add a special check */

#ifdef DEBUG_SUPPORT_SLOW_EMULATION
	if (ticksElapsed < (1e6/DEFAULT_FRAMERATE)) {
#endif
		usleep((1e6/DEFAULT_FRAMERATE) - ticksElapsed);
#ifdef DEBUG_SUPPORT_SLOW_EMULATION
	}
#endif
	vm->ticksAtLastRender = clock_u() - vm->ticksAtStartup;
}

static void updateSTAT(VM* vm, STAT_UPDATE_TYPE type) {
	/* This function updates the STAT register depending on the type of update 
	 * that is requested */

#define SWITCH_MODE(mode) vm->MEM[R_STAT] &= 0x3; \
						  vm->MEM[R_STAT] |= mode

	switch (type) {
		case STAT_UPDATE_LY_LYC:
			if (vm->MEM[R_LY] == vm->MEM[R_LYC]) {
				/* Set bit 2 */
				SET_BIT(vm->MEM[R_STAT], 2);
	
				if (GET_BIT(vm->MEM[R_STAT], 6)) {
					/* If bit 6 is set, we can trigger the STAT interrupt */
					requestInterrupt(vm, INTERRUPT_LCD_STAT);
				}
			} else {
				/* Clear bit 2 */
				CLEAR_BIT(vm->MEM[R_STAT], 2);
			}
			break;
		case STAT_UPDATE_SWITCH_MODE0:
			/* Switch to mode 0 */
			SWITCH_MODE(PPU_MODE_0);

			if (GET_BIT(vm->MEM[R_STAT], 3)) {
				/* If mode 0 interrupt source is enabled */
				requestInterrupt(vm, INTERRUPT_LCD_STAT);
			}
			break;
		case STAT_UPDATE_SWITCH_MODE1:
			/* Switch to mode 1 */
			SWITCH_MODE(PPU_MODE_1);

			if (GET_BIT(vm->MEM[R_STAT], 4)) {
				requestInterrupt(vm, INTERRUPT_LCD_STAT);
			}
			break;
		case STAT_UPDATE_SWITCH_MODE2:
			SWITCH_MODE(PPU_MODE_2);

			if (GET_BIT(vm->MEM[R_STAT], 5)) {
				requestInterrupt(vm, INTERRUPT_LCD_STAT);
			}
			break;
		case STAT_UPDATE_SWITCH_MODE3:
			SWITCH_MODE(PPU_MODE_3);
			break;
	}

#undef SWITCH_MODE
}

static inline void switchModePPU(VM* vm, PPU_MODE mode) {
	vm->ppuMode = mode;
	vm->cyclesSinceLastMode = 0;

	switch (mode) {
		case PPU_MODE_2: 
			vm->lockOAM = true;
			vm->lockPalettes = false;
			vm->lockVRAM = false;
			updateSTAT(vm, STAT_UPDATE_SWITCH_MODE2);
			break;
		case PPU_MODE_3:
			vm->lockOAM = true;
			vm->lockPalettes = true;
			vm->lockVRAM = true;
			updateSTAT(vm, STAT_UPDATE_SWITCH_MODE3);
			break;
		case PPU_MODE_0:
			vm->lockOAM = false;
			vm->lockPalettes = false;
			vm->lockVRAM = false;
			updateSTAT(vm, STAT_UPDATE_SWITCH_MODE0);
			break;
		case PPU_MODE_1:
			vm->lockOAM = false;
			vm->lockPalettes = false;
			vm->lockVRAM = false;
			updateSTAT(vm, STAT_UPDATE_SWITCH_MODE1);
			break;
	}
}

static void advanceFetcher(VM* vm) {
	/* Defines the order of the tasks in the fetcher, 
	* we reuse the sleep state as a way to consume 1 dot when required
	* because the states usually take 2 dots and the fetcher is stepped every dot */

	const FETCHER_STATE fetcherTasks[7] = {
		FETCHER_SLEEP,
		FETCHER_GET_TILE,
		FETCHER_SLEEP,
		FETCHER_GET_DATA_LOW,
		FETCHER_SLEEP,
		FETCHER_SLEEP,
		FETCHER_PUSH			/* The amount of dots this takes isnt fixed */
	};

	/* Fetcher state machine to handle pixel FIFO */
	switch (fetcherTasks[vm->currentFetcherTask]) {
		case FETCHER_GET_TILE: {
			
			break;
		}
		case FETCHER_GET_DATA_LOW: {
			
			break;
		}
		case FETCHER_GET_DATA_HIGH: {
			break;
		}	
		case FETCHER_SLEEP: break;			/* Do nothing */
		case FETCHER_PUSH: {
			vm->currentFetcherTask = 0;
			break;
		}
	}

	vm->currentFetcherTask++;
}

static void advancePPU(VM* vm) {
	/* We use a state machine to handle different PPU modes */
	vm->cyclesSinceLastMode++;


	switch (vm->ppuMode) {
		case PPU_MODE_2:
			/* Do nothing for PPU mode 2 
			 *
			 * Beginning of new scanline */
			if (vm->cyclesSinceLastMode == T_CYCLES_PER_MODE2) 
				switchModePPU(vm, PPU_MODE_3);

			break;
		case PPU_MODE_3:
			/* Draw Pixels */
			advanceFetcher(vm);

			if (vm->cyclesSinceLastMode == 172)
				switchModePPU(vm, PPU_MODE_0);

			break;
		case PPU_MODE_0: {
			/* HBLANK */
			if (vm->cyclesSinceLastMode == 204) {
				/* End of scanline */
				uint8_t nextScanlineNumber = (vm->cyclesSinceLastFrame / T_CYCLES_PER_SCANLINE) + 1;
				if (nextScanlineNumber == 144) switchModePPU(vm, PPU_MODE_1);
				else switchModePPU(vm, PPU_MODE_2);
			} else if (vm->cyclesSinceLastMode == 198) {
				/* LY register gets incremented 6 dots before the *true* increment */
				vm->MEM[R_LY]++;
				updateSTAT(vm, STAT_UPDATE_LY_LYC);
			}

			break;
		}
		case PPU_MODE_1: {
			/* VBLANK */
			requestInterrupt(vm, INTERRUPT_VBLANK);	
			/* LY Resets to 0 after 4 cycles on line 153 
			 * LY=LYC Interrupt is triggered 12 cycles after line 153 begins */

			unsigned cycleAtLYReset = T_CYCLES_PER_VBLANK - T_CYCLES_PER_SCANLINE + 4;	
			unsigned cycleAtLYCInterrupt = T_CYCLES_PER_VBLANK - T_CYCLES_PER_SCANLINE + 12;
			if (cycleAtLYReset != vm->cyclesSinceLastMode && vm->cyclesSinceLastMode % 
					T_CYCLES_PER_SCANLINE == T_CYCLES_PER_SCANLINE - 6) {

				/* LY register Increments 6 cycles before *true* LY, only when
				 * the line is other than 153, because on line 153 it resets early */
				vm->MEM[R_LY]++;
				updateSTAT(vm, STAT_UPDATE_LY_LYC);
			} else if (vm->cyclesSinceLastMode == T_CYCLES_PER_VBLANK) {
				switchModePPU(vm, PPU_MODE_2);
			} else if (vm->cyclesSinceLastMode == cycleAtLYReset) {
				vm->MEM[R_LY] = 0;
			} else if (vm->cyclesSinceLastMode == cycleAtLYCInterrupt) {
				/* The STAT update can be issued now */
				updateSTAT(vm, STAT_UPDATE_LY_LYC);
			}
			break;
		}
	}
}

/* PPU Enable & Disable */

void enablePPU(VM* vm) {
	vm->ppuEnabled = true;

	switchModePPU(vm, PPU_MODE_2);

	/* Skip first frame after LCD is turned on */
	vm->skipFrame = true;
}

void disablePPU(VM* vm) {
	if (vm->ppuMode != PPU_MODE_1) {
		/* This is dangerous for any game/rom to do on real hardware
		 * as it can damage hardware */
		printf("[WARNING] Turning off LCD when not in VBlank can damage a real gameboy\n");
	}

	vm->ppuEnabled = false;
	vm->MEM[R_LY] = 0;

	/* Set STAT mode to 0 */
	switchModePPU(vm, PPU_MODE_0);
	
	/* Reset frame */
	vm->cyclesSinceLastFrame = 0;

	/* Make the screen go black */
	SDL_SetRenderDrawColor(vm->sdl_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(vm->sdl_renderer);
	SDL_RenderPresent(vm->sdl_renderer);
}

/* -------------------- */

void clearFIFO(FIFO *fifo) {
	fifo->count = 0;
	fifo->nextPopIndex = 0;
	fifo->nextPushIndex = 0;
}

void pushFIFO(FIFO* fifo, FIFO_Pixel pixel) {
	if (fifo->count == FIFO_MAX_COUNT) {
		printf("FIFO Error : Pushing beyond max limit\n");
		return;
	}

	fifo->contents[fifo->nextPushIndex] = pixel;
	fifo->count++;
	fifo->nextPushIndex++;

	if (fifo->nextPushIndex == FIFO_MAX_COUNT) {
		/* Wrap it around */
		fifo->nextPushIndex = 0;
	}
}

FIFO_Pixel popFIFO(FIFO* fifo) {
	FIFO_Pixel pixel;

	if (fifo->count == 0) {
		printf("FIFO Error : Popping when no pixels are pushed\n");
		return pixel;
	}

	pixel = fifo->contents[fifo->nextPopIndex];
	fifo->count--;
	fifo->nextPopIndex++;

	if (fifo->nextPopIndex == FIFO_MAX_COUNT) {
		fifo->nextPopIndex = 0;
	}

	return pixel;
}

void syncDisplay(VM* vm, unsigned int cycles) {
    /* We sync the display by running the PPU for the correct number of 
	 * dots (1 dot = 1 tcycle in normal speed) */

	/* About the infinite loop, it seems like im calling lockToFramerate every cycle 
	 * instead of once per frame. Just keep incrementing cycles per frame to keep
	 * track of frames and in the end reset it when ppu is enabled again */
	printf("bro\n");
	if (!vm->ppuEnabled) {
		printf("oof\n");
		lockToFramerate(vm);
		return;
	}

	for (unsigned int i = 0; i < cycles; i++) {
		vm->cyclesSinceLastFrame++;
		
		advancePPU(vm);	

		if (vm->cyclesSinceLastFrame == T_CYCLES_PER_FRAME) {
			/* End of frame */
			vm->cyclesSinceLastFrame = 0;	
			/* Draw frame */

			if (vm->skipFrame) {
				vm->skipFrame = false;
			} else {
				SDL_RenderPresent(vm->sdl_renderer);
			}
			lockToFramerate(vm);
		} 
	}
}
