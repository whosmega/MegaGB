#ifndef gb_cpu_h
#define gb_cpu_h

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GB;

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
    INTERRUPT_VBLANK,
    INTERRUPT_LCD_STAT,
    INTERRUPT_TIMER,
    INTERRUPT_SERIAL,
    INTERRUPT_JOYPAD,

    INTERRUPT_COUNT
} INTERRUPT;

/* Defining macros for 16 bit registers, only for readability purposes */
#define R16_AF R8_A
#define R16_BC R8_B
#define R16_DE R8_D
#define R16_HL R8_H
#define R16_SP R8_SP_HIGH

/* Resets the registers in the GBC */
void resetGBC(struct GB* gb);
/* Resets the registers in the GB */
void resetGB(struct GB* gb);
/* This function is invoked to run the CPU thread */
void dispatch(struct GB* gb);

/* Reading and Writing bus routines (instant) */
void writeAddr(struct GB* gb, uint16_t addr, uint8_t byte);
uint8_t readAddr(struct GB* gb, uint16_t addr);

/* Function to request an interrupt when necessary */
void requestInterrupt(struct GB* gb, INTERRUPT interrupt);

#ifdef __cplusplus
}
#endif

#endif
