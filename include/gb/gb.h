#ifndef MGBC_VM_H
#define MGBC_VM_H

#include <SDL2/SDL.h>
#include <stdint.h>
#include <unistd.h>
#include <gb/cartridge.h>
#include <gb/mbc.h>
#include <gb/cpu.h>
#include <gb/display.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Utility macros */
#define SET_BIT(byte, bit) byte |= 1 << bit
#define GET_BIT(byte, bit) ((byte >> bit) & 1)
#define CLEAR_BIT(byte, bit) byte &= ~(1 << bit)

/* Cycles till the DIV timer is increment */
#define T_CYCLES_PER_DIV      256

typedef enum {
    ROM_N0_16KB = 0x0000,                       /* 16KB ROM Bank number 0 (from cartridge) */
    ROM_N0_16KB_END = 0x3FFF,
    ROM_NN_16KB = 0x4000,                       /* 16KB Switchable ROM Bank area (from cartridge) */
    ROM_NN_16KB_END = 0x7FFF,
    VRAM_N0_8KB = 0x8000,                       /* 8KB Switchable Video memory */
    VRAM_N0_8KB_END = 0x9FFF,
    RAM_NN_8KB = 0xA000,                        /* 8KB Switchable RAM Bank area (from cartridge) */
    RAM_NN_8KB_END = 0xBFFF,
    WRAM_N0_4KB = 0xC000,                       /* 4KB Work RAM */
    WRAM_N0_4KB_END = 0xCFFF,
    WRAM_NN_4KB = 0xD000,                       /* 4KB Switchable Work RAM (not in cartridge) */
    WRAM_NN_4KB_END = 0xDFFF,
    ECHO_N0_8KB = 0xE000,                       /* None of our business lol */
    ECHO_N0_8KB_END = 0xFDFF,
    OAM_N0_160B = 0xFE00,                       /* Where sprites (or screen objects) are stored */
    OAM_N0_160B_END = 0xFE9F,
    UNUSABLE_N0 = 0xFEA0,                       /* One more useless area */
    UNUSABLE_N0_END = 0xFEFF,
    IO_REG = 0xFF00,                            /* A place for input/output registers */
    IO_REG_END = 0xFF7F,
    HRAM_N0 = 0xFF80,                           /* High Ram or 'fast ram' */
    HRAM_N0_END = 0xFFFE,
    R_IE = 0xFFFF                               /* register which stores if interrupts are enabled */
} MEM_ADDR;

typedef enum {
    EMU_DMG,                    /* Emulate gameboy */
    EMU_CGB                     /* Emulate gameboy color */
} EMULATION_MODE;

/* Joypad key select modes */

typedef enum {
    JOYPAD_SELECT_DIRECTION_ACTION,
    JOYPAD_SELECT_ACTION,
    JOYPAD_SELECT_DIRECTION,
    JOYPAD_SELECT_NONE
} JOYPAD_SELECT;

/* IO Port Registers */
typedef enum {
    R_P1_JOYP = 0x00,
    R_SB      = 0x01,
    R_SC      = 0x02,
    R_DIV     = 0x04,
    R_TIMA    = 0x05,
    R_TMA     = 0x06,
    R_TAC     = 0x07,
    R_IF      = 0x0F,
    R_LCDC    = 0x40,
    R_STAT    = 0x41,
    R_SCY     = 0x42,
    R_SCX     = 0x43,
    R_LY	  = 0x44,
    R_LYC     = 0x45,
    R_DMA     = 0x46,
    R_BGP     = 0x47,
    R_OBP0    = 0x48,
    R_OBP1    = 0x49,
    R_WY      = 0x4A,
    R_WX      = 0x4B,
	R_KEY0    = 0x4C,
	R_KEY1    = 0x4D,
    R_VBK     = 0x4F,
    R_HDMA1   = 0x51,
    R_HDMA2   = 0x52,
    R_HDMA3   = 0x53,
    R_HDMA4   = 0x54,
    R_HDMA5   = 0x55,
    R_BCPS    = 0x68,
    R_BCPD    = 0x69,
    R_OCPS    = 0x6A,
    R_OCPD    = 0x6B,
	R_OPRI    = 0x6C,
    R_SVBK    = 0x70
} HREG;

