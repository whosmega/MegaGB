#include "../include/vm.h"
#include "../include/display.h"
#include "../include/debug.h"
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdbool.h>

#include <SDL2/SDL.h>


int initSDL(VM* vm) {
    SDL_Init(SDL_INIT_EVERYTHING);
    SDL_CreateWindowAndRenderer(WIDTH_PX, HEIGHT_PX, SDL_WINDOW_SHOWN,
                                &vm->sdl_window, &vm->sdl_renderer);

    if (!vm->sdl_window) return 1;          /* Failed to create screen */

    SDL_SetWindowTitle(vm->sdl_window, "MegaGBC");
    return 0;
}

void handleSDLEvents(VM *vm) {
    /* We listen for events like keystrokes and window closing */
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT: {
                vm->run = false;
                break;
            }
        }
    }
}

void freeSDL(VM* vm) {
    SDL_DestroyRenderer(vm->sdl_renderer);
    SDL_DestroyWindow(vm->sdl_window);
    SDL_Quit();
}

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
		 * because the emulator goes very fast */

		usleep((1e6/DEFAULT_FRAMERATE) - ticksElapsed);
		vm->ticksAtLastRender = clock_u() - vm->ticksAtStartup;
    }
}
