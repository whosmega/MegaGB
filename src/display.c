#include "../include/gb.h"
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
#include <stdio.h>

static void lockToFramerate(GB* gb) {
    /* The emulator keeps its speed accurate by locking to the framerate
     * Whatever has to be done (cpu execution, audio, rendering a frame) in
     * the interval equivalent to 1 frame render on the gameboy is done in 1 frame
     * render on the emulator, the remaining time is waited for on the emulator to
     * sync with the time on the gameboy */
    unsigned long ticksElapsed = (clock_u() - gb->ticksAtStartup) - gb->ticksAtLastRender;

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
    gb->ticksAtLastRender = clock_u() - gb->ticksAtStartup;
}

static void updateSTAT(GB* gb, STAT_UPDATE_TYPE type) {
    /* This function updates the STAT register depending on the type of update
     * that is requested */

#define SWITCH_MODE(mode) gb->IO[R_STAT] &= ~0x3; \
    gb->IO[R_STAT] |= mode

    switch (type) {
        case STAT_UPDATE_LY_LYC:
            if (gb->IO[R_LY] == gb->IO[R_LYC]) {
                /* Set bit 2 */
                SET_BIT(gb->IO[R_STAT], 2);

                if (GET_BIT(gb->IO[R_STAT], 6)) {
                    /* If bit 6 is set, we can trigger the STAT interrupt */
                    requestInterrupt(gb, INTERRUPT_LCD_STAT);
                }
            } else {
                /* Clear bit 2 */
                CLEAR_BIT(gb->IO[R_STAT], 2);
            }
            break;
        case STAT_UPDATE_SWITCH_MODE0:
            /* Switch to mode 0 */
            SWITCH_MODE(PPU_MODE_0);
            if (GET_BIT(gb->IO[R_STAT], 3)) {
                /* If mode 0 interrupt source is enabled */
                requestInterrupt(gb, INTERRUPT_LCD_STAT);
            }
            break;
        case STAT_UPDATE_SWITCH_MODE1:
            /* Switch to mode 1 */
            SWITCH_MODE(PPU_MODE_1);

            if (GET_BIT(gb->IO[R_STAT], 4)) {
                requestInterrupt(gb, INTERRUPT_LCD_STAT);
            }
            break;
        case STAT_UPDATE_SWITCH_MODE2:
            SWITCH_MODE(PPU_MODE_2);

            if (GET_BIT(gb->IO[R_STAT], 5)) {
                requestInterrupt(gb, INTERRUPT_LCD_STAT);
            }
            break;
        case STAT_UPDATE_SWITCH_MODE3:
            SWITCH_MODE(PPU_MODE_3);
            break;
    }

#undef SWITCH_MODE
}

static inline void switchModePPU(GB* gb, PPU_MODE mode) {
    gb->ppuMode = mode;
    gb->cyclesSinceLastMode = 0;

    switch (mode) {
        case PPU_MODE_2:
            gb->lockOAM = true;
            gb->lockPalettes = false;
            gb->lockVRAM = false;
            updateSTAT(gb, STAT_UPDATE_SWITCH_MODE2);
            break;
        case PPU_MODE_3:
            gb->lockOAM = true;
            gb->lockPalettes = true;
            gb->lockVRAM = true;
            updateSTAT(gb, STAT_UPDATE_SWITCH_MODE3);
            break;
        case PPU_MODE_0:
            gb->lockOAM = false;
            gb->lockPalettes = false;
            gb->lockVRAM = false;
            updateSTAT(gb, STAT_UPDATE_SWITCH_MODE0);
            break;
        case PPU_MODE_1:
            gb->lockOAM = false;
            gb->lockPalettes = false;
            gb->lockVRAM = false;
            updateSTAT(gb, STAT_UPDATE_SWITCH_MODE1);
            break;
    }
}

static uint8_t* getCurrentFetcherTileData(GB* gb) {
    /* Returns the tile data pointer (first of the 16 bytes) of the tile the fetcher is currently
     * on by reading state from the emulator
     *
     * can be BG/Window/Sprite */

    uint16_t tileMapBaseAddress = GET_BIT(gb->IO[R_LCDC], 3) ? 0x1C00 : 0x1800;

    /* Each row takes 2 bytes from the tile data of every tile per scanline
     * which 2 bytes are taken is determined by the Y position of the fetcher,
     * since each tile has a height of 8 pixels */

    /* On CGB, Vram bank 0 pointer points to the bank 0 of the vram, while vram bank pointer
     * can point to either one (because which bank to fetch the tile from depends on
     * bg tile attributes)
     *
     * On DMG, there is only 1 vram bank and therefore both variables will point to the same */
    uint8_t* vramBankPointer = NULL;
    uint8_t* vramBank0Pointer = gb->vram;

    if (gb->emuMode == EMU_CGB) {
        uint8_t useVramBank1 = GET_BIT(gb->fetcherTileAttributes, 3);

        /* Select which pointer to use to fetch tile data */
        if (useVramBank1) {
            /* Tile data from vram bank 1 */
            vramBankPointer = &gb->vram[0x2000];
        } else {
            vramBankPointer = vramBank0Pointer;
        }
    } else if (gb->emuMode == EMU_DMG) {
        vramBankPointer = vramBank0Pointer;
    }

    /* Use the tile index to get a pointer to the first byte of the 16 byte tile data */
    uint8_t* tileData = NULL;

    if (gb->renderingSprites) {
        /* Get tile data for sprites
         *
         * Sprites always use $8000 method to index sprites */
        uint8_t tileIndex = gb->spriteData[2];
        /* Ignore the least significant bit when dealing with 8x16 sprites,
         * the index can only be even numbered as it points to the top sprite */
        if (gb->spriteSize == 1) tileIndex &= 0xFE;

        tileData = vramBankPointer + (tileIndex * 16);
    } else {
        /* Get tile data for window/bg */
        uint8_t tileIndex = vramBank0Pointer[gb->fetcherTileAddress];
        /* Find out the addressing method to use */
        bool method8k = GET_BIT(gb->IO[R_LCDC], 4);

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
                tileData = vramBankPointer + 0x800 + ((tileIndex - 128) * 16);
            }
        }
    }

    return tileData;
}

