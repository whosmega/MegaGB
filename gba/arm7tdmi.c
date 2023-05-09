#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <gba/gba.h>
#include <gba/arm7tdmi.h>
#include <gba/debugGBA.h>

static void flushRefillPipeline(GBA*);

/* ---------------------- CPSR Functions ---------------------- */

static inline CPU_MODE CPSR_GetMode(GBA* gba) {
	return (CPU_MODE)(gba->CPSR & 0xF);
}

static inline void CPSR_SetMode(GBA* gba, CPU_MODE mode) {
	gba->CPSR &= ~0x1F;
	gba->CPSR |= 0x10 | mode;
}

static inline uint8_t CPSR_GetBit(GBA* gba, uint8_t bit) {
	return (gba->CPSR >> bit) & 1;
}

static inline void CPSR_SetBit(GBA* gba, uint8_t bit) {
	gba->CPSR |= 1 << bit;
}

static inline void CPSR_UnsetBit(GBA* gba, uint8_t bit) {
	gba->CPSR &= ~(1 << bit);
}

/* --------------- Register Functions ----------------- */

/* ----------------- Instruction Handlers ------------- */ 

static void BX(struct GBA* gba, uint32_t ins) {

}

static void B(struct GBA* gba, uint32_t ins) {
	int32_t offset = (int32_t)((ins & 0xFFFFFF) << 2); 			// shift by 2 is done to preserve
																// 4 byte alignment
	gba->REG[R15] += offset;
	flushRefillPipeline(gba);
}

static void BL(struct GBA* gba, uint32_t ins) {
	int32_t offset = (int32_t)((ins & 0xFFFFFF) << 2);
	gba->REG[R14] = gba->REG[R15] - 4; 							// Adjust LR value as PC is 8 bytes
																// ahead and we need to point to the
																// next instruction
	flushRefillPipeline(gba);
}

static uint32_t getDataProcessing_RxOP2(GBA* gba, uint16_t OP2, uint8_t* carry) {
	/* Decodes Operand 2 for register operand data processing functions 
	 * We calculate the shift */

	uint32_t data = gba->REG[OP2 & 0xF];
	*carry = CPSR_GetBit(gba, FLG_C); 				// Default - Unchanged

	uint8_t amount; 								// Amount to be shifted by

	if ((OP2 >> 4) & 1) {
		/* Shift amount stored in register */
		amount = gba->REG[OP2 >> 8] & 0xFF;
	} else {
		/* Shift amount in immediate value */
		amount = OP2 >> 7;
	}

	if (amount == 0) return data; 			// no shift

	switch ((OP2 >> 5) & 0b11) {
		/* Type of shift */
		case 0: {	// Logical Left
			if (amount > 32) {
				*carry = 0;
				data = 0;
			} else {
				*carry = (data >> (32 - amount)) & 1;
				data <<= amount;
			}
			break;
		}
		case 1: {	// Logical Right
			if (amount > 32) {
				*carry = 0;
				data = 0;
			} else {
				*carry = (data >> (amount - 1)) & 1;
				data >>= amount;
			}
			break;
		}
		case 2: { 	// Arithmetic Right
			if (amount == 0 || amount > 32) amount = 32; 			// 0 is treated as 32
			uint8_t bit31 = data >> 31;
			*carry = (data >> (amount - 1)) & 1;
			data >>= amount;

			/* Set shifted in bits at left to 1, if bit 31 is set, otherwise they're 0 */
			if (bit31) data |= (uint32_t)(0xFFFFFFFF << (32 - amount));
			break;
		}
		case 3: {	// Rotate Right 
			if (amount == 0) {
				// RRX
				*carry = data & 1;
				data >>= 1;
				data |= CPSR_GetBit(gba, FLG_C) << 31;
			} else {
				// Normal
				if (amount > 32) {
					amount %= 32;
					if (amount == 0) amount = 32;
				}
				*carry = (data >> (amount - 1)) & 1;
				uint32_t shiftedOut = data & (0xFFFFFFFF >> (32 - amount));
				data >>= amount;
				data |= shiftedOut << (32 - amount);
			}
			break;
		}
	}

	return data;
}

static uint32_t getDataProcessing_ImmOP2(GBA* gba, uint16_t OP2) {
	uint32_t Imm = (uint32_t)(OP2 & 0xFF);
	uint8_t rotate = ((OP2 >> 7) & 0xF) * 2;

	uint32_t shiftedOut = Imm & (0xFFFFFFFF >> (32 - rotate));
	Imm >>= rotate;
	Imm |= shiftedOut << (32 - rotate);
	return Imm;
}

