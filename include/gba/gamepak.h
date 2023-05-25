#ifndef gba_gamepak_h
#define gba_gamepak_h

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

/* Structure which contains game pak information for GBA */

typedef struct {
	uint8_t* allocated;
	size_t size;
	bool inserted;
} GamePak;

bool initGamePak(GamePak* gamepak, uint8_t* allocated, size_t size);
void freeGamePak(GamePak* gamepak);

#endif
