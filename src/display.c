#include "../include/vm.h"
#include "../include/display.h"
#include "../include/debug.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
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

static inline void switchModePPU(VM* vm, PPU_MODE mode) {
	vm->ppuMode = mode;
	vm->cyclesSinceLastMode = 0;
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
			}

			break;
		}
		case PPU_MODE_1: {
			/* VBLANK */
			if (vm->cyclesSinceLastMode == T_CYCLES_PER_VBLANK) 
				switchModePPU(vm, PPU_MODE_2);
			break;
		}
		default: return;
	}
}

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

	for (unsigned int i = 0; i < cycles; i++) {
		vm->cyclesSinceLastFrame++;
		
		advancePPU(vm);

		if (vm->cyclesSinceLastFrame == T_CYCLES_PER_FRAME) {
			/* End of frame */
			vm->cyclesSinceLastFrame = 0;
			lockToFramerate(vm);
		}
 
	}
}
