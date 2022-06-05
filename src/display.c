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
#include <stdint.h>

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
	 * gb itself (for debugging), we add a special check */

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

static uint8_t* getCurrentFetcherTileData(VM* vm) {
    /* Returns the tile data pointer (first of the 16 bytes) of the tile the fetcher is currently
     * on by reading state from the emulator */

    uint16_t tileMapBaseAddress = GET_BIT(vm->MEM[R_LCDC], 3) ? 0x1C00 : 0x1800; 
    uint8_t useVramBank1 = GET_BIT(vm->fetcherTileAttributes, 3);
                
    /* Each row takes 2 bytes from the tile data of every tile per scanline
     * which 2 bytes are taken is determined by the Y position of the fetcher,
     * since each tile has a height of 8 pixels */
            
    uint8_t* vramBankPointer = NULL;
    uint8_t* vramBank0Pointer = (vm->MEM[R_VBK] == 0xFF) ? (uint8_t*)&vm->vramBank : &vm->MEM[VRAM_N0_8KB];

    /* Select which pointer to use to fetch tile data */
    if (useVramBank1) {
        /* Tile data from vram bank 1 */
        vramBankPointer = (vm->MEM[R_VBK] == 0xFF) ? &vm->MEM[VRAM_N0_8KB] : (uint8_t*)&vm->vramBank;
    } else {
        vramBankPointer = vramBank0Pointer;
    }
            
    uint8_t tileIndex = vramBank0Pointer[vm->fetcherTileAddress];
    /* Find out the addressing method to use */
            
    bool method8k = GET_BIT(vm->MEM[R_LCDC], 4);

    /* Use the tile index to get a pointer to the first byte of the 16 byte tile data */
    uint8_t* tileData = NULL; 

    if (method8k) {
        /* $8000 method */
        tileData = vramBankPointer + (tileIndex * 16);
    } else {
        /* $8800 method */
        if (tileIndex < 128) {
            /* Use $9000 as base pointer */
            tileData = vramBankPointer + 0x1000 + (tileIndex * 16);
        } else {
            /* Use $8800 as base pointer */
            tileData = vramBankPointer + 0x800 + (tileIndex * 16);
        }
    }

    return tileData;
}

static inline uint8_t toRGB888(uint8_t rgb555) {
    /* Input can be red, green or blue value of rgb 555 */
    return (rgb555 << 5) | (rgb555 >> 2);
}

static void renderPixel(VM* vm) {
    if (!vm->ppuEnabled) return;
    if (vm->BackgroundFIFO.count == 0) return;

    FIFO_Pixel pixel = popFIFO(&vm->BackgroundFIFO);
   
/*
    for (int i = 0; i < 64; i++) {
        printf("%02x ", vm->colorRAM[i]);
    }


    printf("\n");
*/

    uint8_t* palettePointer = &vm->colorRAM[pixel.colorPalette * 8];
 
    /* Color is stored as little endian rgb555 */
    uint8_t colorLow = palettePointer[pixel.colorID * 2];
    uint8_t colorHigh = palettePointer[(pixel.colorID * 2) + 1];
    // printf("ci %d cl %02x ch %02x cp %02x x %03d y %03d\n", pixel.colorID, colorLow, colorHigh, pixel.colorPalette, pixel.screenX, pixel.screenY);
    uint16_t color = (colorHigh << 8) + colorLow;

    /* We need to convert these values to rgb888 
     * Source for conversion : https://stackoverflow.com/questions/4409763/how-to-convert-from-rgb555-to-rgb888-in-c*/


    uint8_t r = toRGB888(color & 0b0000000000011111);
    uint8_t g = toRGB888(color & 0b0000001111100000 >> 5);
    uint8_t b = toRGB888(color & 0b0111110000000000 >> 10);
    
    // printf("c%04x p%d\n", color, pixel.colorPalette);
    SDL_SetRenderDrawColor(vm->sdl_renderer, r, g, b, 255);
    SDL_RenderDrawPoint(vm->sdl_renderer, pixel.screenX, pixel.screenY);

    vm->lastRenderedPixelX = pixel.screenX;

    // printf("rendered pixel at x%d\n", vm->lastRenderedPixelX);
}

