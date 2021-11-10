#ifndef MGBC_CARTRIDGE_H
#define MGBC_CARTRIDGE_H
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum {
    LC_NONE = 0x0,
    LC_NINTENDO_R_AND_D1 = 0x1,
    LC_CAPCOM = 0x8,
    LC_ELECTRONIC_ARTS = 0x13,
    LC_HUDSON_SOFT = 0x18,
    LC_B_AI = 0x19,
    LC_KSS = 0x20,
    LC_POW = 0x22,
    LC_PCM_COMPLETE = 0x24,
    LC_SAN_X = 0x25,
    LC_KEMCO_JAPAN = 0x28,
    LC_SETA = 0x29,
    LC_VIACOM = 0x30,
    LC_NINTENDO = 0x31,
    LC_BANDAI = 0x32,
    LC_OCEAN_ACCLAIM = 0x33,
    LC_KONAMI = 0x34
    
    /* More to be added */
} LICENCEE_CODE;

typedef enum {
    OLC_USE_NEW = 0x33,                 /* The new lincencee format should be used instead */

    /* More to be added */
} OLD_LICENCEE_CODE;

typedef enum {
    CGB_LEGACY_MODE = 0x80,
    CGB_ONLY_MODE = 0xC0,
    PGB_MODE
} CGB_CODE;

typedef enum {
    CARTRIDGE_NONE = 0x00,
    CARTRIDGE_MBC1 = 0x01,
    CARTRIDGE_MBC1_RAM = 0x02,
    CARTRIDGE_MBC1_RAM_BATTERY = 0x03,
    CARTRIDGE_MBC2 = 0X05,
    CARTRIDGE_MBC2_BATTERY = 0x06,
    CARTRIDGE_ROM_RAM = 0x08,
    CARTRIDGE_ROM_RAM_BATTERY = 0x09,
    CARTRIDGE_MMM01 = 0x0B,
    CARTRIDGE_MMM01_RAM = 0x0C,
    CARTRIDGE_MMM01_RAM_BATTERY = 0x0D,
    CARTRIDGE_MBC3_TIMER_BATTERY = 0x0F,
    CARTRIDGE_MBC3_TIMER_RAM_BATTERY = 0x10,
    CARTRIDGE_MBC3 = 0x11,
    CARTRIDGE_MBC3_RAM = 0x12,
    CARTRIDGE_MBC3_RAM_BATTERY = 0x13,
    CARTRIDGE_MBC5 = 0x19,
    CARTRIDGE_MBC5_RAM = 0x1A,
    CARTRIDGE_MBC5_RAM_BATTERY = 0x1B,
    CARTRIDGE_MBC5_RUMBLE = 0x1C,
    CARTRIDGE_MBC5_RUMBLE_RAM = 0x1D,
    CARTRIDGE_MBC5_RUMBLE_RAM_BATTERY = 0x1E,
    CARTRIDGE_MBC6 = 0x20,
    CARTRIDGE_MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,
    CARTRIDGE_POCKET_CAMERA = 0xFC,
    CARTRIDGE_BANDAI_TAMA = 0xFD,
    CARTRIDGE_HUC3 = 0xFE,
    CARTRIDGE_HUC1_RAM_BATTERY = 0xFF
} CARTRIDGE_TYPE;

typedef enum {
    ROM_32KB,
    ROM_64KB,
    ROM_128KB,
    ROM_256KB,
    ROM_512KB,
    ROM_1MB,
    ROM_2MB,
    ROM_4MB,
    ROM_8MB
} ROM_SIZE;

typedef enum {
    /* External Ram */
    EXT_RAM_0,
    EXT_RAM_2KB,
    EXT_RAM_8KB,
    EXT_RAM_32KB,
    EXT_RAM_128KB,
    EXT_RAM_64KB
} RAM_SIZE;

typedef enum {
    DEST_CODE_JP,
    DEST_CODE_NONJP
} DEST_CODE;

typedef struct {
    uint8_t* allocated;
    uint8_t logoChecksum[0x30];              /* 0x30 bytes long logo checksum in the cartridge */
    char title[11];                          /* 11 character long title */
    char mfcCode[4];                         /* 4 character long manufacturer code */
    CGB_CODE cgbCode;                        /* Is CGB supported? */
    LICENCEE_CODE lCode;                     /* Licencee code enum, or the company of the game */
    CARTRIDGE_TYPE cType;                    /* Enum of the defined list of external hardware that 
                                                comes with the cartridge or Memory Bank Controller 
                                                aka. ram extension */
    ROM_SIZE romSize;                        /* Enum specifying the rom size */
    RAM_SIZE extRamSize;                     /* External ram size enum */
    DEST_CODE dCode;                         /* Destination code, is game sold in japan or outside */
    OLD_LICENCEE_CODE oldLCode;              /* Old way of storing game company, also valid */
    uint8_t romVersion;                      /* ROM version number */
    bool supportsSGB;                        /* Are SGB functions supported */
    uint8_t headerChecksum;                  /* A value computed beforehand, used by cpu to verify */
    uint8_t globalChecksum;                  /* Another kind of checksum, but doesnt get verified */

    bool inserted;
} Cartridge;


/* Init Cartridge sets all the data 
 * fields in the cartridge and returns whether it was a 
 * success or not */

bool initCartridge(Cartridge* c, uint8_t* data, size_t size);
void printCartridge(Cartridge* c);
void freeCartridge(Cartridge* c);
#endif
