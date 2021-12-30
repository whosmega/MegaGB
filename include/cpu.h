#ifndef megagbc_cpu_h
#define megagbc_cpu_h

struct VM;

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
} INTERRUPTS;

/* Defining macros for 16 bit registers, only for readability purposes */
#define R16_AF R8_A
#define R16_BC R8_B
#define R16_DE R8_D
#define R16_HL R8_H
#define R16_SP R8_SP_HIGH

/* Resets the registers in the GBC */
void resetGBC(struct VM* vm);

/* This function is invoked to run the CPU thread */
void dispatch(struct VM* vm);

#endif