static void AND_Rx(struct GBA* gba, uint32_t ins) {
	uint8_t carry;
	uint32_t OP1  = gba->REG[(ins >> 16) & 0xF];
	uint32_t OP2  = getDataProcessing_RxOP2(gba, ins & 0xFFF, &carry);
	uint8_t Dest = (ins >> 12) & 0xF;

	uint8_t S = (ins >> 20) & 0xF;

	// uint32_t result = 
	
}

static void AND_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void EOR_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void EOR_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void SUB_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void SUB_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void RSB_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void RSB_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void ADD_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void ADD_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void ADC_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void ADC_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void SBC_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void SBC_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void RSC_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void RSC_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void TST_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void TST_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void TEQ_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void TEQ_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void CMP_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void CMP_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void CMN_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void CMN_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void ORR_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void ORR_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void MOV_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void MOV_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void BIC_Rx(struct GBA* gba, uint32_t ins) { 
	
}

static void BIC_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void MVN_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void MVN_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void MRS(struct GBA* gba, uint32_t ins) {
	
}

static void MSR(struct GBA* gba, uint32_t ins) {
	
}

static void MUL(struct GBA* gba, uint32_t ins) {
	
}

static void MLA(struct GBA* gba, uint32_t ins) {
	
}

static void UMULL(struct GBA* gba, uint32_t ins) {
	
}

static void UMLAL(struct GBA* gba, uint32_t ins) {
	
}

static void SMULL(struct GBA* gba, uint32_t ins) {
	
}

static void SMLAL(struct GBA* gba, uint32_t ins) {
	
}

static void LDR_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void LDR_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void STR_Rx(struct GBA* gba, uint32_t ins) {
	
}

static void STR_Imm(struct GBA* gba, uint32_t ins) {
	
}

static void LDRH(struct GBA* gba, uint32_t ins) {
	
}

static void LDRSB(struct GBA* gba, uint32_t ins) {
	
}

static void LDRSH(struct GBA* gba, uint32_t ins) {
	
}

static void STRH(struct GBA* gba, uint32_t ins) {
	
}

static void LDM(struct GBA* gba, uint32_t ins) {
	
}

static void STM(struct GBA* gba, uint32_t ins) {
	
}

static void SWP(struct GBA* gba, uint32_t ins) {
	
}

static void SWP_B(struct GBA* gba, uint32_t ins) {
	
}

static void SWI(struct GBA* gba, uint32_t ins) {
	
}

static void Undefined(struct GBA* gba, uint32_t ins) {	
}

static void Unimplemented(struct GBA* gba, uint32_t ins) {
	printf("Instruction: %08x is unimplemented or an unsupported co-processor instruction\n", ins);
}

/* ---------------------------------------------------- */