struct GB {
	/* ---------------- IMGUI --------------- */
	void* imgui_main_context; 				/* Main IMGUI Context for MENU */
	void* imgui_secondary_context; 			/* Secondary IMGUI Context for second window */
	SDL_Window* imgui_secondary_sdl_window; /* Secondary IMGUI SDL Window and Renderer */
	SDL_Renderer* imgui_secondary_sdl_renderer;
	void* imgui_gui_state; 					/* Gui State */
    /* ---------------- SDL ----------------- */
    SDL_Window* sdl_window;					/* The window */
    SDL_Renderer* sdl_renderer;             /* Renderer */
    unsigned long ticksAtStartup;			/* Stores the ticks at emulator startup (rom boot) */
    unsigned long ticksAtLastRender;		/* Used to calculate how much time has passed
                                               since last sdl frame render */
    uint8_t joypadDirectionBuffer;			/* Stores joypad direction button states */
    uint8_t joypadActionBuffer;				/* Stores joypad action button states */
    JOYPAD_SELECT joypadSelectedMode;
    /* -------------- Emulator ------------- */
    Cartridge* cartridge;
    EMULATION_MODE emuMode;                 /* Which behaviour are we emulating, dmg, cgb, ect */
    bool run;                               /* A flag that when set to false, quits the emulator */
    bool paused;
    bool IME;                               /* Interrupt Master Enable Flag */
    unsigned long lastDIVSync;              /* Holds the clock's state when DIV timer was last synced
                                             * this helps in getting the cycles elapsed */
    unsigned long lastTIMASync;             /* Same but for the TIMA timer */
    unsigned long clock;                    /* Main clock of the whole emulator
                                               Counts in T-Cycles */
    bool scheduleHaltBug;				    /* If set to true,the CPU recreates the halt bug */
    bool scheduleDMA;                       /* If set to true, schedules the DMA to be enabled */
    bool doingDMA;
    uint16_t mCyclesSinceDMA;               /* M-Cycles elapsed since DMA began */
    uint16_t scheduled_dmaSource;           /* Source of the DMA about to be started */
    uint8_t scheduled_dmaTimer;             /* Timer that counts no. of tcycles till the DMA begins
                                               when the DMA is scheduled */
    uint16_t dmaSource;                     /* DMA Source Address */

	bool doingSpeedSwitch; 					/* Doing speed switch (CGB Only) */
	bool isDoubleSpeedMode; 				/* Is the emulator in double speed mode? (CGB Only) */
	bool scheduleGDMA; 						/* Start GDMA/HDMA at next cycle*/
	bool scheduleHDMA;
	bool doingGDMA; 						/* General DMA (CGB Only) */
	bool doingHDMA; 						/* HBlank DMA (CGB Only) */
	bool stepHDMA; 							/* We can proceed to step HDMA by 1 block */
	uint16_t ghdmaSource; 					/* GDMA/HDMA Source, Destination, Length & Block Index */
	uint16_t ghdmaDestination;
	uint8_t ghdmaLength;
	uint8_t ghdmaIndex;

    /* ---------------- CPU ---------------- */
    uint8_t GPR[GP_COUNT];
    uint16_t PC;                        /* Program Counter */
    bool scheduleInterruptEnable;       /* If set to true, it enables interrupts at the
                                           dispatch of the next instruction */
    bool haltMode;						/* If set to true, the CPU enters the halt
                                           procedure */
    /* ------------- Memory ---------------- */
    uint8_t* vram;                      /* Stores VRAM along with all banks in blocks of 0x2000 */
    uint8_t* wram;                      /* Stores WRAM along with all banks in blocks of 0x1000 */
    uint8_t OAM[0xA0];                  /* OAM memory */
    uint8_t IO[0x80];                   /* IO Memory */
    uint8_t hram[0x7F];                 /* High RAM */
    uint8_t IE;                         /* Interrupt Enable Register */

