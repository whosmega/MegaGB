#include "../include/vm.h"
#include "../include/display.h"
#include "../include/debug.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_scancode.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

void syncDisplay(VM* vm) {
    /* We sync the display by checking if we have to draw a frame 
     *
     * This is called every cycle update*/

    vm->cyclesSinceLastFrame++;

    if (vm->cyclesSinceLastFrame >= M_CYCLES_PER_FRAME) {
        /* We preserve any extra cycles just in case */
        vm->cyclesSinceLastFrame -= M_CYCLES_PER_FRAME;
        /* Draw frame */


		/* The emulator keeps its speed accurate by locking to the framerate
		 * Whatever has to be done (cpu execution, audio, rendering a frame) in 
		 * the interval equivalent to 1 frame render on the gameboy is done in 1 frame
		 * render on the emulator, the remaining time is waited for on the emulator to
		 * sync with the time on the gameboy */
		unsigned long ticksElapsed = (clock_u() - vm->ticksAtStartup) - vm->ticksAtLastRender;
		
		/* Ticks elapsed is the amount of time elapsed since last frame render (in microsec),
		 * which is lesser than the amount of time it would have taken on the real gameboy
		 * because the emulator goes very fast 
		 *
		 * In special cases where the emulator needs to be able to go slower than the 
		 * gba itself (for debugging), we add a special check */

#ifdef DEBUG_SUPPORT_SLOW_EMULATION
		if (ticksElapsed < (1e6/DEFAULT_FRAMERATE)) {
#endif
			usleep((1e6/DEFAULT_FRAMERATE) - ticksElapsed);
#ifdef DEBUG_SUPPORT_SLOW_EMULATION
		}
#endif
		vm->ticksAtLastRender = clock_u() - vm->ticksAtStartup;
    }
}