static void switchMode(GBA* gba, CPU_MODE newMode) {
	/* Switches CPU to given mode, sets up banked registers 
	 * also updates CPSR */	

	CPU_MODE currentMode = gba->cpu_mode;

	if (currentMode == newMode) return;
	else if ((currentMode == CPU_MODE_SYSTEM && newMode == CPU_MODE_USER)
			 || (currentMode == CPU_MODE_USER && newMode == CPU_MODE_SYSTEM)) {

		/* No register bank switching required but we still switch modes */
		gba->cpu_mode = newMode;
		CPSR_SetMode(gba, newMode);
		return;
	}

	/* Save the registers of the current mode (only the ones which will be swapped out) */
	switch (currentMode) {
		case CPU_MODE_SYSTEM:
		case CPU_MODE_USER:
			memcpy(&gba->REG_SWAP[5], &gba->REG[R13], 2*sizeof(uint32_t));
			break;
		case CPU_MODE_FIQ:
			memcpy(&gba->BANK_FIQ, &gba->REG[R8], 7*sizeof(uint32_t));
			
			/* Load in R8-R12 normal registers as we're about to leave FIQ */
			memcpy(&gba->REG[R8], &gba->REG_SWAP, 5*sizeof(uint32_t));
			break;
		case CPU_MODE_IRQ:
			memcpy(&gba->BANK_IRQ, &gba->REG[R13], 2*sizeof(uint32_t));
			break;
		case CPU_MODE_SVC:
			memcpy(&gba->BANK_SVC, &gba->REG[R13], 2*sizeof(uint32_t));		
			break;
		case CPU_MODE_ABT:
			memcpy(&gba->BANK_ABT, &gba->REG[R13], 2*sizeof(uint32_t));
			break;
		case CPU_MODE_UND:
			memcpy(&gba->BANK_UND, &gba->REG[R13], 2*sizeof(uint32_t));
			break;
	}

	/* Save SPSR of the current mode if the mode isnt USER/SYSTEM */
	if (currentMode != CPU_MODE_USER && currentMode != CPU_MODE_SYSTEM) {
		gba->BANK_SPSR[currentMode] = gba->SPSR;
	}

	/* Load the registers of the new mode (only the ones for the mode) */
	switch (newMode) {
		case CPU_MODE_SYSTEM:
		case CPU_MODE_USER:
			/* Swap in R13-R14 only, if the previous mode was FIQ it has already reverted the rest */
			memcpy(&gba->REG[R13], &gba->REG_SWAP[5], 2*sizeof(uint32_t));
			break;
		case CPU_MODE_FIQ:
			/* Save R8-R12 in REG_SWAP as we're entering FIQ 
			 * R13-R14 should be saved by whichever mode was present before this */
			memcpy(&gba->REG_SWAP, &gba->REG[R8], 5*sizeof(uint32_t));
			/* Swap in R8-R14 */
			memcpy(&gba->REG[R8], &gba->BANK_FIQ, 7*sizeof(uint32_t));
			break;
		case CPU_MODE_IRQ:
			/* Swap in R13-R14 */
			memcpy(&gba->REG[R13], &gba->BANK_IRQ, 2*sizeof(uint32_t));
			break;
		case CPU_MODE_SVC:
			memcpy(&gba->REG[R13], &gba->BANK_SVC, 2*sizeof(uint32_t));		
			break;
		case CPU_MODE_ABT:
			memcpy(&gba->REG[R13], &gba->BANK_ABT, 2*sizeof(uint32_t));
			break;
		case CPU_MODE_UND:
			memcpy(&gba->REG[R13], &gba->BANK_UND, 2*sizeof(uint32_t));
			break;
	}

	/* Load in the SPSR if the new mode isnt USER/SYSTEM */
	if (newMode != CPU_MODE_USER && newMode != CPU_MODE_SYSTEM) {
		gba->SPSR = gba->BANK_SPSR[newMode];
	} else {
		/* SPSR cannot be read in USER/SYSTEM mode, so we initialise it to a garbage value */
		gba->SPSR = 0xFFFFFFFF;
	}

	CPSR_SetMode(gba, newMode);
	gba->cpu_mode = newMode;
}

static bool checkCondition(GBA* gba, uint8_t condCode) {
	/* Check if an ARM condition code is valid and if the instruction can be executed */
	switch (condCode) {
		case 0b1110: return true;
		case 0b0000: return CPSR_GetBit(gba, FLG_Z);
		case 0b0001: return !CPSR_GetBit(gba, FLG_Z);
		case 0b0010: return CPSR_GetBit(gba, FLG_C);
		case 0b0011: return !CPSR_GetBit(gba, FLG_C);
		case 0b0100: return CPSR_GetBit(gba, FLG_N);
		case 0b0101: return !CPSR_GetBit(gba, FLG_N);
		case 0b0110: return CPSR_GetBit(gba, FLG_V);
		case 0b0111: return !CPSR_GetBit(gba, FLG_V);
		case 0b1000: return CPSR_GetBit(gba, FLG_C) && !CPSR_GetBit(gba, FLG_Z);
		case 0b1001: return !CPSR_GetBit(gba, FLG_C) || CPSR_GetBit(gba, FLG_Z);
		case 0b1010: return CPSR_GetBit(gba, FLG_N) == CPSR_GetBit(gba, FLG_V);
		case 0b1011: return CPSR_GetBit(gba, FLG_N) != CPSR_GetBit(gba, FLG_V);
		case 0b1100: return !CPSR_GetBit(gba, FLG_Z) && (CPSR_GetBit(gba, FLG_N) 
							 									== CPSR_GetBit(gba, FLG_V));
		case 0b1101: return CPSR_GetBit(gba, FLG_Z) || (CPSR_GetBit(gba, FLG_N)
							 									!= CPSR_GetBit(gba, FLG_V));
		default: printf("[WARNING] Unknown Condition Code %x\n", condCode); return false;
	}
}

static inline uint32_t readARMOpcode(GBA* gba) {
	/* Read from the bus using the address at PC then increment it */
	uint32_t opcode = busRead32(gba, gba->REG[R15]);
	gba->REG[R15] += 4;

	return opcode;
}

