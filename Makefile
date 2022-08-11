# megagbc

CC = gcc
CFLAGS = -O2 `sdl2-config --cflags`
LFLAGS = -O2 `sdl2-config --libs`
EXE = megagbc

BIN = cartridge.o gb.o main.o debug.o display.o cpu.o mbc.o mbc1.o mbc2.o

# test suite

ASMFLAGS = -i debug/test_suite/

$(EXE): $(BIN)
	$(CC) $(BIN) $(LFLAGS) -o $(EXE)
	mkdir -p bin
	mv *.o bin

cartridge.o : include/cartridge.h \
			  src/cartridge.c
	$(CC) -c src/cartridge.c $(CFLAGS)

gb.o : include/gb.h \
	   src/gb.c
	$(CC) -c src/gb.c $(CFLAGS)

main.o : include/gb.h include/cartridge.h \
		 src/main.c
	$(CC) -c src/main.c $(CFLAGS)

cpu.o : include/cpu.h \
		src/cpu.c
	$(CC) -c src/cpu.c $(CFLAGS)

mbc.o : include/mbc.h \
		src/mbc.c
	$(CC) -c src/mbc.c $(CFLAGS)

mbc1.o : include/mbc1.h \
		src/mbc1.c
	$(CC) -c src/mbc1.c $(CFLAGS)

mbc2.o : include/mbc2.h \
		src/mbc2.c
	$(CC) -c src/mbc2.c $(CFLAGS)

display.o : include/display.h \
			src/display.c
	$(CC) -c src/display.c $(CFLAGS)

debug.o : include/gb.h include/debug.h \
		 src/debug.c
	$(CC) -c src/debug.c $(CFLAGS)
# --------------------------------------------------------------------
tests: edge_sprite.o
	rgblink -o edge_sprite.gb edge_sprite.o
	rgbfix -v -p 0xFF edge_sprite.gb

	mkdir -p roms_bin
	mv *.o roms_bin/
	mkdir -p roms
	mv *.gb roms/

edge_sprite.o :
	rgbasm $(ASMFLAGS) -L -o edge_sprite.o debug/test_suite/edge_sprite.s

clean:
	rm -rf bin
	rm -rf roms_bin
	rm -rf roms
	rm -f megagbc
	