    /* Selected bank fields only account for the selected bank numbers on the switchable
     * banking spaces. On DMG, selected WRAM Bank will be always 1 and selected VRAM bank will
     * always be 0 */
    uint8_t selectedWRAMBank;
    uint8_t selectedVRAMBank;
    void* memController;                /* Memory Bank Controller */
    MBC_TYPE memControllerType;
    /* ---------------- PPU ---------------- */
    FIFO BackgroundFIFO;
    FIFO OAMFIFO;
    PPU_MODE ppuMode;
    unsigned int hblankDuration;            /* HBlank duration depends on mode 3 duration,
                                               this is set by mode 3 at every scanline to
                                               set the hblank wait cycle duration */
    bool ppuEnabled;
    bool skipFrame;							/* Skips a frame render */
    uint8_t currentFetcherTask;
    uint16_t fetcherTileAddress;            /* Address of the current tile the fetcher is on */
    uint8_t fetcherTileAttributes;          /* Attributes of the current tile the fetcher is on */
    uint8_t fetcherX;                       /* X coordinate of the tile in a row */
    uint8_t fetcherY;                       /* Y coordinate of the tile in pixels */
    /* Note: FetcherX and FetcherY do not reflect the
     * true coordinates of the tile row being rendered,
     * scrolling is applied to these values and then
     * they are used */
    uint8_t fetcherTileRowLow;              /* Lower byte of the fetcher tile row data */
    uint8_t fetcherTileRowHigh;             /* Upper .... */
    bool firstTileInScanline;               /* Is true if the current tile the fetcher is on
                                               is the first tile in the current scanline */
    uint8_t windowYCounter;                 /* Internal window Y counter, it counts the current
                                               Y coordinate of the window scanline */
    bool lyWasWY;                           /* Set to true when LY = WY, reset every frame */
    bool renderingWindow;
    bool doOptionalPush;                    /* If set to true, the fetcher does a push on the second
                                               dot */
    uint8_t pauseDotClock;                  /* If this is non zero, the ppu is paused for
                                               that many dots */
    uint8_t nextRenderPixelX;             /* X coordinate of the next pixel to be rendered */
    uint8_t nextPushPixelX;               /* X coordinate of the next pixel to be pushed */
    uint8_t pixelsToDiscard;                /* Fixed offset of pixels to discard at start of a
                                             * scanline. This is decided by the lower 3 bits of
                                             * SCX for BG tiles and when WX < 7 for window tiles */
    uint8_t preservedFetcherTileLow;
    uint8_t preservedFetcherTileHigh;
    uint8_t preservedFetcherTileAttributes;
    uint8_t* spriteData;                    /* Pointer to the data of the sprite being rendered rn */
    bool renderingSprites;
    uint8_t oamDataBuffer[50];              /* OAM Data is read and written to this area on every
                                               scanline in mode 2, it stores only 10 sprites at
                                               max as its the limit, each sprite occupies 5 bytes
                                               (4 bytes for the normal data + 5th byte has the
                                               index in OAM) */
    uint8_t spritesInScanline;              /* Number of sprites to be rendered in the current
                                               scanline */
    uint8_t spriteSize;                     /* 0 = 8x8, 1 = 8x16, read every scanline */
    bool isLastSpriteOverlap;
    uint8_t lastSpriteOverlapPushIndex;     /* Index of the last overlapping sprite that was pushed fully */
    int lastSpriteOverlapX;                 /* X Coordinate of the last overlapping sprite that was pushed fully */
    uint8_t* bgColorRAM;                    /* 64 Byte long color ram which stores CGB palettes */
    uint8_t* spriteColorRAM;                /* ^^^ for sprites */
    uint8_t currentBackgroundCRAMIndex;     /* Current byte value in color ram which can be
                                               addressed by BCPS */
    uint8_t currentSpriteCRAMIndex;         /* ^^^^ addressed by OCPS */
    unsigned int cyclesSinceLastFrame;      /* Holds the cycles passed since last frame was drawn */
    unsigned int cyclesSinceLastMode;
    bool lockVRAM;							/* Locks CPU from accessing VRAM */
    bool lockOAM;							/*						 -> OAM */
    bool lockPalettes;						/*						 -> CGB Palettes */
};

typedef struct GB GB;

/* Loads in the cartridge into the VM and starts the overall emulator */
void startGBEmulator(Cartridge* cartridge);

void pauseGBEmulator(GB* gb);
void unpauseGBEmulator(GB* gb);
/* will perform a memory cleanup by freeing the VM state and then safely exiting */
void stopGBEmulator(GB* gb);

/* Increments the cycle count by 4 tcycles and syncs all hardware to act accordingly if necessary */
void cyclesSync_4(GB* gb);

/* Joypad */

/* Updates the register by writing correct values to the lower nibble */
void updateJoypadRegBuffer(GB* gb, JOYPAD_SELECT mode);

/* SDL */
int initSDL(GB* gb);
void freeSDL(GB* gb);
void handleSDLEvents(GB* gb);

/* Sync timer */
void syncTimer(GB* gb);
void incrementTIMA(GB* gb);

/* DMA/GDMA/HDMA */
void scheduleDMATransfer(GB* gb, uint8_t byte);
void scheduleGDMATransfer(GB* gb, uint16_t source, uint16_t dest, uint8_t length);
void scheduleHDMATransfer(GB* gb, uint16_t source, uint16_t dest, uint8_t length);
void cancelHDMATransfer(GB* gb);
/* Utility */
unsigned long clock_u();

#ifdef __cplusplus
}
#endif
#endif