static inline uint16_t readTHUMBOpcode(GBA* gba) {
	return 0;
}

static void dispatchARM(GBA* gba, uint32_t opcode) {
	/* Decodes and Dispatches an ARM gba opcode */
	if (!checkCondition(gba, opcode >> 28)) return;

	/* Execute from Lookup-Table (combine bits 27-20 and 7-4 to form a 12 bit index) */
	gba->ARM_LUT[((opcode & 0x0FF00000) >> 16) | ((opcode >> 4) & 0xF)]((struct GBA*)gba, opcode);
}

static void dispatchTHUMB(GBA* gba, uint16_t opcode) {

}

/* ----------------------------------------------------------------- */

static void initialiseLUT_ARM(GBA* gba) {
	/* Fills lookup table for ARM instructions with corresponding function pointers
	 * which handle the particular instruction */

	for (int index = 0; index < 4096; index++) {
		/* index corresponds to 12 bit index formed by combining bits 27-20 and 7-4 of OPCODE
		 *
		 * The order in which we check matters as there are encoding collisions, however 
		 * a simple way to understand it is to check which encoding has the most hardcoded 
		 * bits in 27-20 and 7-4, as these guarantee some instructions. So we check using 
		 * bit masks which are full of 1s (as we need to check more hardcoded bits) and slowly 
		 * reduce to more zeroes as bits become less significant 
		 *
		 * Basically we're extracting bits which are useful then comparing it to see if they match
		 * for the particular encoding */

		if ((index & 0b111111111111) == 0b000100100001) {
			/* Checking for Branch And Exchange */
			gba->ARM_LUT[index] = &BX;
		} else if ((index & 0b111110111111) == 0b000100001001) {
			/* Single Data Swap (SWP) 
			 *
			 * Byte/Word is the only option and we interpret them in the LUT */
			uint8_t B = (index >> 6) & 1;

			gba->ARM_LUT[index] = B ? &SWP_B : &SWP;
		} else if ((index & 0b111111001111) == 0b000000001001) {
			/* Checking for multiply and multiply accumulate
			 * bit 21 tells us whether its accumulate or not, while 20 is the S bit
			 * We check for S at runtime as usual but use different handlers for MUL and MLA */
			uint8_t A = (index >> 5) & 1;
			gba->ARM_LUT[index] = A ? &MLA : &MUL;
		} else if ((index & 0b111110001111) == 0b000010001001) {
			/* Checking for multiply long and multiply accumulate long with variations
			 * (UMULL, UMLAL, SMULL, SMLAL), S bit is checked at runtime */
			uint8_t U = (index >> 6) & 1;
			uint8_t A = (index >> 5) & 1;

			if (U) gba->ARM_LUT[index] = A ? &SMLAL : &SMULL;
			else gba->ARM_LUT[index] = A ? &UMLAL : &UMULL;
		} else if ((index & 0b111110110000) == 0b000100000000) {
			/* Checking for MRS (transfer PSR->Reg)
			 * bit 22 can be 1 or 0 depending on whether SPSR/CPSR has to be used,
			 * which will be determined at runtime */
			gba->ARM_LUT[index] = &MRS;
		} else if ((index & 0b110110110000) == 0b000100100000) {
			/* Checking for MSR (transfer Reg/Imm->PSR)
			 * This instruction has 2 encodings, one for flag only transfer which can be Imm/Reg
			 * or full register transfer which is only Reg. This however introduces complicated 
			 * bit collisions which we can avoid by doing runtime checks instead */
			gba->ARM_LUT[index] = &MSR;
		} else if ((index & 0b111000001001) == 0b000000001001) {
			/* Checking for Halfword and Signed Data transfer
			 * (LDRH, STRH, LDRSB, LDRSH) -> Signed Byte and Signed halfword is
			 * only available for LDR.
			 * We check for each of these options and leave the rest for runtime 
			 * Note: We dont differtiate between Rx and Imm for this one as interpretation
			 * is pretty similar for both of them (no shifting) and we already have many variations */

			uint8_t L = (index >> 4) & 1; 				// LOAD/STORE
			uint8_t SH = (index >> 1) & 0b11;			// Sign and Halfword

			if (L) {
				switch (SH) {
					// 00 never occurs as it is interpreted as a SWP instruction checked earlier
					case 0b01: gba->ARM_LUT[index] = &LDRH; break;
					case 0b10: gba->ARM_LUT[index] = &LDRSB; break;
					case 0b11: gba->ARM_LUT[index] = &LDRSH; break;
				}
			} else {
				// STORE only allows unsigned halfwords
				gba->ARM_LUT[index] = &STRH;
			}
		} else if ((index & 0b111100000000) == 0b111100000000) {
			/* Software Interrupt - SWI */
			gba->ARM_LUT[index] = &SWI;
		} else if ((index & 0b111000000001) == 0b011000000001) {
			/* Undefined Instruction */
			gba->ARM_LUT[index] = &Undefined;
		} else if ((index & 0b111000000000) == 0b101000000000) {
			/* Checking for Branch and Branch with Link */
			uint8_t Link = (index >> 8) & 1;
			gba->ARM_LUT[index] = Link ? &BL : &B;
		} else if ((index & 0b111000000000) == 0b100000000000) {
			/* Block Data Transfer - LDM/STM 
			 *
			 * All options are interpreted at runtime except LOAD/STORE */
			uint8_t L = (index >> 4) & 1;

			gba->ARM_LUT[index] = L ? &LDM : &STM;
		} else if ((index & 0b110000000000) == 0b010000000000) {
			/* Checking for Single Data Transfer (LDR/STR)
			 * with options for immediate or register with shift
			 * Other options are checked at runtime */
			uint8_t L = (index >> 4) & 1; 				// LOAD/STORE
			uint8_t I = (index >> 9) & 1; 				// IMM/Reg

			if (L) {
				gba->ARM_LUT[index] = I ? &LDR_Rx : &LDR_Imm;
			} else {
				gba->ARM_LUT[index] = I ? &STR_Rx : &STR_Imm;
			}
		} else if ((index & 0b110000000000) == 0) {
			/* Checking for Data Processing Instructions
		 	 * Only bit 27-26 are fixed, rest are variable showing various opcodes and other info 
			 * We setup separate functions for Immediate value and Register for each data proc 
			 * instruction
		 	 * We only keep granularity till that level and do checking for S bit internally as 
			 * otherwise that would double the number of functions */

			uint8_t I = (index >> 9) & 1; 			/* Immediate or not */

#define WRAP_DATAPROC(opcode, Rx, Imm) case opcode: gba->ARM_LUT[index] = I ? &Imm : &Rx; break;	

			switch ((index >> 5) & 0xF) {
				/* Check OP CODE */
				WRAP_DATAPROC(0x0, AND_Rx, AND_Imm)
				WRAP_DATAPROC(0x1, EOR_Rx, EOR_Imm)
				WRAP_DATAPROC(0x2, SUB_Rx, SUB_Imm)
				WRAP_DATAPROC(0x3, RSB_Rx, RSB_Imm)
				WRAP_DATAPROC(0x4, ADD_Rx, ADD_Imm)
				WRAP_DATAPROC(0x5, ADC_Rx, ADC_Imm)
				WRAP_DATAPROC(0x6, SBC_Rx, SBC_Imm)
				WRAP_DATAPROC(0x7, RSC_Rx, RSC_Imm)
				WRAP_DATAPROC(0x8, TST_Rx, TST_Imm)
				WRAP_DATAPROC(0x9, TEQ_Rx, TEQ_Imm)
				WRAP_DATAPROC(0xA, CMP_Rx, CMP_Imm)
				WRAP_DATAPROC(0xB, CMN_Rx, CMN_Imm)
				WRAP_DATAPROC(0xC, ORR_Rx, ORR_Imm)
				WRAP_DATAPROC(0xD, MOV_Rx, MOV_Imm)
				WRAP_DATAPROC(0xE, BIC_Rx, BIC_Imm)
				WRAP_DATAPROC(0xF, MVN_Rx, MVN_Imm)
			}
#undef WRAP_DATAPROC
		} else {
			gba->ARM_LUT[index] = &Unimplemented;
		}
	}
}

