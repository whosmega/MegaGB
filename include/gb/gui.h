#ifndef gb_gui_h
#define gb_gui_h

/* C Wrapper for Dear ImGui C++ Library, uses SDL_Renderer bindings */

#include <SDL2/SDL.h>
#include <gb/gb.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_HEIGHT_PX 28

int initIMGUI(GB* gb);
void processEventsIMGUI(GB* gb, SDL_Event* event);
void renderFrameIMGUI(GB* gb);
void freeIMGUI(GB* gb);

#ifdef __cplusplus
}
#endif
#endif
