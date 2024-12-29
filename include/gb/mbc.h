#ifndef gb_mbc_h
#define gb_mbc_h
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <gb/cartridge.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward Declare GB instead of including gb.h
 * to avoid a circular include */

struct GB;

typedef enum {
    BANK_MODE_ROM,
    BANK_MODE_RAM
} BANK_MODE;

typedef enum {
    MBC_NONE,
    MBC_TYPE_1,
    MBC_TYPE_2,
    MBC_TYPE_3,
    MBC_TYPE_5,
    MBC_TYPE_6,
    MBC_TYPE_7
} MBC_TYPE;

void mbc_allocate(struct GB* gb);
void mbc_free(struct GB* gb);
uint8_t mbc_readROM_N0(struct GB* gb, uint16_t addr);
uint8_t mbc_readROM_NN(struct GB* gb, uint16_t addr);
void mbc_writeExternalRAM(struct GB* gb, uint16_t addr, uint8_t byte);
uint8_t mbc_readExternalRAM(struct GB* gb, uint16_t addr);
void mbc_interceptROMWrite(struct GB* gb, uint16_t addr, uint8_t byte);
void switchROMBank(struct GB* gb, int bankNumber);
void switchRestrictedROMBank(struct GB* gb, int bankNumber);

#ifdef __cplusplus
}
#endif

#endif
