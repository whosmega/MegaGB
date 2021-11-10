#ifndef megagbc_mbc_h
#define megagbc_mbc_h
#include <stdbool.h>
#include <stdint.h>
#include "../include/cartridge.h"

/* Forward Declare VM instead of including vm.h 
 * to avoid a circular include */

struct VM;

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

/* Memory Bank Controller structure */

typedef struct {
    /* External RAM Bank Storage 
     *
     * MBC1 supports 4 8kib banks for RAM */
    uint8_t* ramBanks;
    
    /* Note : Selected RAM/ROM banks are kept in their 
     * associated memory locations in the VM Memory.
     * The cartridge structure located in VM contains 
     * the entire rom as a continuous array and the MBC maps
     * the correct bank from it */

    /* Registers */
    bool ramEnabled;
    uint8_t romBankNumber;       /* Used to store lower 5 bits of bank number */
    uint8_t secondaryBankNumber; /* Used to store upper 2 bits of rom bank number or 
                                    ram bank number */
    BANK_MODE bankMode;          /* Banking Mode */
} MBC_1;


void mbc_allocate(struct VM* vm);
void mbc_free(struct VM* vm);
void mbc_interceptRead(struct VM* vm);
void mbc_interceptWrite(struct VM* vm);

#endif
