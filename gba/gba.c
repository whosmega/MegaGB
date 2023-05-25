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

static inline uint32_t littleEndian32Decode(uint8_t* ptr) {
	return (uint32_t)((ptr[3] << 24) | (ptr[2] << 16) | (ptr[1] << 8) | ptr[0]);
}

static inline uint16_t littleEndian16Decode(uint8_t* ptr) {
	return (uint16_t)((ptr[1] << 8) | ptr[0]);
}

static inline void littleEndian32Encode(uint8_t* ptr, uint32_t value) {
	ptr[0] = value & 0xFF;
	ptr[1] = (value >> 8) & 0xFF;
	ptr[2] = (value >> 16) & 0xFF;
	ptr[3] = (value >> 24) & 0xFF;
}

static inline void littleEndian16Encode(uint8_t* ptr, uint16_t value) {
	ptr[0] = value & 0xFF;
	ptr[1] = (value >> 8) & 0xFF;
}

uint32_t busRead(GBA* gba, uint32_t address, uint8_t size) {
	/* We're reading a 32/16/8 bit value from the given address */	
	if (address >= EXT_ROM0_32MB && address <= EXT_ROM2_32MB_END) {
		uint32_t relativeAddress;

		switch ((address >> 24) & 0xF) {
			case 0x8: 
				relativeAddress = address - EXT_ROM0_32MB;
				break;
			case 0xA:
				relativeAddress = address - EXT_ROM1_32MB;
				break;
			case 0xC:
				relativeAddress = address - EXT_ROM2_32MB;
				break;
		}

		if (relativeAddress > (gba->gamepak->size - 1)) {
			printf("[WARNING] Read attempt from gamepak to an invalid address\n");
			return 0;
		}

		uint8_t* ptr = &gba->gamepak->allocated[relativeAddress];

		switch (size) {
			case WIDTH_32: return littleEndian32Decode(ptr);
			case WIDTH_16: return littleEndian16Decode(ptr);
			case WIDTH_8 : return *ptr;
		}
	} else if (address >= INT_WRAM_32KB && address <= INT_WRAM_32KB_END) {
		/* Read from internal work RAM */
		uint8_t* ptr = &gba->IWRAM[address - INT_WRAM_32KB];

		switch (size) {
			case WIDTH_32: return littleEndian32Decode(ptr);
			case WIDTH_16: return littleEndian16Decode(ptr);
			case WIDTH_8:  return *ptr;
		}
	}

	return 0;
}

void busWrite(GBA* gba, uint32_t address, uint32_t data, uint8_t size) {
	if (address >= INT_WRAM_32KB && address <= INT_WRAM_32KB_END) {
		/* Write to internal workram with current size and little endian formatting */
		uint8_t* ptr = &gba->IWRAM[address - INT_WRAM_32KB];

		switch (size) {
			case WIDTH_32: littleEndian32Encode(ptr, data); return;
			case WIDTH_16: littleEndian16Encode(ptr, data); return;
			case WIDTH_8:  *ptr = (uint8_t)data; return;
		}
	}
}



/* -------------------------------- */
