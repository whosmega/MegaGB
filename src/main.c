#include "../include/vm.h"
#include "../include/cartridge.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Error : Please give an input file\n");
        exit(1);
    }

    char* filePath = argv[1];
    FILE* file = fopen(filePath, "r");
    
    if (file == NULL) {
        printf("Error : Couldn't open input file\n");
        exit(2);
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Allocate enough space for the bytes */
    uint8_t* allocation = (uint8_t*)malloc(size);
    fread(allocation, size, 1, file);
	
    fclose(file);
    
    Cartridge c;
    bool result = initCartridge(&c, allocation, size);
    
    if (!result) exit(3);

    startEmulator(&c);
}