static void pushPixels(VM* vm) {
    uint8_t tileDataLow = vm->fetcherTileRowLow;
    uint8_t tileDataHigh = vm->fetcherTileRowHigh;

    /* Push pixels to FIFO */
    for (int i = 1; i <= 8; i++) {
        FIFO_Pixel pixel;
                
        /* Set color palette */
        pixel.colorPalette = vm->fetcherTileAttributes & 0b00000111;

        uint8_t higherBit = GET_BIT(tileDataHigh, (8 - i));
        uint8_t lowerBit = GET_BIT(tileDataLow, (8 - i));
        /* Set color ID */
        // printf("ci %02d ", (lowerBit << 1) | higherBit);
        pixel.colorID = (lowerBit << 1) | higherBit;
        pixel.screenX = (vm->fetcherX * 8) + i - 1;
        pixel.screenY = vm->fetcherY;
        pushFIFO(&vm->BackgroundFIFO, pixel);

    }
    // printf("pushed 8 pixels %03d-%03d\n", vm->fetcherX * 8, vm->fetcherX * 8 + 7); 
    vm->fetcherX++;
}

static void advanceFetcher(VM* vm) {
	/* Defines the order of the tasks in the fetcher, 
	* we reuse the sleep state as a way to consume 1 dot when required
	* because the states usually take 2 dots and the fetcher is stepped every dot */

	const FETCHER_STATE fetcherTasks[8] = {
		FETCHER_SLEEP,
		FETCHER_GET_TILE,
		FETCHER_SLEEP,
		FETCHER_GET_DATA_LOW,
        FETCHER_SLEEP,
        FETCHER_GET_DATA_HIGH,
        FETCHER_PUSH,
		FETCHER_OPTIONAL_PUSH			
        /* The push is attempted on the first dot, if it succeeds, the second dot is slept 
         * otherwise the push is retried on the second dot and then it indefinitely retries till
         * the push can be made
        */
	};

	/* Fetcher state machine to handle pixel FIFO */
	switch (fetcherTasks[vm->currentFetcherTask]) {
		case FETCHER_GET_TILE: {
            uint16_t tileMapBaseAddress;
            
            /* There are two tile maps, check which one to use */
            if (GET_BIT(vm->MEM[R_LCDC], 3)) {
                tileMapBaseAddress = 0x1C00; 
            } else {
                tileMapBaseAddress = 0x1800;
            }
            
            uint8_t x = vm->fetcherX;
            uint8_t y = vm->fetcherY;

            vm->fetcherTileAddress = tileMapBaseAddress + x + (y / 8) * 32;
            vm->fetcherTileAttributes = vm->MEM[R_VBK] == 0xFF ? vm->MEM[VRAM_N0_8KB + vm->fetcherTileAddress] :
                                                                 vm->vramBank[vm->fetcherTileAddress];
            // printf("taddr %04x, tindx %02x, tattr %02x, x %d, y %d \n", vm->fetcherTileAddress, vm->MEM[VRAM_N0_8KB + vm->fetcherTileAddress], vm->fetcherTileAttributes, x, y);
            vm->currentFetcherTask++;
			break;
		}
		case FETCHER_GET_DATA_LOW: {
            uint8_t* tileData = getCurrentFetcherTileData(vm);
            
            /* Now retrieve the lower byte of this row */
            bool verticallyFlipped = GET_BIT(vm->fetcherTileAttributes, 6);
            uint8_t currentRowInTile = vm->fetcherY % 8;
            
            /* If the tile is flipped, we can get the vertically opposite row in the tile */
            if (verticallyFlipped) currentRowInTile = 8 - currentRowInTile;
            vm->fetcherTileRowLow = tileData[2 * currentRowInTile];
            
            vm->currentFetcherTask++;
			break;
		}
		case FETCHER_GET_DATA_HIGH: {
            /* Do the same as above but instead get the higher byte */
            uint8_t* tileData = getCurrentFetcherTileData(vm);
            bool verticallyFlipped = GET_BIT(vm->fetcherTileAttributes, 6);
            uint8_t currentRowInTile = vm->fetcherY % 8;

            if (verticallyFlipped) currentRowInTile = 8 - currentRowInTile;
            vm->fetcherTileRowHigh = tileData[(2 * currentRowInTile) + 1];
            
            /*         
            if (currentRowInTile == 7) {
                printf("tile data for x%d\n", vm->fetcherX * 8);
                for (int i = 0; i < 16; i++) {
                    printf("%02x ", tileData[i]);
                }
                printf("\n");
            }
            */

            if (vm->firstTileInScanline) {
                vm->currentFetcherTask = 0;
                vm->firstTileInScanline = false;
                break;
            }

            /* We reset it on the 6th cycle itself instead of doing it on the 7th 
             * because we only need to wait 6 dots in total */
            if (vm->fetcherX > 19) {
                /* Dont push if the pixel X coordinate exceeds 160 
                 * we just reset it back to the top */
                vm->currentFetcherTask = 0;
                vm->fetcherX = 0;
                break;
            }
            vm->currentFetcherTask++;
			break;
		}	
		case FETCHER_SLEEP:
            vm->currentFetcherTask++;
            break;			/* Do nothing */
		case FETCHER_PUSH: { 
            if (vm->BackgroundFIFO.count != 0) {
                /* We cant push yet since all pixels havent been pushed to the LCD yet */
                vm->doOptionalPush = true;
                vm->currentFetcherTask++;
                break;
            } 

            pushPixels(vm);
            vm->doOptionalPush = false;
        	vm->currentFetcherTask++;
			break;
		}
        case FETCHER_OPTIONAL_PUSH: {
            if (vm->doOptionalPush) {
                pushPixels(vm);
                vm->doOptionalPush = false;
            }

            vm->currentFetcherTask = 0;
        }
	}	
}