static inline uint8_t toRGB888(uint8_t rgb555) {
    /* Input can be red, green or blue value of rgb 555 */
    return (rgb555 << 3) | (rgb555 >> 2);
}

static void getPixelColor_CGB(GB* gb, FIFO_Pixel pixel, uint8_t* r, uint8_t* g, uint8_t* b,
        bool isSprite) {

    uint8_t* colorRAM = isSprite ? gb->spriteColorRAM : gb->bgColorRAM;
    uint8_t* palettePointer = &colorRAM[pixel.colorPalette * 8];
    /* Color is stored as little endian rgb555 */
    uint8_t colorLow = palettePointer[pixel.colorID * 2];
    uint8_t colorHigh = palettePointer[(pixel.colorID * 2) + 1];
    // printf("ci %d cl %02x ch %02x cp %02x x %03d y %03d\n", pixel.colorID, colorLow, colorHigh, pixel.colorPalette, pixel.screenX, pixel.screenY);
    uint16_t color = (colorHigh << 8) + colorLow;

    /* We need to convert these values to rgb888
     * Source for conversion : https://stackoverflow.com/questions/4409763/how-to-convert-from-rgb555-to-rgb888-in-c*/
                                     // printf("c%04x p%d\n", color, pixel.colorPalette);

    *r = toRGB888((color & 0b0000000000011111));
    *g = toRGB888((color & 0b0000001111100000) >> 5);
    *b = toRGB888((color & 0b0111110000000000) >> 10);

    /*
       for (int i = 0; i < 64; i++) {
       printf("%02x ", gb->colorRAM[i]);
       }


       printf("\n");
       */
}

static void getPixelColor_DMG(GB* gb, FIFO_Pixel pixel, uint8_t* r, uint8_t* g, uint8_t* b,
        bool isSprite) {

    int paletteReg = R_BGP;
    if (isSprite) paletteReg = pixel.colorPalette == 0 ? R_OBP0 : R_OBP1;

    uint8_t shadeId = (gb->IO[paletteReg] >> (pixel.colorID * 2)) & 0b00000011;
    uint8_t shade = 0x00;

    switch (shadeId) {
        case 0: {
                    /* White */
                    shade = 0xFF;
                    break;
                }
        case 1: {
                    /* Light Gray */
                    shade = 0xAA;
                    break;
                }
        case 2: {
                    /* Dark Gray */
                    shade = 0x55;
                    break;
                }
        case 3: {
                    /* Black */
                    shade = 0x00;
                    break;
                }
    }

    *r = shade;
    *g = shade;
    *b = shade;
}

static void renderPixel(GB* gb) {
    if (!gb->ppuEnabled) return;
    if (gb->BackgroundFIFO.count == 0) return;

    FIFO_Pixel pixel = popFIFO(&gb->BackgroundFIFO);
    uint8_t r, g, b = 0;
    bool isSprite = false;

    if (gb->OAMFIFO.count != 0) {
        FIFO_Pixel spritePixel = popFIFO(&gb->OAMFIFO);

        if (spritePixel.colorID != 0) {
            if (gb->emuMode == EMU_DMG) {
                if (spritePixel.bgPriority == 0 || pixel.colorID == 0) {
                    /* case 1. Pixel isnt transparent and it has a priority over BG/Window
                     * or case 2. BG color 0 will be overwritten, otherwise BG/Window have a
                     * higher priority*/
                    pixel = spritePixel;
                    isSprite = true;
                }
            } else if (gb->emuMode == EMU_CGB) {
                /* For CGB, we've got additional 2 priority flags to check */
                if (!GET_BIT(gb->IO[R_LCDC], 0)) {
                    /* Sprites will be displayed on top because master priority */
                    pixel = spritePixel;
                    isSprite = true;
                } else {
                    /* Master priority is not used, instead we start by checking
                     * BG Attributes priority. If its set, sprite only overlaps color 0 BG,
                     * it gets covered by anything else BG/Window
                     * If its not set, it uses OAM bit.
                     *
                     * If OAM bit is set, sprite overlaps BG/Window color 1-3
                     * If its not set, sprite overlaps all BG/Window unless sprite is color O*/
                    if ((pixel.bgPriority == 1 && pixel.colorID == 0) ||
                            (pixel.bgPriority == 0 && (spritePixel.bgPriority == 0 || pixel.colorID == 0))) {
                        pixel = spritePixel;
                        isSprite = true;
                    }
                }
            }
        }
    }

    /*
       if (pixel.screenY >= 64 && pixel.screenY < 80) {
       if (isSprite) printf("Rendering Sprite Pixel CID%d X%d Y%d\n", pixel.colorID, pixel.screenX, pixel.screenY);
       else printf("Rendering BG/Win Pixel CID%d X%d Y%d\n", pixel.colorID, pixel.screenX, pixel.screenY);
       }
       */

    if (gb->emuMode == EMU_CGB) {
        getPixelColor_CGB(gb, pixel, &r, &g, &b, isSprite);
    } else if (gb->emuMode == EMU_DMG) {
        getPixelColor_DMG(gb, pixel, &r, &g, &b, isSprite);
    }

    SDL_SetRenderDrawColor(gb->sdl_renderer, r, g, b, 255);
    SDL_RenderDrawPoint(gb->sdl_renderer, pixel.screenX, pixel.screenY);

    gb->nextRenderPixelX = pixel.screenX + 1;
    // printf("rendered pixel at x%d\n", pixel.screenX);
}

