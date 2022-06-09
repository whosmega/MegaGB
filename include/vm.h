#ifndef MGBC_VM_H
#define MGBC_VM_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <stdint.h>
#include <unistd.h>
#include "../include/cartridge.h"
#include "../include/mbc.h"
#include "../include/cpu.h"
#include "../include/display.h"

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
    INTERRUPT_ENABLE = 0xFFFF                   /* register which stores if interrupts are enabled */
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

/* IO Port Register Macros */
#define R_P1_JOYP	0xFF00
#define R_SB        0xFF01
#define R_SC        0xFF02
#define R_DIV       0xFF04
#define R_TIMA      0xFF05
#define R_TMA       0xFF06
#define R_TAC       0xFF07
#define R_IF        0xFF0F
#define R_LCDC		0xFF40
#define R_STAT		0xFF41
#define R_SCY       0xFF42
#define R_SCX       0xFF43
#define R_LY		0xFF44
#define R_LYC		0xFF45
#define R_BGP       0xFF47
#define R_OBP0      0xFF48
#define R_OBP1      0xFF49
#define R_WY        0xFF4A
#define R_WX        0xFF4B
#define R_VBK		0xFF4F
#define R_BCPS		0xFF68
#define R_BCPD		0xFF69
#define R_OCPS		0xFF6A
#define R_OCPD		0xFF6B
#define R_SVBK		0xFF70
#define R_IE        INTERRUPT_ENABLE

struct VM {
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
    bool IME;                               /* Interrupt Master Enable Flag */ 
    unsigned long lastDIVSync;              /* Holds the clock's state when DIV timer was last synced
                                             * this helps in getting the cycles elapsed */
    unsigned long lastTIMASync;             /* Same but for the TIMA timer */
    unsigned long clock;                    /* Main clock of the whole emulator 
											   Counts in T-Cycles */
    /* ---------------- CPU ---------------- */
    uint8_t GPR[GP_COUNT];
    uint16_t PC;                        /* Program Counter */
    bool scheduleInterruptEnable;       /* If set to true, it enables interrupts at the
                                           dispatch of the next instruction */
	bool haltMode;						/* If set to true, the CPU enters the halt 
										   procedure */
	bool scheduleHaltBug;				/* If set to true,the CPU recreates the halt bug */
    /* ------------- Memory ---------------- */
    uint8_t MEM[0xFFFF + 1];
	uint8_t* wramBanks;         	    /* 7 Banks for WRAM when on CGB mode */
	uint8_t* vramBank;			        /* Switchable VRAM Bank when on CGB mode */
    void* memController;                /* Memory Bank Controller */
    MBC_TYPE memControllerType;
	/* ---------------- PPU ---------------- */
	FIFO BackgroundFIFO;
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
    bool doOptionalPush;                    /* If set to true, the fetcher does a push on the second
                                               dot */
    uint8_t pauseDotClock;                  /* If this is non zero, the ppu is paused for 
                                               that many dots */
    uint8_t lastRenderedPixelX;             /* X coordinate of the last rendered pixel */
    uint8_t lastPushedPixelX;               /* X coordinate of the last pushed pixel */
    uint8_t scxOffsetForScanline;           /* Fixed offset or the lower 3 bits of scx during 
                                               the start of a scanline */
    uint8_t* colorRAM;                      /* 64 Byte long color ram which stores CGB palettes */
    uint8_t currentCRAMIndex;               /* Current byte value in color ram which can be 
                                               addressed by BCPD */
    unsigned int cyclesSinceLastFrame;      /* Holds the cycles passed since last frame was drawn */
	unsigned int cyclesSinceLastMode;
	bool lockVRAM;							/* Locks CPU from accessing VRAM */
	bool lockOAM;							/*						 -> OAM */
	bool lockPalettes;						/*						 -> CGB Palettes */
};

typedef struct VM VM;

/* Loads in the cartridge into the VM and starts the overall emulator */
void startEmulator(Cartridge* cartridge);

/* will perform a memory cleanup by freeing the VM state and then safely exiting */
void stopEmulator(VM* vm);

/* Increments the cycle count by 4 tcycles and syncs all hardware to act accordingly if necessary */
void cyclesSync_4(VM* vm);

/* Joypad */

/* Updates the register by writing correct values to the lower nibble */
void updateJoypadRegBuffer(VM* vm, JOYPAD_SELECT mode);

/* SDL */
int initSDL(VM* vm);
void freeSDL(VM* vm);
void handleSDLEvents(VM* vm);

/* Sync timer */
void syncTimer(VM* vm);
void incrementTIMA(VM* vm);

/* CGB Only, WRAM/VRAM bank switching */
void switchCGB_WRAM(VM* vm, uint8_t oldBankNumber, uint8_t bankNumber);
void switchCGB_VRAM(VM* vm, uint8_t oldBankNumber, uint8_t bankNumber);

/* Utility */
unsigned long clock_u();
#endif
