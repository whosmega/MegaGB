#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <gba/gamepak.h>

bool initGamePak(GamePak* gamepak, uint8_t* allocated, long size) {
	gamepak->allocated = allocated;
	gamepak->inserted = false;
	gamepak->size = (size_t)size;
	return true;
}

void freeGamePak(GamePak* gamepak) {
	free(gamepak->allocated);
	gamepak->inserted = false;
	gamepak->size = 0;
	gamepak->allocated = NULL;
}