static uint8_t* getSprite(GB* gb) {
    if (gb->spritesInScanline == 0) return NULL;
    /* If OBJ isnt enabled, its instantly aborted */
    if (!(GET_BIT(gb->IO[R_LCDC], 1))) return NULL;
    /* Read OAM Memory to find a sprite that can be rendered at the current X and Y of the
     * fetcher if it is not found we return NULL */


    for (int i = 0; i < gb->spritesInScanline; i++) {
        uint8_t spriteIndex = i * 5;
        uint8_t spriteX = gb->oamDataBuffer[spriteIndex + 1];

        /* Dont render invisible sprites */
        if (spriteX == 0 || spriteX >= 168) continue;
        /* Only when theres a full overlap, the index of the current sprite has to be
         * greater than the index of the last overlapping sprite, to prevent infinite loop
         * and ensure none are repeated */

        int startX = spriteX - 8;

        if (gb->nextPushPixelX == startX || (gb->nextPushPixelX == 0 && startX < 0)) {
            if (gb->isLastSpriteOverlap && startX == gb->lastSpriteOverlapX) {
                if (gb->oamDataBuffer[spriteIndex + 4] > gb->lastSpriteOverlapPushIndex)
                    return &gb->oamDataBuffer[spriteIndex];
            } else {
                return &gb->oamDataBuffer[spriteIndex];
            }
        }
    }

    return NULL;
}

static void pushPixels(GB* gb) {
    uint8_t tileDataLow = gb->fetcherTileRowLow;
    uint8_t tileDataHigh = gb->fetcherTileRowHigh;
    bool switchedToWindowRender = false;
    bool switchedToSpriteRender = false;
    /* Push pixels to FIFO
     *
     * When SCX % 8 != 0 at the start of the scanline,
     * the extra pixels are stored and then used to adjust tile fetching
     *
     * On the first tile we skip the pixels which are not to be rendered for a tile
     * and then continue rendering normally. This also means in the end we might not
     * fully render the rightmost tile, we exit the pushing when the last pixel that can
     * be rendered is pushed, i.e screen X = 159*/
    for (int i = 1; i <= 8; i++) {
        uint8_t* sprite = getSprite(gb);
        if (sprite != NULL) {
            /* Since a part of the BG/Window tiles might be already pushed, we can calculate
             * how many pixels to discard when continuing pushing for BG
             *
             * This can happen when either the sprite is not aligned with the 8 pixel alignment
             * of the BG/Window grid i.e Sprite X % 8 != 0
             *
             * or when the initial bg scrolling is not aligned with the 8 pixel grid
             * which causes a few pixels in the bg to be discarded at the start of the scanline
             * Since this affects the position of the pixels in the rest of the scanline,
             * this has to be calculated when discarding pixels for the sprite which are already
             * pushed
             *
             * When sprites overlap, the pixels to discard calculated by the first sprite
             * should be modified to accomodate the pixels of the overlapping sprite too
             * For example, if a sprite offseted 4 pixels from bg grid gets overlapped with 7
             * pixels of another sprite positioned 1X after it, the first sprite calculates 4
             * pixels to be discarded, but because the overlapping sprite causes 1 more pixel to
             * be rendered, pixels to discard has to be adjusted to discard 1 more pixel
             */
            if (!gb->isLastSpriteOverlap) {
                /* If sprite is slightly off screen, leave the scx offset unchanged as the sprite
                 * technically starts at 0X, the part of the tile that should be discarded is
                 * already calculated */
                if (gb->nextPushPixelX == 0 && sprite[1] < 8 && gb->pixelsToDiscard != 0); // do nothing
                else {
                    /* If the sprite starts rendering at the start of a BG/Window tile,
                     * i - 1 == 0, so there are no pixels to discard for the sprite
                     * If it starts in the middle of a tile, the number of pixels in tile already
                     * pushed need to be discarded after the fetcher returns back to bg push mode
                     * after completing sprite push. In that case we safely *overwrite* the pixels
                     * to be discarded that were calculated for BG offset as atleast 1 bg pixel
                     * has been rendered, and the pixels have been discarded, therefore the pixels
                     * to discard value has no use now */
                    if (i - 1 != 0) gb->pixelsToDiscard = i - 1;
                }
            }
            else {
                gb->pixelsToDiscard += gb->nextPushPixelX - (gb->lastSpriteOverlapX < 0 ? 0 : gb->lastSpriteOverlapX);
            }

            gb->renderingSprites = true;
            switchedToSpriteRender = true;
            gb->spriteData = sprite;
            /* Fetch data and attributes for the remaining bg/window tiles get preserved
             * for later use */
            gb->preservedFetcherTileLow = gb->fetcherTileRowLow;
            gb->preservedFetcherTileHigh = gb->fetcherTileRowHigh;
            gb->preservedFetcherTileAttributes = gb->fetcherTileAttributes;
            break;
        }

        if (GET_BIT(gb->IO[R_LCDC], 5) &&
                gb->lyWasWY && !gb->renderingWindow && (gb->nextPushPixelX == gb->IO[R_WX] - 7 || gb->IO[R_WX] < 7)) {
            /* Window layer is enabled, and the current pixel to render is a window
             * pixel, if a scanline has window tiles, it takes 6 more cycles anyhow
             * as the BG fetch is aborted
             */

            gb->renderingWindow = true;
            gb->fetcherX = 0;
            switchedToWindowRender = true;
            /* We set the fetcher task to 1 because the fetcher is supposed to switch to
             * window rendering immediately after the last bg pixel in the row is pushed,
             * we're detecting that the next pixel is a window pixel one cycle after the last
             * bg push (or this is the first cycle anyway if the window tile is the first tile)
             * This means the current cycle does the job of sleeping automatically */
            gb->currentFetcherTask = 1;

            if (gb->IO[R_WX] <= 7) {
                if (gb->IO[R_WX] == 0 && gb->pixelsToDiscard != 0) {
                    /* Shorten mode 3 by 1 dot (bug) */
                    gb->cyclesSinceLastMode--;
                }
                gb->pixelsToDiscard = 7 - gb->IO[R_WX];
            } else {
                gb->pixelsToDiscard = 0;
            }

            break;
        }

        /* This would work for BG tiles and Window tiles both as when fetcherX is set to 0
         * after switching to window mode, pixelsToDiscard is also automatically set to the
         * correct value
         *
         * For OBJs, pixels to discard is also used to discard pixels which are already pushed
         * for window/background when dealing with sprites which are not fully aligned with
         * BG tiles */
        if (i <= gb->pixelsToDiscard) {
            continue;
        }
        // if (pixelsToDiscard > 0 && gb->fetcherX != 0 && !gb->renderingSprites && !gb->renderingWindow) printf("fuck x%d y%d %d\n", gb->nextPushPixelX - 1, gb->fetcherY, gb->pixelsToDiscard);
        FIFO_Pixel pixel;
        /* Check for horizontal bit flip */
        uint8_t index = GET_BIT(gb->fetcherTileAttributes, 5) ? i - 1 : 8 - i;
        uint8_t higherBit = GET_BIT(tileDataHigh, index);
        uint8_t lowerBit = GET_BIT(tileDataLow, index);
        uint8_t bgPriority = 0;

        /* Set color palette, color ID and other data  */
        if (gb->emuMode == EMU_CGB) {
            pixel.colorPalette = gb->fetcherTileAttributes & 0b00000111;
            pixel.colorID = (higherBit << 1) | lowerBit;
            bgPriority = GET_BIT(gb->fetcherTileAttributes, 7);

        } else if (gb->emuMode == EMU_DMG) {
            pixel.colorPalette = 0;
            /* On DMG, if background/window is disabled through lcdc, bgp color 0 is rendered */
            if (GET_BIT(gb->IO[R_LCDC], 0)) {
                pixel.colorID = (higherBit << 1) | lowerBit;
            } else {
                pixel.colorID = 0;
            }
        }

        /* Store the tile map BG priority bit */
        pixel.bgPriority = bgPriority;

        pixel.screenX = gb->nextPushPixelX;
        pixel.screenY = gb->fetcherY;

        pushFIFO(&gb->BackgroundFIFO, pixel);
        /* When it reaches 159, the scanline is over */
        gb->nextPushPixelX++;
        /* Even if we didnt complete the tile, we need to exit after pushing the last pixel */
        if (gb->nextPushPixelX == 160) break;

    }
    // printf("pushed 8 pixels %03d-%03d\n", gb->fetcherX * 8, gb->fetcherX * 8 + 7);
    if (switchedToWindowRender) {
        gb->isLastSpriteOverlap = false;
        gb->currentFetcherTask = 0;
    } else if (switchedToSpriteRender) {
        if (gb->currentFetcherTask == 7) gb->currentFetcherTask = 0;
        else gb->currentFetcherTask++;
    } else {
        gb->isLastSpriteOverlap = false;
        gb->fetcherX++;
        gb->currentFetcherTask++;

        gb->pixelsToDiscard = 0;
    }
}

