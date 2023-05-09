#include <gba/gba.h>
#include <gba/arm7tdmi.h>
#include <gba/gamepak.h>
#include <gba/debugGBA.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void initialiseGBA(GBA* gba, GamePak* gamepak) {
	gba->gamepak = gamepak;
	gba->run = false;

	/* Allocate memory for components */
	uint8_t* IWRAM 		= (uint8_t*)malloc(0x8000);			// 32 KB
	uint8_t* EWRAM 		= (uint8_t*)malloc(0x40000); 		// 256 KB
	uint8_t* IO    		= (uint8_t*)malloc(0x3FF);
	uint8_t* PaletteRAM = (uint8_t*)malloc(0x400);
	uint8_t* VRAM 		= (uint8_t*)malloc(0x18000);
	uint8_t* OAM 		= (uint8_t*)malloc(0x400);

	if (!IWRAM || !EWRAM || !IO || !PaletteRAM || !VRAM || !OAM) {
		printf("[FATAL] Error allocating memory for GBA components\n");
		exit(89);
	}

	gba->IWRAM 		= IWRAM;
	gba->EWRAM 		= EWRAM;
	gba->IO    		= IO;
	gba->PaletteRAM = PaletteRAM;
	gba->VRAM 		= VRAM;
	gba->OAM 		= OAM;


	/* Waitstates - Default Timings on GBA startup (WAITCNT is initialised to 0000/Int.Mem Control) */
	gba->WS0_N = 4; 	gba->WS1_N = 4; 	gba->WS2_N = 4; 	gba->WSRAM = 4;
	gba->WS0_S = 2;		gba->WS1_S = 4;		gba->WS2_N = 8; 	gba->WEWRAM = 2;
	
	gba->lastAccessAddress = 0;

	/* Initialising functions */
	initialiseCPU(gba);

#ifdef DEBUG_ENABLED
	initDissembler();
#endif
}

void freeGBA(GBA* gba) {
	free(gba->IWRAM);
	free(gba->EWRAM);
	free(gba->IO);
	free(gba->PaletteRAM);
	free(gba->VRAM);
	free(gba->OAM);

	gba->IWRAM = NULL;
	gba->EWRAM = NULL;
	gba->IO = NULL;
	gba->PaletteRAM = NULL;
	gba->VRAM = NULL;
	gba->OAM = NULL;
}

void startGBAEmulator(GamePak* gamepak) {
	GBA gba;
	initialiseGBA(&gba, gamepak);

	gba.run = true;
	
	for (;gba.run;) {
		stepCPU(&gba);
		usleep(50000);
	}

	freeGBA(&gba);
}

/* -------- Bus Functions --------- */

static inline uint32_t littleEndian32(uint8_t* ptr) {
	return (uint32_t)((ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0]);
}

uint32_t busRead32(GBA* gba, uint32_t address) {
	/* We're reading a 32 bit value from the given address, appropriate clock cycles are 
	 * consumed based on the bus width of the region */

	bool isSequential = gba->lastAccessAddress + sizeof(uint32_t) == address;

	if (address >= EXT_ROM0_32MB && address <= EXT_ROM2_32MB_END) {
		uint32_t relativeAddress;
		uint8_t WS_S, WS_N;

		switch ((address >> 24) & 0xF) {
			case 0x8: 
				relativeAddress = address - EXT_ROM0_32MB;
				WS_S = gba->WS0_S;
				WS_N = gba->WS0_N;
				break;
			case 0xA:
				relativeAddress = address - EXT_ROM1_32MB;
				WS_S = gba->WS1_S;
				WS_N = gba->WS1_N;
				break;
			case 0xC:
				relativeAddress = address - EXT_ROM2_32MB;
				WS_S = gba->WS2_S;
				WS_N = gba->WS2_N;
				break;
		}

		if (relativeAddress > (gba->gamepak->size - 1)) {
			printf("[WARNING] Read attempt from gamepak to an invalid address\n");
			return 0xFFFFFFFF;
		}

		/* GamePak ROM has a 16 bit bus, this means a 32 bit read = 1S/N + 1S */
		ACCESS_CYCLE(gba, isSequential, WS_S, WS_N);
		S_CYCLE(gba, WS_S);
		gba->lastAccessAddress = address;

		return littleEndian32(&gba->gamepak->allocated[relativeAddress]);
	}
	return 0xFFFFFFFF;
}

void busWrite32(GBA* gba, uint32_t address, uint32_t data) {

}

uint16_t busRead16(GBA* gba, uint32_t address) {
	return 0;
}

void busWrite16(GBA* gba, uint32_t address, uint16_t data) {

}

/* -------------------------------- */
