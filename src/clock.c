#include "../include/clock.h"
#include "../include/vm.h"

void* startClock(void* arg) {
    /* The gameboy's internal clock ticks 4194304 times every second 
     * (in CGB normal speed mode */
    VM* vm = (VM*)arg;
    vm->clockThreadStatus = THREAD_RUNNING;

    while (vm->runClock) {

    }

    vm->clockThreadStatus = THREAD_DEAD;
    return NULL;
}