void initialiseCPU(GBA* gba) {
	gba->cpu_state = CPU_STATE_ARM;
	gba->cpu_mode = CPU_MODE_SYSTEM;

	/* Preset register values as set by BIOS (we dont use a BIOS file, just emulate 
	 * its behaviour, including BIOS functions) */
	memset(&gba->BANK_FIQ, 	0, 7*sizeof(uint32_t));
	memset(&gba->BANK_SVC, 	0, 2*sizeof(uint32_t));
	memset(&gba->BANK_ABT, 	0, 2*sizeof(uint32_t));
	memset(&gba->BANK_IRQ, 	0, 2*sizeof(uint32_t));
	memset(&gba->BANK_UND, 	0, 2*sizeof(uint32_t));
	memset(&gba->BANK_SPSR, 0, 5*sizeof(uint32_t));
	memset(&gba->REG, 	  	0,16*sizeof(uint32_t));
	memset(&gba->REG_SWAP,  0, 7*sizeof(uint32_t));

	gba->BANK_SVC[R13_SVC] = 0x03007FE0;
	gba->BANK_IRQ[R13_IRQ] = 0x03007FA0;
	gba->REG[R13] 		   = 0x03007F00;
	gba->REG[R15] 		   = 0x08000000;
	gba->CPSR = 0x0000001F; 					// ARM State, System Mode
	gba->SPSR = 0xFFFFFFFF; 					// SPSR cannot be read in USER/SYSTEM mode */

	/* Setup Lookup Tables */
	initialiseLUT_ARM(gba);

	/* Other values */
	memset(&gba->pipeline, 0, 3*sizeof(uint32_t));
	gba->pipelineInsertPoint = 0;
	gba->pipelineReadPoint = 0;
	gba->skipFetch = false;
	gba->cycles = 0;

	flushRefillPipeline(gba); 		gba->skipFetch = false;
}

