# megagbc

INCLUDE=include
INCLUDE_GB=$(INCLUDE)/gb
INCLUDE_GBA=$(INCLUDE)/gba
SRC_GB=gb
SRC_GBA=gba
DEBUG=debug

CC = gcc
CFLAGS = -O2 `sdl2-config --cflags` -I$(INCLUDE)
LFLAGS = -O2 `sdl2-config --libs` -lm
EXE = megagb

BIN_GB = cartridge.o gb.o debug.o display.o cpu.o mbc.o mbc1.o mbc2.o mbc3.o
BIN_GBA = gamepak.o arm7tdmi.o gba.o debugGBA.o

# test suite

ASMFLAGS = -i $(DEBUG)/test_suite/

$(EXE): $(BIN_GB) $(BIN_GBA) main.o
	$(CC) $(BIN_GB) $(BIN_GBA) main.o $(LFLAGS) -o $(EXE)
	mkdir -p bin
	mv *.o bin
# ----------------------------------------------------------------------
cartridge.o : $(INCLUDE_GB)/cartridge.h \
			  $(SRC_GB)/cartridge.c
	$(CC) -c $(SRC_GB)/cartridge.c $(CFLAGS)

gb.o : $(INCLUDE_GB)/gb.h \
	   $(SRC_GB)/gb.c
	$(CC) -c $(SRC_GB)/gb.c $(CFLAGS)

main.o : $(INCLUDE_GB)/gb.h \
		 main.c
	$(CC) -c main.c $(CFLAGS)

cpu.o : $(INCLUDE_GB)/cpu.h \
		$(SRC_GB)/cpu.c
	$(CC) -c $(SRC_GB)/cpu.c $(CFLAGS)

mbc.o : $(INCLUDE_GB)/mbc.h \
		$(SRC_GB)/mbc.c
	$(CC) -c $(SRC_GB)/mbc.c $(CFLAGS)

mbc1.o : $(INCLUDE_GB)/mbc1.h \
		$(SRC_GB)/mbc1.c
	$(CC) -c $(SRC_GB)/mbc1.c $(CFLAGS)

mbc2.o : $(INCLUDE_GB)/mbc2.h \
		$(SRC_GB)/mbc2.c
	$(CC) -c $(SRC_GB)/mbc2.c $(CFLAGS)

mbc3.o : $(INCLUDE_GB)/mbc3.h \
		$(SRC_GB)/mbc3.c
	$(CC) -c $(SRC_GB)/mbc3.c $(CFLAGS)

display.o : $(INCLUDE_GB)/display.h \
			$(SRC_GB)/display.c
	$(CC) -c $(SRC_GB)/display.c $(CFLAGS)

debug.o : $(INCLUDE_GB)/gb.h $(INCLUDE_GB)/debug.h \
		 $(SRC_GB)/debug.c
	$(CC) -c $(SRC_GB)/debug.c $(CFLAGS)
# --------------------------------------------------------------------

gamepak.o: $(INCLUDE_GBA)/gamepak.h \
		  $(SRC_GBA)/gamepak.c
	$(CC) -c $(SRC_GBA)/gamepak.c $(CFLAGS)

arm7tdmi.o: $(INCLUDE_GBA)/gamepak.h \
		  $(SRC_GBA)/arm7tdmi.c
	$(CC) -c $(SRC_GBA)/arm7tdmi.c $(CFLAGS)

gba.o: $(INCLUDE_GBA)/gba.h \
		  $(SRC_GBA)/gba.c
	$(CC) -c $(SRC_GBA)/gba.c $(CFLAGS)

debugGBA.o: $(INCLUDE_GBA)/debugGBA.h \
		  $(SRC_GBA)/debugGBA.c
	$(CC) -c $(SRC_GBA)/debugGBA.c $(CFLAGS)

# --------------------------------------------------------------------
tests: edge_sprite.o sound.o
	rgblink -o edge_sprite.gb edge_sprite.o
	rgblink -o sound.gb sound.o
	rgbfix -v -p 0xFF edge_sprite.gb
	rgbfix -v -p 0xFF sound.gb

	mkdir -p roms_bin
	mv *.o roms_bin/
	mkdir -p roms
	mv *.gb roms/

edge_sprite.o :
	rgbasm $(ASMFLAGS) -L -o edge_sprite.o $(DEBUG)/test_suite/edge_sprite.s

sound.o :
	rgbasm $(ASMFLAGS) -L -o sound.o $(DEBUG)/test_suite/sound.s

clean:
	rm -rf bin
	rm -rf roms_bin
	rm -rf roms
	rm -f $(EXE)
	