static void pushSpritePixels(GB* gb) {
    /* When the sprite fetch is complete, this function is called to push the
     * sprite pixels to the sprite fifo as well as push the BG/Window pixels
     * which were remaining at the start of sprite rendering. */
    uint8_t tileDataLow = gb->fetcherTileRowLow;
    uint8_t tileDataHigh = gb->fetcherTileRowHigh;
    uint8_t tileAttributes = gb->fetcherTileAttributes;
    int startX = gb->spriteData[1] - 8;
    uint8_t spriteOAMIndex = gb->spriteData[4];
    bool partiallyOverlaps = false;

    /* Fill the OAM fifo with transparent pixels of lowest priority if its not full */
    if (gb->OAMFIFO.count > 0 && gb->OAMFIFO.count < 8) partiallyOverlaps = true;

    for (int i = 0; i < 8 && gb->OAMFIFO.count != 8; i++) {
        if (gb->nextPushPixelX + i == 160) break;
        /* If the sprite is off screen towards the left, because only a portion of
         * the sprite is rendered, we dont fill the fifo completely but only make
         * sure it has the minimum size it needs. Overlapping sprites are also taken
         * into account as the fifo count as a whole is checked */
        if (startX < 0 && gb->OAMFIFO.count >= 8 + startX) break;

        FIFO_Pixel p;
        p.bgPriority = 1;
        p.colorID = 0;
        p.colorPalette = 0;
        p.screenX = gb->nextPushPixelX + i;
        p.screenY = gb->fetcherY;

        pushFIFO(&gb->OAMFIFO, p);
    }

    /* Compare and replace OAM FIFO with the sprite tile data
     *
     * In case the sprite starts a bit off screen to the left, we discard the correct amount
     * of pixels */
    for (int i = startX < 0 ? -startX + 1 : 1; i <= 8; i++) {
        /* Check for horizontal bit flip */
        uint8_t pixelIndex = GET_BIT(tileAttributes, 5) ? i - 1 : 8 - i;
        /* Make adjustments to the index in the fifo that is utilized when sprite is
         * off screen to the left
         *
         * Note : startX is a negative value so its subtracted */
        uint8_t fifoIndex = (i - 1) + (startX < 0 ? startX : 0);
        uint8_t higherBit = GET_BIT(tileDataHigh, pixelIndex);
        uint8_t lowerBit = GET_BIT(tileDataLow, pixelIndex);
        uint8_t colorID = (higherBit << 1) | lowerBit;
        uint8_t bgPriority = GET_BIT(tileAttributes, 7);
        uint8_t colorPalette = 0;

        if (gb->emuMode == EMU_DMG) colorPalette = GET_BIT(tileAttributes, 4);
        else if (gb->emuMode == EMU_CGB) colorPalette = tileAttributes & 0b00000111;

        /* When we've got overlapping sprites, the pixels which already exist
         * in the fifo are compared with the pixels of the current sprite,
         * transparent pixels are overwritten instantly, otherwise the sprite priority
         * is determined by the X coordinate and index in OAM on DMG and only index in OAM
         * on CGB. The sprite fifo will always be empty when pushing unless 2 sprites
         * are overlapping either partially or fully
         *
         * After they are pushed, the shifter calculates which pixels overlap BG and which dont
         * using the BG priority */

        FIFO_Pixel fifoPixel = peekFIFO(&gb->OAMFIFO, fifoIndex);
        FIFO_Pixel pixel;

        pixel.colorID = colorID;
        pixel.colorPalette = colorPalette;
        pixel.bgPriority = bgPriority;
        pixel.screenX = gb->nextPushPixelX + fifoIndex;
        pixel.screenY = gb->fetcherY;
        /* Compare */

        if (fifoPixel.colorID == 0 && pixel.colorID != 0) {
            /* Overwrite FIFO pixel if the current pixel in fifo is transparent and
             * the new pixel isnt */
            insertFIFO(&gb->OAMFIFO, pixel, fifoIndex);
        } else if (fifoPixel.colorID != 0 && pixel.colorID != 0) {
            /* If both are opaque */
            if (gb->emuMode == EMU_DMG) {
                /* If current sprite partially overlaps a previous one, it means
                 * the X coordinate of the previous sprite is greater. So it is given a higher
                 * priority and we dont overwrite the fifo in that case
                 *
                 * Otherwise if the X coordinate is the same, and this sprite's index is
                 * lesser than the last sprite's index, the pixel is overwritten */
                if (!partiallyOverlaps && spriteOAMIndex < gb->lastSpriteOverlapPushIndex) {
                    insertFIFO(&gb->OAMFIFO, pixel, fifoIndex);
                }
            } else if (gb->emuMode == EMU_CGB) {
                /* On CGB, the pixel is overwritten if the current sprite's index is greater
                 * than the previous sprite in any condition */
                if (spriteOAMIndex < gb->lastSpriteOverlapPushIndex) {
                    insertFIFO(&gb->OAMFIFO, pixel, fifoIndex);
                }
            }
        }

        /* If fifoPixel is opaque and current pixel is transparent, even if current pixel has a
         * higher priority we dont overwrite */
    }

    // printf("pixels to discard %d, X%d, Y%d\n", gb->pixelsToDiscard, gb->nextPushPixelX, gb->fetcherY);

    gb->isLastSpriteOverlap = true;
    gb->lastSpriteOverlapPushIndex = spriteOAMIndex;
    gb->lastSpriteOverlapX = startX;
    /* Restore State */
    gb->fetcherTileAttributes = gb->preservedFetcherTileAttributes;
    gb->fetcherTileRowHigh = gb->preservedFetcherTileHigh;
    gb->fetcherTileRowLow = gb->preservedFetcherTileLow;

    gb->preservedFetcherTileAttributes = 0;
    gb->preservedFetcherTileHigh = 0;
    gb->preservedFetcherTileLow = 0;

    gb->renderingSprites = false;
    /* Continue BG/Window pushing */
    pushPixels(gb);
}

