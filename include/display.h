#ifndef megagbc_display_h
#define megagbc_display_h
#include <SDL2/SDL.h>

#define DISPLAY_SCALING 3
#define HEIGHT_PX 144
#define WIDTH_PX  160

#define FIFO_MAX_COUNT 8

#define T_CYCLES_PER_FRAME 70224
#define T_CYCLES_PER_SCANLINE 456
#define DEFAULT_FRAMERATE 59.7275

/* 1 T-Cycle = 1 Dot in normal speed mode */
/* Mode 0 and 3 dont have a fixed dot count */

#define T_CYCLES_PER_MODE2 80
#define T_CYCLES_PER_VBLANK 4560		// or MODE1

struct VM;

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
	FETCHER_PUSH
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
	uint8_t colorID;
	uint8_t colorPalette;
	/* TODO - Add sprite and background priority */
} FIFO_Pixel;

typedef struct {
	FIFO_Pixel contents[FIFO_MAX_COUNT];		/* Each FIFO can hold upto 16 pixels */
	uint8_t nextPushIndex;					
	uint8_t nextPopIndex;
	uint8_t count;
} FIFO;

void pushFIFO(FIFO* fifo, FIFO_Pixel pixel);
FIFO_Pixel popFIFO(FIFO* fifo);
void clearFIFO(FIFO* fifo);

void syncDisplay(struct VM* vm, unsigned int cycles);
void enablePPU(struct VM* vm);
void disablePPU(struct VM* vm);

#endif
