#ifndef gba_debug_h
#define gba_debug_h
#include <gba/gba.h>

/* -------- Options -------- */
#define DEBUG_ENABLED
#define DEBUG_TRACE_STATE
	#define DEBUG_TRACE_REGS
		#define DEBUG_LIMIT_REGS

/* Contains useful dissembler for debugging and tracing the emulator */

#ifdef DEBUG_ENABLED
void printStateARM(GBA* gba, uint32_t opcode);
void initDissembler();
#endif
#endif