static void advanceFetcher(GB* gb) {
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
        FETCHER_OPTIONAL_PUSH,
        /* The push is attempted on the first dot, if it succeeds, the second dot is slept
         * otherwise the push is retried on the second dot and then it indefinitely retries till
         * the push can be made
         */
    };

    /* Fetcher state machine to handle pixel FIFO */
    switch (fetcherTasks[gb->currentFetcherTask]) {
        case FETCHER_GET_TILE: {
            uint16_t tileMapBaseAddress;

            if (gb->renderingSprites) {
                /* Fetch Sprite */
                gb->fetcherTileAttributes = gb->spriteData[3];

                /* We already have access to the index of the tile */
            } else if (gb->renderingWindow) {
                /* Fetch Window Tile
                 *
                 * If a scanline enters window rendering mode, it cant exit it till the next
                 * scanline begins, mid scanline writes to window enable bit in lcdc are supposed
                 * to make it switch back to bg tiles for the scanline but we dont emulate that
                 * yet */
                if (GET_BIT(gb->IO[R_LCDC], 6)) {
                    tileMapBaseAddress = 0x1C00;
                } else {
                    tileMapBaseAddress = 0x1800;
                }

                /* Fetcher X contains the window coords since its reset when we start rendering
                 * the window. Fetcher Y is left unchanged as it preserves the line number of the
                 * entire frame. Instead we use the internal window counter */
                uint8_t x = gb->fetcherX;
                uint8_t y = gb->windowYCounter;

                gb->fetcherTileAddress = tileMapBaseAddress + x + (y/8) * 32;
                if (gb->emuMode == EMU_CGB) {
                    /* Handle tile attributes if on a CGB
                     *
                     * This works for both BG and Window */
                    uint8_t* vramBank1Pointer = &gb->vram[0x2000];
                    gb->fetcherTileAttributes = vramBank1Pointer[gb->fetcherTileAddress];
                } else if (gb->emuMode == EMU_DMG) {
                    gb->fetcherTileAttributes = 0;
                }
            } else {
                /* Fetch BG Tiles */
                /* There are two tile maps, check which one to use */
                if (GET_BIT(gb->IO[R_LCDC], 3)) {
                    tileMapBaseAddress = 0x1C00;
                } else {
                    tileMapBaseAddress = 0x1800;
                }

                /* Fetcher X and Y are not the final x and y coordinates we get the tile from
                 * First scrolling has to be calculated */
                uint8_t scx = gb->IO[R_SCX] & ~0b00000111;         // clear the last 3 bits
                uint8_t x = (scx/8 + gb->fetcherX);
                x &= 0x1F;                                          // wrap it around if it exceeds
                uint8_t y = (uint16_t)(gb->fetcherY + gb->IO[R_SCY]) & 0xFF;
                gb->fetcherTileAddress = tileMapBaseAddress + x + (y / 8) * 32;

                if (gb->emuMode == EMU_CGB) {
                    /* Handle tile attributes if on a CGB
                     *
                     * This works for both BG and Window */
                    uint8_t* vramBank1Pointer = &gb->vram[0x2000];
                    gb->fetcherTileAttributes = vramBank1Pointer[gb->fetcherTileAddress];
                } else if (gb->emuMode == EMU_DMG) {
                    gb->fetcherTileAttributes = 0;
                }
            }

            gb->currentFetcherTask++;
            break;
        }
        case FETCHER_GET_DATA_LOW: {
            uint8_t* tileData = getCurrentFetcherTileData(gb);
            uint8_t currentRowInTile;

            if (gb->renderingSprites) {
                /* For OBJ/Sprite tiles */
                currentRowInTile = gb->spriteData[0];
            } else if (gb->renderingWindow) {
                /* For window tiles */
                currentRowInTile = gb->windowYCounter % 8;
            } else {
                /* For BG tiles
                 * Now retrieve the lower byte of this row */
                currentRowInTile = (gb->fetcherY + gb->IO[R_SCY]) % 8;
            }

            /* Vertical flip works for sprites on DMG and CGB, and it works for BG/Window
             * only on CGB */
            if (gb->renderingSprites || gb->emuMode == EMU_CGB) {
                bool verticallyFlipped = GET_BIT(gb->fetcherTileAttributes, 6);
                /* If the tile is flipped, we can get the vertically opposite row in the tile */
                if (verticallyFlipped) currentRowInTile = (gb->spriteSize == 0 ? 7 : 15) - currentRowInTile;
            }

            gb->fetcherTileRowLow = tileData[2 * currentRowInTile];
            gb->currentFetcherTask++;
            break;
        }
        case FETCHER_GET_DATA_HIGH: {
            /* Do the same as above but instead get the higher byte */
            uint8_t* tileData = getCurrentFetcherTileData(gb);
            uint8_t currentRowInTile;

            if (gb->renderingSprites) {
                /* Sprites */
                currentRowInTile = gb->spriteData[0];
            } else if (gb->renderingWindow) {
                /* WIndow tiles */
                currentRowInTile = gb->windowYCounter % 8;
            } else {
                /* BG Tiles */
                currentRowInTile = (gb->fetcherY + gb->IO[R_SCY]) % 8;
            }

            if (gb->renderingSprites || gb->emuMode == EMU_CGB) {
                bool verticallyFlipped = GET_BIT(gb->fetcherTileAttributes, 6);
                if (verticallyFlipped) currentRowInTile = (gb->spriteSize == 0 ? 7 : 15) - currentRowInTile;
            }

            gb->fetcherTileRowHigh = tileData[(2 * currentRowInTile) + 1];
            if (gb->firstTileInScanline) {
                gb->currentFetcherTask = 0;
                gb->firstTileInScanline = false;
                break;
            }

            /* After the fetcher is done fetching for the scanline, it goes
             * in a loop where it stops right before moving on to the pushing stage
             * and instead goes back to GET_TILE, normally with BG, the pixel shifter
             * should be 6 cycles behind the fetcher, but after adding window delays
             * and sprite delays, this can change. So we dont wait for a fixed time */
            if (gb->nextPushPixelX == 160) {
                gb->currentFetcherTask = 0;
                break;
            }

            gb->currentFetcherTask++;
            break;
        }
        case FETCHER_SLEEP:
            gb->currentFetcherTask++;
            break;			/* Do nothing */
        case FETCHER_PUSH: {
            if (gb->BackgroundFIFO.count != 0) {
                /* We cant push yet since all pixels havent been pushed to the LCD yet */
                gb->doOptionalPush = true;
                gb->currentFetcherTask++;
                break;
            }

            /* Fetcher task is set internally */
            if (gb->renderingSprites) pushSpritePixels(gb);
            else pushPixels(gb);

            gb->doOptionalPush = false;
            break;
        }
        case FETCHER_OPTIONAL_PUSH: {
            if (gb->doOptionalPush) {
                if (gb->BackgroundFIFO.count != 0) break;

                pushPixels(gb);
                gb->doOptionalPush = false;
            }

            gb->currentFetcherTask = 0;
            break;
        }
    }
}

