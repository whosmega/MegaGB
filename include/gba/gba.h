#ifndef gba_include_h
#define gba_include_h

#include <stdint.h>
#include <stddef.h> 
#include <gba/arm7tdmi.h>
#include <gba/gamepak.h>

#define N_CYCLE(gba, ws) gba->cycles += 1 + ws 				// Non Sequential Access Cycle
#define S_CYCLE(gba, ws) gba->cycles += 1 + ws 				// Sequential Access Cycle
#define I_CYCLE(gba) 	 gba->cycles += 1 					// CPU Internal Cycle
#define ACCESS_CYCLE(gba, isSequential, wsS, wsN) gba->cycles += 1 + (isSequential ? wsS : wsN)

enum {
	BIOS_ROM_16KB 		= 0x00000000,
	BIOS_ROM_16KB_END 	= 0x00003FFF,

	EXT_WRAM_256KB		= 0x02000000,
	EXT_WRAM_256KB_END  = 0x0203FFFF,

	INT_WRAM_32KB 		= 0x03000000,
	INT_WRAM_32KB_END  	= 0x03007FFF,

	IO_REG_1KB 			= 0x04000000,
	IO_REG_1KB_END 		= 0x040003FE,

	PALETTE_RAM_1KB 	= 0x05000000,
	PALETTE_RAM_1KB_END = 0x050003FF,

	VRAM_96KB 			= 0x06000000,
	VRAM_96KB_END 		= 0x06017FFF,

	OAM_1KB 			= 0x07000000,
	OAM_1KB_END 		= 0x070003FF,

	EXT_ROM0_32MB 		= 0x08000000, 				/* ROM from gamepak/flash with waitstate 0*/
	EXT_ROM0_32MB_END 	= 0x09FFFFFF,

	EXT_ROM1_32MB 		= 0x0A000000, 				/* Mirror of ROM with waitstate 1 */
	EXT_ROM1_32MB_END 	= 0x0BFFFFFF,

	EXT_ROM2_32MB		= 0x0C000000, 				/* Mirror of ROM with waitstate 2 */
	EXT_ROM2_32MB_END 	= 0x0DFFFFFF,

	EXT_SRAM_64KB		= 0x0E000000,
	EXT_SRAM_64KB_END 	= 0x0E00FFFF

	/* 0x10000000-0xFFFFFFFF unused (upper 4 bits of address bus) */
};

struct GBA {
	/* ------------------ CPU -------------------- */
	CPU_STATE cpu_state; 		/* THUMB/ARM state */
	CPU_MODE cpu_mode; 			/* Mode of the CPU */
	unsigned long cycles; 		/* Cycle counter of the CPU */
	uint32_t pipeline[3]; 		/* 3 Stage pipeline (Queue for fetched opcodes) */
	uint8_t pipelineInsertPoint;/* Point where prefetched opcode is inserted */
	uint8_t pipelineReadPoint;  /* Point where opcode to be executed is read from */
	bool skipFetch; 			/* Skip the pipeline fetch in the current pipeline cycle
								   either because the pipeline was flushed or the fetch was already
								   done internally during execution stage to emulate PC+12 */

	/* Main Regs */
	uint32_t REG[16]; 			/* Main 16 registers */
	uint32_t CPSR; 				/* Main CPSR Register */
	uint32_t SPSR; 				/* SPSR accessible in only exception modes */
	
	/* Swap-In for main registers */
	uint32_t REG_SWAP[7]; 		/* Swaps in anything from R8-R14 of the main registers
								 * when switching modes */

	/* Banked Registers */	
	uint32_t BANK_FIQ[7]; 		/* R8_FIQ, R9_FIQ, R10_FIQ, R11_FIQ, R12_FIQ, R13_FIQ, R14_FIQ */
	uint32_t BANK_SVC[2]; 		/* R13_SVC, R14_SVC */
	uint32_t BANK_ABT[2]; 		/* R13_ABT, R14_ABT */
	uint32_t BANK_IRQ[2]; 		/* R13_IRQ, R14_IRQ */
	uint32_t BANK_UND[2]; 		/* R13_UND, R14_UND */
	uint32_t BANK_SPSR[5]; 		/* SPSR_FIQ, SPSR_SVC, SPSR_ABT, SPSR_IRQ, SPSR_UND */

	void (*ARM_LUT[4096])(struct GBA* gba, uint32_t ins);		/* Lookup table with 12 bit indices
																   for ARM instructions */

	/* ----------------- Emulator ---------------- */
	GamePak* gamepak; 					/* Cartridge containing allocated code and important info
										   about the game */
	bool run; 							/* Flag used to stop the emulator and check if its running */

	/* Allocations */
	uint8_t* IWRAM;
	uint8_t* EWRAM;
	uint8_t* IO;
	
	uint8_t* PaletteRAM;
	uint8_t* VRAM;
	uint8_t* OAM;

	/* Wait States for ROM and EWRAM */
	uint8_t WS0_N; 						/* These are waitstates which can be modified using the
										   WAITCNT registers. These are sequential/non sequential
										   access times which are consumed when doing reads */
	uint8_t WS0_S;
	uint8_t WS1_N;
	uint8_t WS1_S;
	uint8_t WS2_N;
	uint8_t WS2_S;
	uint8_t WSRAM;

	uint8_t WEWRAM;
	uint32_t lastAccessAddress; 		/* Every access address is stored to calculate sequential/
										   non sequential read/writes */
	/* ------------------------------------------- */

};

typedef struct GBA GBA;

/* Bus functions */

uint32_t busRead32(GBA* gba, uint32_t address);
void busWrite32(GBA* gba, uint32_t address, uint32_t data);

/* --------------- */

void startGBAEmulator(GamePak* gamepak);

void initialiseGBA(GBA* gba, GamePak* gamepak);
void freeGBA(GBA* gba);

#endif