static inline void insertPipeline(GBA* gba, uint32_t opcode) {
	gba->pipeline[gba->pipelineInsertPoint++] = opcode;
	gba->pipelineInsertPoint %= 3;
	
}

static inline uint32_t readPipeline(GBA* gba) {
	uint32_t code = gba->pipeline[gba->pipelineReadPoint++];
	gba->pipelineReadPoint %= 3;
	return code;
}

static void flushRefillPipeline(GBA* gba) {
	/* Flushes then refills pipeline relative to PC,
	 * we always ensure that the pipeline is always in execuatable state */

	gba->pipelineInsertPoint = 0;
	gba->pipelineReadPoint = 0;

	if (gba->cpu_state == CPU_STATE_ARM) {
		insertPipeline(gba, readARMOpcode(gba));
		insertPipeline(gba, readARMOpcode(gba));
	} else {
		insertPipeline(gba, readTHUMBOpcode(gba));
		insertPipeline(gba, readTHUMBOpcode(gba));
	}
	gba->skipFetch = true; 							/* Set flag in emulator so pipeline operations 
														   after the current execution stage are 
														   discarded */
}

void stepCPU(GBA* gba) {
	/* CPU Pipeline has 3 stages happening simulataneously, Execute/Decode/Fetch
	 *
	 * C1 -> Execute  - 	Decode  - 		Fetch ins1
	 * C2 -> Execute  - 	Decode ins1 	Fetch ins2
	 * C3 -> Execute ins1   Decode ins2   	Fetch ins3
	 * C4 -> Execute ins2 	Decode ins3 	Fetch ins4
	 * ...
	 *
	 * But we can merge the execute and decode stages, so essentially we do this
	 *
	 * C1 -> Execute/Decode   - 	Fetch 	ins1
	 * C2 -> Execute/Decode   - 	Fetch   ins2
	 * C3 -> Execute/Decode  ins1	Fetch	ins3
	 * C4 -> Execute/Decode  ins2 	Fetch 	ins4
	 *
	 * Any instruction that modifies R15 causes a pipeline flush, causing it clear up
	 * and start fetching again. However, we can optimize by not emulating the first 2 cpu steps where
	 * no execution takes place and keeping the pipeline always in executable state. This is done by 
	 * flushRefillPipeline() which is called during initialization and at every R15 modification 
	 *
	 * The pipeline is managed by a queue, which is responsible for storing prefetched instructions
	 * The instructions flow in the queue once they are loaded in, independent of what happens at
	 * the actual address. This means if the address of the next instruction is modified, 
	 * the instruction will still execute as it was prefetched in the queue */

	if (gba->cpu_state == CPU_STATE_ARM) {
#if defined(DEBUG_ENABLED) && defined(DEBUG_TRACE_STATE)
		uint32_t opcode = readPipeline(gba);
		printStateARM(gba, opcode);
		dispatchARM(gba, opcode);
#else
		dispatchARM(gba, readPipeline(gba));
#endif
		if (gba->skipFetch) {gba->skipFetch = false; return;}
	
		insertPipeline(gba, readARMOpcode(gba));
	} else {
		dispatchTHUMB(gba, readPipeline(gba));
		if (gba->skipFetch) {gba->skipFetch = false; return;}

		insertPipeline(gba, readTHUMBOpcode(gba));
	}
}