static void advanceMode2(GB* gb) {
    if (gb->spritesInScanline >= 10) return;

    uint8_t isLargeSprite = GET_BIT(gb->IO[R_LCDC], 2);
    uint8_t index = (gb->cyclesSinceLastMode / 2) - 1;

    gb->spriteSize = isLargeSprite;

    uint16_t spriteIndex = index * 4;
    uint8_t spriteY = gb->OAM[spriteIndex];
    uint8_t firstRowY, lastRowY;
    bool partial = false;

    if (isLargeSprite) {
        /* 8 x 16 sprite mode */
        if (spriteY == 0 || spriteY >= 160) return;

        if (spriteY < 16) {
            firstRowY = 0;
            lastRowY = spriteY - 1;
            partial = true;
        } else {
            firstRowY = spriteY - 16;
            lastRowY = firstRowY + 15;
        }
    } else {
        /* 8 x 8 sprite mode */
        if (spriteY <= 8 || spriteY >= 160) return;

        if (spriteY < 16) {
            firstRowY = 0;
            lastRowY = spriteY - 9;
            partial = true;
        } else {
            // if (spriteY - 16 == 0) printf("bruh %d %d\n", spriteY - 16, spriteY - 16 + 7);
            firstRowY = spriteY - 16;
            lastRowY = firstRowY + 7;
        }
    }

    if (gb->IO[R_LY] >= firstRowY && gb->IO[R_LY] <= lastRowY) {
        /* Sprite has to be rendered on this scanline */
        uint8_t bufferIndex = gb->spritesInScanline * 5;

        /* Instead of storing the actual sprite Y value, we can
         * store the current row of the sprite this scanline will render
         * so it doesnt have to be recalculated */
        if (partial) {
            uint8_t row;
            if (isLargeSprite) row = (15 - lastRowY) + gb->IO[R_LY];
            else row = (7 - lastRowY) + gb->IO[R_LY];

            gb->oamDataBuffer[bufferIndex] = row;
        } else gb->oamDataBuffer[bufferIndex] = gb->IO[R_LY] - firstRowY;

        memcpy(&gb->oamDataBuffer[bufferIndex+1], &gb->OAM[spriteIndex+1], 3);
        gb->oamDataBuffer[bufferIndex+4] = index;

        gb->spritesInScanline++;
    }
}

