#ifndef megagbc_display_h
#define megagbc_display_h
#include <pthread.h>
#include <gtk/gtk.h>
#include "../include/vm.h"

typedef struct {
    VM* vm;
} Display;

bool startDisplay(VM* vm);

#endif
