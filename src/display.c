#include "../include/vm.h"
#include "../include/display.h"
#include <stdbool.h>
#include <gtk/gtk.h>

void initDisplay(Display* display, VM* vm) {
    display->vm = vm;
}

void* threadHandler(void* arg) {
    Display* display = (Display*)arg;
            
    return NULL;
}

bool startDisplay(VM* vm) {
    Display display; 
    initDisplay(&display, vm);
    
    /* Create new thread for our display */
    pthread_t threadId;
    pthread_create(&threadId, NULL, threadHandler, (void*)&display);
    
    return true;    
}