static void advancePPU(GB* gb) {
    /* We use a state machine to handle different PPU modes */
    gb->cyclesSinceLastMode++;

    switch (gb->ppuMode) {
        case PPU_MODE_2:
            /* Reading oam data
             * and some basic checks which are
             * necessary for timing, for example WY value
             *
             * Beginning of new scanline */

            if (gb->cyclesSinceLastMode % 2 == 0) {
                /* OAM Read Step
                 *
                 * Note : This also means that the very first scanline of first frame can only
                 * scan 38 oam entries because its 4 dots shorter */
                advanceMode2(gb);
            }

            if (gb->cyclesSinceLastMode == 1) {
                if (!GET_BIT(gb->IO[R_LCDC], 5)) break;

                uint8_t wx = gb->IO[R_WX];
                uint8_t wy = gb->IO[R_WY];
                /* On the first cycle, read WY */
                if (wy == gb->IO[R_LY]) {
                    /* TODO - Still need to confirm if the check is done with the internal LY
                     * counter or what the LY register reads, since LY is incremented 6 cycles
                     * earlier, its safe to assume the latter for now */
                    gb->lyWasWY = true;
                }
            } else if (gb->cyclesSinceLastMode == T_CYCLES_PER_MODE2) {
                switchModePPU(gb, PPU_MODE_3);
            }

            break;
        case PPU_MODE_3:
            if (gb->pauseDotClock > 0) {
                gb->pauseDotClock--;
                break;
            }

            if (gb->cyclesSinceLastMode == 1) {
                /* clear fifo at the start of mode 3 */
                clearFIFO(&gb->BackgroundFIFO);
                clearFIFO(&gb->OAMFIFO);
                /* When the fetcher is on the first tile in a scanline,
                 * it rolls back to task 0 after getting the higher
                 * tile data, this consumes a total of 6 dots */
                gb->firstTileInScanline = true;

                /* If SCX % 8 is not zero at the start of a scanline, the ppu pauses for as many
                 * dots as the remainder pixels,
                 * we pause the first dot aswell */
                uint8_t remainderPixels = gb->IO[R_SCX] % 8;
                gb->pixelsToDiscard = gb->IO[R_SCX] & 0b00000111;
                gb->pauseDotClock = 0;

                if (remainderPixels != 0) {
                    gb->pauseDotClock = remainderPixels - 1;
                    break;
                }

            }
            /* Mode 3 has a variable duration */
            /* Because the renderer idles for 6 dots, we run mode 3 for 6 dots more
             * this also means that the fetcher has to idle (not push) for 6 dots while the renderer fully
             * renders the last tile in the scanline */
            advanceFetcher(gb);
            renderPixel(gb);

            /* The last push should increment fetcher X to 20, after the last pixel in the scanline
             * has been pushed, we wait for 6 more dots (because we need to wait for the renderer
             * to finish which is 6 dots behind). At the end of the 6th dot itself, it resets back
             * to its initial state for the next scanline */
            if (gb->nextRenderPixelX == 160) {
                if (gb->renderingWindow) {
                    /* If we were rendering the window in this line,
                     * increment the window line counter
                     *
                     * This means if window gets disabled between scanlines,
                     * the window line counter is still preserved */
                    gb->renderingWindow = false;
                    gb->windowYCounter++;
                }

                gb->currentFetcherTask = 0;
                gb->fetcherX = 0;
                gb->nextPushPixelX = 0;
                gb->renderingSprites = false;
                gb->spritesInScanline = 0;
                gb->spriteData = NULL;
                gb->preservedFetcherTileLow = 0;
                gb->preservedFetcherTileHigh = 0;
                gb->preservedFetcherTileAttributes = 0;
                /* tile pixel row over */
                gb->nextRenderPixelX = 0;
                gb->hblankDuration = T_CYCLES_PER_SCANLINE - T_CYCLES_PER_MODE2 - gb->cyclesSinceLastMode;

                switchModePPU(gb, PPU_MODE_0);
            }

            break;
        case PPU_MODE_0: {
            /* HBLANK */
            if (gb->cyclesSinceLastMode == gb->hblankDuration) {
                /* End of scanline */
                uint8_t currentScanlineNumber = gb->cyclesSinceLastFrame / T_CYCLES_PER_SCANLINE;
                /* Set fetcher Y */
                gb->fetcherY++;
                if (currentScanlineNumber == 144) {
                    /* End of frame rendering
                     * switch to vblank now */
                    gb->fetcherY = 0;
                    gb->renderingWindow = false;
                    gb->lyWasWY = false;
                    gb->windowYCounter = 0;

                    switchModePPU(gb, PPU_MODE_1);
                    requestInterrupt(gb, INTERRUPT_VBLANK);
                } else switchModePPU(gb, PPU_MODE_2);

            } else if (gb->cyclesSinceLastMode == gb->hblankDuration - 6) {
                /* LY register gets incremented 6 dots before the *true* increment */
                gb->IO[R_LY]++;
                updateSTAT(gb, STAT_UPDATE_LY_LYC);
            }

            break;
        }
        case PPU_MODE_1: {
            /* VBLANK */
            /* LY Resets to 0 after 4 cycles on line 153
             * LY=LYC Interrupt is triggered 12 cycles after line 153 begins */
            unsigned cycleAtLYReset = T_CYCLES_PER_VBLANK - T_CYCLES_PER_SCANLINE + 4;
            unsigned cycleAtLYCInterrupt = T_CYCLES_PER_VBLANK - T_CYCLES_PER_SCANLINE + 12;
            if (cycleAtLYReset != gb->cyclesSinceLastMode && gb->cyclesSinceLastMode %
                T_CYCLES_PER_SCANLINE == T_CYCLES_PER_SCANLINE - 6) {

                /* LY register Increments 6 cycles before *true* LY, only when
                 * the line is other than 153, because on line 153 it resets early
                 * LY also stays 0 at the end of line 153 so dont increment when its 0 */
                if (gb->IO[R_LY] != 0) {
                    gb->IO[R_LY]++;
                }
                updateSTAT(gb, STAT_UPDATE_LY_LYC);
            } else if (gb->cyclesSinceLastMode == T_CYCLES_PER_VBLANK) {
                /* vblank has ended */
                switchModePPU(gb, PPU_MODE_2);
            } else if (gb->cyclesSinceLastMode == cycleAtLYReset) {
                gb->IO[R_LY] = 0;
            } else if (gb->cyclesSinceLastMode == cycleAtLYCInterrupt) {
                /* The STAT update can be issued now */
                updateSTAT(gb, STAT_UPDATE_LY_LYC);
            }
            break;
        }
    }
}

