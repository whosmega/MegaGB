#include <gb/cartridge.h>
#include <gba/gamepak.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void startGBEmulator(Cartridge*);
extern void startGBAEmulator(GamePak*);

static void runGB(uint8_t* allocation, size_t size) {
	Cartridge c;
    bool result = initCartridge(&c, allocation, size);

    if (!result) exit(3);

    startGBEmulator(&c);

	freeCartridge(&c);
}

static void runGBA(uint8_t* allocation, size_t size) {
	GamePak gamepak;
	bool result = initGamePak(&gamepak, allocation, size);

	if (!result) exit(3);

	startGBAEmulator(&gamepak);

	freeGamePak(&gamepak);
}

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
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    /* Allocate enough space for the bytes */
    uint8_t* allocation = (uint8_t*)malloc(size);
    int ret = fread(allocation, size, 1, file);
	if (ret != 1) {
		printf("Error : Could not read full file\n");
		exit(3);
	}
    fclose(file);

	/* Figure out emulation type (gb/gbc or gba) */
	if (argc >= 3) {
		char* arg = argv[2];

		if ((strlen(arg) == 3) && memcmp(&arg, "-gb", 3)) {

			/* GB/GBC */
			runGB(allocation, size);

		} else if ((strlen(arg) == 4) && memcmp(&arg, "-gba", 4)) {
			/* GBA */
			runGBA(allocation, size);
		} else {
			printf("Error : Invalid Argument - %s\n", arg);
			exit(3);
		}
	} else {
		/* GB/GBC */
		runGB(allocation, size);
	}

	return 0;
}
