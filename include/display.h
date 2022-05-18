#ifndef megagbc_display_h
#define megagbc_display_h
#include "../include/vm.h"
#include <SDL2/SDL.h>

#define DISPLAY_SCALING 3

#define MIN_RES_HEIGHT_PX 144 
#define MIN_RES_WIDTH_PX 160
#define HEIGHT_PX MIN_RES_HEIGHT_PX * DISPLAY_SCALING
#define WIDTH_PX  MIN_RES_WIDTH_PX * DISPLAY_SCALING

#define T_CYCLES_PER_FRAME 70224
#define M_CYCLES_PER_FRAME 17556	// 70224/4
#define DEFAULT_FRAMERATE 59.7275

void syncDisplay(VM* vm);

#endif