/* PPU Enable & Disable */

void enablePPU(GB* gb) {
    if (gb->ppuEnabled) return;
    gb->ppuEnabled = true;
    switchModePPU(gb, PPU_MODE_2);

    /* Skip first frame after LCD is turned on */
    gb->skipFrame = true;

    /* Reset frame counter so the whole emulator is again aligned with the PPU frames */
    gb->cyclesSinceLastFrame = 0;
}

void disablePPU(GB* gb) {
    if (!gb->ppuEnabled) return;
    if (gb->ppuMode != PPU_MODE_1) {
        /* This is dangerous for any game/rom to do on real hardware
         * as it can damage hardware */
        printf("[WARNING] Turning off LCD when not in VBlank can damage a real gameboy\n");
    }

    gb->ppuEnabled = false;
    gb->IO[R_LY] = 0;

    /* Set STAT mode to 0 */
    switchModePPU(gb, PPU_MODE_0);
}

/* -------------------- */

void clearFIFO(FIFO *fifo) {
    fifo->count = 0;
    fifo->nextPopIndex = 0;
    fifo->nextPushIndex = 0;
}

void pushFIFO(FIFO* fifo, FIFO_Pixel pixel) {
    if (fifo->count == FIFO_MAX_COUNT) {
        // printf("FIFO Error : Pushing beyond max limit\n");
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
        // printf("FIFO Error : Popping when no pixels are pushed\n");
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

FIFO_Pixel peekFIFO(FIFO* fifo, uint8_t index) {
    FIFO_Pixel pixel;

    if (fifo->count < index + 1) {
        // printf("FIFO Error : Peek index out of range\n");
        return pixel;
    }

    /* Handle wrapping */
    if (fifo->nextPopIndex + index >= FIFO_MAX_COUNT)
        pixel = fifo->contents[(fifo->nextPopIndex + index) - FIFO_MAX_COUNT];

    else pixel = fifo->contents[fifo->nextPopIndex + index];
    return pixel;
}

void insertFIFO(FIFO* fifo, FIFO_Pixel pixel, uint8_t index) {
    if (fifo->count < index + 1) {
        // printf("FIFO Error : Insert index out of range\n");
        return;
    }

    if (fifo->nextPopIndex + index >= FIFO_MAX_COUNT)
        fifo->contents[(fifo->nextPopIndex + index) - FIFO_MAX_COUNT] = pixel;

    else fifo->contents[fifo->nextPopIndex + index] = pixel;
}

void syncDisplay(GB* gb, unsigned int cycles) {
    /* We sync the display by running the PPU for the correct number of
     * dots (1 dot = 1 tcycle in normal speed) */
    if (!gb->ppuEnabled) {
        /* Keep locking to framerate even if PPU is off */
        for (unsigned int i = 0; i < cycles; i++) {
            gb->cyclesSinceLastFrame++;

            if (gb->cyclesSinceLastFrame == T_CYCLES_PER_FRAME) {
                gb->cyclesSinceLastFrame = 0;

                /* On CGB and DMG, the screen goes blank or white when the PPU is disabled */
                SDL_SetRenderDrawColor(gb->sdl_renderer, 255, 255, 255, 255);
                SDL_RenderClear(gb->sdl_renderer);
                SDL_RenderPresent(gb->sdl_renderer);

                lockToFramerate(gb);
            }
        }

        return;
    }

    for (unsigned int i = 0; i < cycles; i++) {
        gb->cyclesSinceLastFrame++;
#ifdef DEBUG_PRINT_PPU
        printf("[m%d|ly%03d|fcy%05d|mcy%04d|fifoc%d|lastX%03d|type %s]\n", gb->ppuMode, gb->IO[R_LY], gb->cyclesSinceLastFrame, gb->cyclesSinceLastMode, gb->BackgroundFIFO.count, gb->nextRenderPixelX, gb->renderingWindow ? "win" : "bg");
#endif
        advancePPU(gb);

        if (gb->cyclesSinceLastFrame == T_CYCLES_PER_FRAME) {
            /* End of frame */
            gb->cyclesSinceLastFrame = 0;
            /* Draw frame */

            if (gb->skipFrame) {
                gb->skipFrame = false;
            } else {
                SDL_RenderPresent(gb->sdl_renderer);
            }
            lockToFramerate(gb);
        }
    }
}
