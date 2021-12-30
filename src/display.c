#include "../include/vm.h"
#include "../include/display.h"
#include "../include/debug.h"
#include "../include/vm.h"
#include <SDL2/SDL_events.h>
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


void syncDisplay(VM* vm, unsigned int cycleUpdate) {
    /* We sync the display by checking if we have to draw a frame 
     *
     * This is probably called every cycle update*/

    vm->cyclesSinceLastFrame += cycleUpdate;

    if (vm->cyclesSinceLastFrame >= CYCLES_PER_FRAME) {
        /* We preserve any extra cycles */
        vm->cyclesSinceLastFrame -= CYCLES_PER_FRAME;

        /* Draw frame */
    }
}