static void advancePPU(VM* vm) {
	/* We use a state machine to handle different PPU modes */
	vm->cyclesSinceLastMode++;


	switch (vm->ppuMode) {
		case PPU_MODE_2:
			/* Do nothing for PPU mode 2 
			 *
			 * Beginning of new scanline */
			if (vm->cyclesSinceLastMode == T_CYCLES_PER_MODE2) {
				switchModePPU(vm, PPU_MODE_3);
                /* clear fifo at the start of mode 3 */
                clearFIFO(&vm->BackgroundFIFO);

                /* When the fetcher is on the first tile in a scanline,
                 * it rolls back to task 0 after getting the higher
                 * tile data, this consumes a total of 6 dots */ 
                vm->firstTileInScanline = true;
            }

			break;
		case PPU_MODE_3:
            /* Mode 3 has a variable duration */
			/* Draw Pixels */ 

            /* Because the renderer idles for 6 dots, we run mode 3 for 6 dots more 
             * this also means that the fetcher has to idle (not push) for 6 dots while the renderer fully
             * renders the last tile in the scanline */
			advanceFetcher(vm); 
            renderPixel(vm);
            
            /* The last push should increment fetcher X to 20, the fetcher will reset back to 
             * step 1 and not do anything for 1 tile after that (because mode 3 is still waiting
             * for the rendering to complete which is delayed by 6 dots) as running it further
             * will push uneeded pixels */
			if (vm->lastRenderedPixelX == 159) {
                /* tile pixel row over */ 
                vm->lastRenderedPixelX = 0;
                vm->hblankDuration = T_CYCLES_PER_SCANLINE - T_CYCLES_PER_MODE2 - vm->cyclesSinceLastMode;
				switchModePPU(vm, PPU_MODE_0);
            }

			break;
		case PPU_MODE_0: {
			/* HBLANK */
			if (vm->cyclesSinceLastMode == vm->hblankDuration) {
				/* End of scanline */
				uint8_t currentScanlineNumber = vm->cyclesSinceLastFrame / T_CYCLES_PER_SCANLINE;
                // printf("%d\n", currentScanlineNumber);
                /* Set fetcher Y */
                vm->fetcherY++;
				if (currentScanlineNumber == 144) {
                    /* End of scanline 
                     * switch to vblank now */
                    vm->fetcherY = 0;
                    switchModePPU(vm, PPU_MODE_1);
                    requestInterrupt(vm, INTERRUPT_VBLANK);
                }
				else switchModePPU(vm, PPU_MODE_2);
			} else if (vm->cyclesSinceLastMode == vm->hblankDuration - 6) {
				/* LY register gets incremented 6 dots before the *true* increment */
				vm->MEM[R_LY]++;
				updateSTAT(vm, STAT_UPDATE_LY_LYC);
			}

			break;
		}
		case PPU_MODE_1: {
			/* VBLANK */	
			/* LY Resets to 0 after 4 cycles on line 153 
			 * LY=LYC Interrupt is triggered 12 cycles after line 153 begins */ 
			unsigned cycleAtLYReset = T_CYCLES_PER_VBLANK - T_CYCLES_PER_SCANLINE + 4;	
			unsigned cycleAtLYCInterrupt = T_CYCLES_PER_VBLANK - T_CYCLES_PER_SCANLINE + 12;
			if (cycleAtLYReset != vm->cyclesSinceLastMode && vm->cyclesSinceLastMode % 
					T_CYCLES_PER_SCANLINE == T_CYCLES_PER_SCANLINE - 6) {

				/* LY register Increments 6 cycles before *true* LY, only when
				 * the line is other than 153, because on line 153 it resets early 
                 * LY also stays 0 at the end of line 153 so dont increment when its 0 */
                if (vm->MEM[R_LY] != 0) {
				    vm->MEM[R_LY]++;
                } 
				updateSTAT(vm, STAT_UPDATE_LY_LYC);
			} else if (vm->cyclesSinceLastMode == T_CYCLES_PER_VBLANK) {
                /* vblank has ended */
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
    if (vm->ppuEnabled) return;
	vm->ppuEnabled = true;
	switchModePPU(vm, PPU_MODE_2);

	/* Skip first frame after LCD is turned on */
	vm->skipFrame = true;

    /* Reset frame counter so the whole emulator is again aligned with the PPU frames */
    vm->cyclesSinceLastFrame = 0;
}

void disablePPU(VM* vm) {
    if (!vm->ppuEnabled) return;
	if (vm->ppuMode != PPU_MODE_1) {
		/* This is dangerous for any game/rom to do on real hardware
		 * as it can damage hardware */
		printf("[WARNING] Turning off LCD when not in VBlank can damage a real gameboy\n");
	}

	vm->ppuEnabled = false;
	vm->MEM[R_LY] = 0;

	/* Set STAT mode to 0 */
	switchModePPU(vm, PPU_MODE_0);
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
	if (!vm->ppuEnabled) {
        /* Keep locking to framerate even if PPU is off */
        for (unsigned int i = 0; i < cycles; i++) {
            vm->cyclesSinceLastFrame = 0;

            if (vm->cyclesSinceLastFrame == T_CYCLES_PER_FRAME) {
                vm->cyclesSinceLastFrame = 0;
		        lockToFramerate(vm);
            }
        }

		return;
	}

	for (unsigned int i = 0; i < cycles; i++) {
		vm->cyclesSinceLastFrame++;
#ifdef DEBUG_PRINT_PPU
        printf("[m%d|ly%03d|fcy%05d|mcy%04d|fifoc%d|lastX%03d]\n", vm->ppuMode, vm->MEM[R_LY], vm->cyclesSinceLastFrame, vm->cyclesSinceLastMode, vm->BackgroundFIFO.count, vm->lastRenderedPixelX);
#endif
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
