#ifndef gb_display_h
#define gb_display_h
#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISPLAY_SCALING 4
#define HEIGHT_PX 144
#define WIDTH_PX  160

#define FIFO_MAX_COUNT 8

#define T_CYCLES_PER_SEC 4194304
#define T_CYCLES_PER_FRAME 70224
#define T_CYCLES_PER_SCANLINE 456
#define DEFAULT_FRAMERATE 59.7275

/* 1 T-Cycle = 1 Dot in normal speed mode */
/* Mode 0 and 3 dont have a fixed dot count */

#define T_CYCLES_PER_MODE2 80
#define T_CYCLES_PER_VBLANK 4560		// or MODE1

struct GB;

typedef enum {
    PPU_MODE_0,		// Hblank
    PPU_MODE_1,		// Vblank
    PPU_MODE_2,		// OAM Scan
    PPU_MODE_3		// Drawing Pixels
} PPU_MODE;

typedef enum {
    FETCHER_GET_TILE,
    FETCHER_GET_DATA_LOW,
    FETCHER_GET_DATA_HIGH,
    FETCHER_SLEEP,
    FETCHER_PUSH,
    FETCHER_OPTIONAL_PUSH
} FETCHER_STATE;

/* Different types of STAT updates */

typedef enum {
    STAT_UPDATE_LY_LYC,
    STAT_UPDATE_SWITCH_MODE0,
    STAT_UPDATE_SWITCH_MODE1,
    STAT_UPDATE_SWITCH_MODE2,
    STAT_UPDATE_SWITCH_MODE3
} STAT_UPDATE_TYPE;

typedef struct {
    /* Internal */
    uint8_t screenX;
    uint8_t screenY;

    /* Main Data */
    uint8_t colorID;
    uint8_t colorPalette;                       /* CGB only normally, also used to store OBP color
                                                   palette*/
    uint8_t bgPriority;                         /* On DMG this stores the OAM priority bit,
                                                   on CGB it also stored bg map attribute bit
                                                   for background pixels */
} FIFO_Pixel;

typedef struct {
    FIFO_Pixel contents[FIFO_MAX_COUNT];		/* Each FIFO can hold upto 16 pixels */
    uint8_t nextPushIndex;
    uint8_t nextPopIndex;
    uint8_t count;
} FIFO;

void pushFIFO(FIFO* fifo, FIFO_Pixel pixel);
FIFO_Pixel popFIFO(FIFO* fifo);
FIFO_Pixel peekFIFO(FIFO* fifo, uint8_t index);
void insertFIFO(FIFO* fifo, FIFO_Pixel pixel, uint8_t index);
void clearFIFO(FIFO* fifo);

void syncDisplay(struct GB* gb);
void enablePPU(struct GB* gb);
void disablePPU(struct GB* gb);

#ifdef __cplusplus
}
#endif

#endif
