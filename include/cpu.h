#ifndef megagbc_cpu_h
#define megagbc_cpu_h
#include "../include/vm.h"

/* Resets the registers in the GBC */
void resetGBC(VM* vm);

/* This function is invoked to run the CPU thread */
void dispatch(VM* vm);

#endif
