#ifndef megagbc_display_h
#define megagbc_display_h
#include <pthread.h>
#include <gtk/gtk.h>
#include "../include/vm.h"

#define MIN_RES_HEIGHT_PX 144 
#define MIN_RES_WIDTH_PX 160

/* The display GUI is started by invoking this function in a new thread */

void* startDisplay(void* arg);

#endif
