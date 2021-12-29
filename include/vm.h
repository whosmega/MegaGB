#ifndef MGBC_VM_H
#define MGBC_VM_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <stdint.h>
#include <unistd.h>
#include "../include/cartridge.h"
#include "../include/mbc.h"

typedef enum {
    /* General purpose Registers */
    R8_A, R8_F,
    R8_B, R8_C,
    R8_D, R8_E,
    R8_H, R8_L,
    
    R8_SP_HIGH, R8_SP_LOW,
    GP_COUNT
} GP_REG;

typedef enum {
    FLAG_C,
    FLAG_H,
    FLAG_N,
    FLAG_Z
} FLAG;

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
    INTERRUPT_ENABLE_REG8 = 0xFFFF              /* register which stores if interrupts are enabled */
} MEM_ADDR;

/* Defining macros for 16 bit registers, only for readability purposes */
#define R16_AF R8_A
#define R16_BC R8_B
#define R16_DE R8_D
#define R16_HL R8_H
#define R16_SP R8_SP_HIGH

struct VM {
    /* ---------------- SDL ----------------- */
    SDL_Window* sdl_window;             /* The window */
    SDL_Renderer* sdl_renderer;             /* Renderer */
    /* -------------------------------------- */
    Cartridge* cartridge;
    bool run;                          /* A flag that when set to false, quits the emulator */
    bool IME;                           /* Interrupt Master Enable Flag */
    unsigned int cyclesSinceLastFrame;
    unsigned long clock;
    /* ---------------- CPU ---------------- */
    uint8_t GPR[GP_COUNT];
    uint16_t PC;                        /* Program Counter */
    /* ------------- Memory ---------------- */
    uint8_t MEM[0xFFFF + 1];
    void* memController;                 /* Memory Bank Controller */
    MBC_TYPE memControllerType;
};

typedef struct VM VM;

/* Loads in the cartridge into the VM and starts the overall emulator */
void startEmulator(Cartridge* cartridge);
/* will perform a memory cleanup by freeing the VM state and then safely exiting */
void stopEmulator(VM* vm);
/* Increments the cycle count and syncs all hardware to act accordingly if necessary */
void cyclesSync(VM* vm, unsigned int cycles);
#endif
