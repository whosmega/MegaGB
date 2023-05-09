#include <gba/debugGBA.h>
#include <gba/gba.h>
#include <stdio.h>

#ifdef DEBUG_ENABLED

void (*Dissembler_ARM_LUT[4096])(GBA* gba, uint32_t opcode);

static void BX(struct GBA* gba, uint32_t ins) {
	printf("BX");
}

static void B(struct GBA* gba, uint32_t ins) {
	printf("B    %-7d", (int32_t)((ins & 0xFFFFFF) << 2));
}

static void BL(struct GBA* gba, uint32_t ins) {
	printf("BL   %-7d", (int32_t)(ins & 0xFFFFFF) << 2);
}

static void AND_Rx(struct GBA* gba, uint32_t ins) {
	printf("AND");
}

static void AND_Imm(struct GBA* gba, uint32_t ins) {
	printf("AND Imm");
}

static void EOR_Rx(struct GBA* gba, uint32_t ins) {
	printf("EOR Rx");
}

static void EOR_Imm(struct GBA* gba, uint32_t ins) {
	printf("EOR Imm");
}

static void SUB_Rx(struct GBA* gba, uint32_t ins) {
	printf("SUB Rx");
}

static void SUB_Imm(struct GBA* gba, uint32_t ins) {
	printf("SUB Imm");
}

static void RSB_Rx(struct GBA* gba, uint32_t ins) {
	printf("RSB Rx");
}

static void RSB_Imm(struct GBA* gba, uint32_t ins) {
	printf("RSB Imm");
}

static void ADD_Rx(struct GBA* gba, uint32_t ins) {
	printf("ADD Rx");
}

static void ADD_Imm(struct GBA* gba, uint32_t ins) {
	printf("ADD Imm");
}

static void ADC_Rx(struct GBA* gba, uint32_t ins) {
	printf("ADC Rx");
}

static void ADC_Imm(struct GBA* gba, uint32_t ins) {
	printf("ADC Imm");
}

static void SBC_Rx(struct GBA* gba, uint32_t ins) {
	printf("SBC Rx");
}

static void SBC_Imm(struct GBA* gba, uint32_t ins) {
	printf("SBC Imm");
}

static void RSC_Rx(struct GBA* gba, uint32_t ins) {
	printf("RSC Rx");
}

static void RSC_Imm(struct GBA* gba, uint32_t ins) {
	printf("RSC Imm");
}

static void TST_Rx(struct GBA* gba, uint32_t ins) {
	printf("TST Rx");
}

static void TST_Imm(struct GBA* gba, uint32_t ins) {
	printf("TST Imm");
}

static void TEQ_Rx(struct GBA* gba, uint32_t ins) {
	printf("TEQ Rx");
}

static void TEQ_Imm(struct GBA* gba, uint32_t ins) {
	printf("TEQ Imm");
}

static void CMP_Rx(struct GBA* gba, uint32_t ins) {
	printf("CMP Rx");
}

static void CMP_Imm(struct GBA* gba, uint32_t ins) {
	printf("CMP Imm");
}

static void CMN_Rx(struct GBA* gba, uint32_t ins) {
	printf("CMN Rx");
}

static void CMN_Imm(struct GBA* gba, uint32_t ins) {
	printf("CMN IMm");
}

static void ORR_Rx(struct GBA* gba, uint32_t ins) {
	printf("ORR Rx");
}

static void ORR_Imm(struct GBA* gba, uint32_t ins) {
	printf("ORR Imm");
}

static void MOV_Rx(struct GBA* gba, uint32_t ins) {
	printf("MOV Rx");
}

static void MOV_Imm(struct GBA* gba, uint32_t ins) {
	printf("MOV Imm");
}

static void BIC_Rx(struct GBA* gba, uint32_t ins) { 
	printf("BIC Rx");
}

static void BIC_Imm(struct GBA* gba, uint32_t ins) {
	printf("BIC Imm");
}

static void MVN_Rx(struct GBA* gba, uint32_t ins) {
	printf("MVN Rx");
}

static void MVN_Imm(struct GBA* gba, uint32_t ins) {
	printf("MVN Imm");
}

static void MRS(struct GBA* gba, uint32_t ins) {
	printf("MRS");
}

static void MSR(struct GBA* gba, uint32_t ins) {
	printf("MSR");
}

static void MUL(struct GBA* gba, uint32_t ins) {
	printf("MUL");
}

static void MLA(struct GBA* gba, uint32_t ins) {
	printf("MLA");
}

static void UMULL(struct GBA* gba, uint32_t ins) {
	printf("UMULL");
}

static void UMLAL(struct GBA* gba, uint32_t ins) {
	printf("UMLAL");
}

static void SMULL(struct GBA* gba, uint32_t ins) {
	printf("SMULL");
}

static void SMLAL(struct GBA* gba, uint32_t ins) {
	printf("SMLAL");
}

static void LDR_Rx(struct GBA* gba, uint32_t ins) {
	printf("LDR Rx");
}

static void LDR_Imm(struct GBA* gba, uint32_t ins) {
	printf("LDR Imm");
}

static void STR_Rx(struct GBA* gba, uint32_t ins) {
	printf("STR Rx");
}

static void STR_Imm(struct GBA* gba, uint32_t ins) {
	printf("STR Imm");
}

static void LDRH(struct GBA* gba, uint32_t ins) {
	printf("LDRH");
}

static void LDRSB(struct GBA* gba, uint32_t ins) {
	printf("LDRSB");
}

static void LDRSH(struct GBA* gba, uint32_t ins) {
	printf("LDRSH");
}

static void STRH(struct GBA* gba, uint32_t ins) {
	printf("STRH");
}

static void LDM(struct GBA* gba, uint32_t ins) {
	printf("LDM");
}

