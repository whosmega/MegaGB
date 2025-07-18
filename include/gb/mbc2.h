#ifndef gb_mbc2_h
#define gb_mbc2_h
#include <gb/gb.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t builtInRAM[0x200];              /* Built in 512x4 bit RAM */

    bool ramEnabled;
} MBC_2;

void mbc2_allocate(GB* gb);
uint8_t mbc2_readROM(GB* gb, uint16_t addr);
void mbc2_writeBuiltInRAM(GB* gb, uint16_t addr, uint8_t byte);
uint8_t mbc2_readBuiltInRAM(GB* gb, uint16_t addr);
void mbc2_free(GB* gb);
void mbc2_interceptROMWrite(GB* gb, uint16_t addr, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif
