#ifndef megagbc_display_h
#define megagbc_display_h
#include "../include/vm.h"
#include <SDL2/SDL.h>

#define MIN_RES_HEIGHT_PX 144 
#define MIN_RES_WIDTH_PX 160
#define HEIGHT_PX MIN_RES_HEIGHT_PX * 5
#define WIDTH_PX  MIN_RES_WIDTH_PX * 5

#define CYCLES_PER_FRAME 70224

int initSDL(VM* vm);
void freeSDL(VM* vm);
void handleSDLEvents(VM* vm);

void syncDisplay(VM* vm, unsigned int cycleUpdate);

#endif