static void STM(struct GBA* gba, uint32_t ins) {
	printf("STM");
}

static void SWP(struct GBA* gba, uint32_t ins) {
	printf("SWP");
}

static void SWP_B(struct GBA* gba, uint32_t ins) {
	printf("SWPB");
}

static void SWI(struct GBA* gba, uint32_t ins) {
	printf("SWI");
}

static void Undefined(struct GBA* gba, uint32_t ins) {
	printf("Undefined");
}

static void Unimplemented(struct GBA* gba, uint32_t ins) {
	printf("Unimplemented");
}

void initDissembler() {
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
			Dissembler_ARM_LUT[index] = &BX;
		} else if ((index & 0b111110111111) == 0b000100001001) {
			/* Single Data Swap (SWP) 
			 *
			 * Byte/Word is the only option and we interpret them in the LUT */
			uint8_t B = (index >> 6) & 1;

			Dissembler_ARM_LUT[index] = B ? &SWP_B : &SWP;
		} else if ((index & 0b111111001111) == 0b000000001001) {
			/* Checking for multiply and multiply accumulate
			 * bit 21 tells us whether its accumulate or not, while 20 is the S bit
			 * We check for S at runtime as usual but use different handlers for MUL and MLA */
			uint8_t A = (index >> 5) & 1;
			Dissembler_ARM_LUT[index] = A ? &MLA : &MUL;
		} else if ((index & 0b111110001111) == 0b000010001001) {
			/* Checking for multiply long and multiply accumulate long with variations
			 * (UMULL, UMLAL, SMULL, SMLAL), S bit is checked at runtime */
			uint8_t U = (index >> 6) & 1;
			uint8_t A = (index >> 5) & 1;

			if (U) Dissembler_ARM_LUT[index] = A ? &SMLAL : &SMULL;
			else Dissembler_ARM_LUT[index] = A ? &UMLAL : &UMULL;
		} else if ((index & 0b111110110000) == 0b000100000000) {
			/* Checking for MRS (transfer PSR->Reg)
			 * bit 22 can be 1 or 0 depending on whether SPSR/CPSR has to be used,
			 * which will be determined at runtime */
			Dissembler_ARM_LUT[index] = &MRS;
		} else if ((index & 0b110110110000) == 0b000100100000) {
			/* Checking for MSR (transfer Reg/Imm->PSR)
			 * This instruction has 2 encodings, one for flag only transfer which can be Imm/Reg
			 * or full register transfer which is only Reg. This however introduces complicated 
			 * bit collisions which we can avoid by doing runtime checks instead */
			Dissembler_ARM_LUT[index] = &MSR;
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
					case 0b01: Dissembler_ARM_LUT[index] = &LDRH; break;
					case 0b10: Dissembler_ARM_LUT[index] = &LDRSB; break;
					case 0b11: Dissembler_ARM_LUT[index] = &LDRSH; break;
				}
			} else {
				// STORE only allows unsigned halfwords
				Dissembler_ARM_LUT[index] = &STRH;
			}
		} else if ((index & 0b111100000000) == 0b111100000000) {
			/* Software Interrupt - SWI */
			Dissembler_ARM_LUT[index] = &SWI;
		} else if ((index & 0b111000000001) == 0b011000000001) {
			/* Undefined Instruction */
			Dissembler_ARM_LUT[index] = &Undefined;
		} else if ((index & 0b111000000000) == 0b101000000000) {
			/* Checking for Branch and Branch with Link */
			uint8_t Link = (index >> 8) & 1;
			Dissembler_ARM_LUT[index] = Link ? &BL : &B;
		} else if ((index & 0b111000000000) == 0b100000000000) {
			/* Block Data Transfer - LDM/STM 
			 *
			 * All options are interpreted at runtime except LOAD/STORE */
			uint8_t L = (index >> 4) & 1;

			Dissembler_ARM_LUT[index] = L ? &LDM : &STM;
		} else if ((index & 0b110000000000) == 0b010000000000) {
			/* Checking for Single Data Transfer (LDR/STR)
			 * with options for immediate or register with shift
			 * Other options are checked at runtime */
			uint8_t L = (index >> 4) & 1; 				// LOAD/STORE
			uint8_t I = (index >> 9) & 1; 				// IMM/Reg

			if (L) {
				Dissembler_ARM_LUT[index] = I ? &LDR_Rx : &LDR_Imm;
			} else {
				Dissembler_ARM_LUT[index] = I ? &STR_Rx : &STR_Imm;
			}
		} else if ((index & 0b110000000000) == 0) {
			/* Checking for Data Processing Instructions
		 	 * Only bit 27-26 are fixed, rest are variable showing various opcodes and other info 
			 * We setup separate functions for Immediate value and Register for each data proc 
			 * instruction
		 	 * We only keep granularity till that level and do checking for S bit internally as 
			 * otherwise that would double the number of functions */

			uint8_t I = (index >> 9) & 1; 			/* Immediate or not */

#define WRAP_DATAPROC(opcode, Rx, Imm) case opcode: Dissembler_ARM_LUT[index] = I ? &Imm : &Rx; break;	

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
			Dissembler_ARM_LUT[index] = &Unimplemented;
		}
	}
}

void printStateARM(GBA* gba, uint32_t opcode) {
	printf("[A][%08x][N%dS%dC%dV%d] ", gba->REG[R15] - 8, 
				gba->CPSR>>31, (gba->CPSR>>30)&1, (gba->CPSR>>29)&1, (gba->CPSR>>28)&1);
	Dissembler_ARM_LUT[((opcode & 0x0FF00000) >> 16) | ((opcode >> 4) & 0xF)](gba, opcode);
	printf("\n");
}

#endif
