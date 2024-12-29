# megagbc

INCLUDE=include
INCLUDE_GB=$(INCLUDE)/gb
INCLUDE_GBA=$(INCLUDE)/gba
SRC_GB=gb
SRC_GBA=gba
DEBUG=debug

CC = gcc
CPPC = g++
CFLAGS = -O3 `sdl2-config --cflags` -I$(INCLUDE)
LFLAGS = -O3 `sdl2-config --libs` -lm
EXE = megagb

BIN_GB = cartridge.o gb.o gui.o debug.o display.o cpu.o mbc.o mbc1.o mbc2.o mbc3.o mbc5.o
BIN_GBA = gamepak.o arm7tdmi.o gba.o debugGBA.o renderer.o
BIN_IMGUI = imgui.o imgui_tables.o imgui_draw.o imgui_widgets.o imgui_impl_sdlrenderer2.o imgui_impl_sdl2.o

# test suite

ASMFLAGS = -i $(DEBUG)/test_suite/

$(EXE): $(BIN_GB) $(BIN_GBA) $(BIN_IMGUI) main.o
	$(CPPC) $(BIN_GB) $(BIN_GBA) $(BIN_IMGUI) main.o $(LFLAGS) -o $(EXE)
# ----------------------------------------------------------------------
cartridge.o : $(INCLUDE_GB)/cartridge.h \
			  $(SRC_GB)/cartridge.c
	$(CC) -c $(SRC_GB)/cartridge.c $(CFLAGS)

gui.o : $(INCLUDE_GB)/gui.h $(INCLUDE_GB)/cpu.h $(INCLUDE_GB)/debug.h \
		$(SRC_GB)/gui.cpp
	$(CPPC) -c $(SRC_GB)/gui.cpp $(CFLAGS) -Iimgui

gb.o : $(INCLUDE_GB)/gb.h $(INCLUDE_GB)/gui.h $(INCLUDE_GB)/cpu.h \
		$(INCLUDE_GB)/debug.h $(INCLUDE_GB)/display.h $(INCLUDE_GB)/mbc.h \
	   	$(SRC_GB)/gb.c
	$(CC) -c $(SRC_GB)/gb.c $(CFLAGS)

main.o : $(INCLUDE_GB)/gb.h $(INCLUDE_GBA)/gba.h \
		 main.c
	$(CC) -c main.c $(CFLAGS)

cpu.o : $(INCLUDE_GB)/cpu.h $(INCLUDE_GB)/gb.h \
		$(SRC_GB)/cpu.c
	$(CC) -c $(SRC_GB)/cpu.c $(CFLAGS)

mbc.o : $(INCLUDE_GB)/mbc.h $(INCLUDE_GB)/mbc1.h $(INCLUDE_GB)/mbc2.h $(INCLUDE_GB)/mbc3.h \
		$(INCLUDE_GB)/mbc5.h $(INCLUDE_GB)/gb.h $(INCLUDE_GB)/debug.h \
		$(SRC_GB)/mbc.c
	$(CC) -c $(SRC_GB)/mbc.c $(CFLAGS)

mbc1.o : $(INCLUDE_GB)/mbc1.h $(INCLUDE_GB)/mbc.h $(INCLUDE_GB)/debug.h \
		$(SRC_GB)/mbc1.c
	$(CC) -c $(SRC_GB)/mbc1.c $(CFLAGS)

mbc2.o : $(INCLUDE_GB)/mbc2.h $(INCLUDE_GB)/mbc.h $(INCLUDE_GB)/debug.h \
		$(SRC_GB)/mbc2.c
	$(CC) -c $(SRC_GB)/mbc2.c $(CFLAGS)

mbc3.o : $(INCLUDE_GB)/mbc3.h $(INCLUDE_GB)/mbc.h $(INCLUDE_GB)/debug.h \
		$(SRC_GB)/mbc3.c
	$(CC) -c $(SRC_GB)/mbc3.c $(CFLAGS)

mbc5.o : $(INCLUDE_GB)/mbc5.h $(INCLUDE_GB)/mbc.h $(INCLUDE_GB)/debug.h \
		$(SRC_GB)/mbc5.c
	$(CC) -c $(SRC_GB)/mbc5.c $(CFLAGS)

display.o : $(INCLUDE_GB)/display.h $(INCLUDE_GB)/gui.h $(INCLUDE_GB)/gb.h $(INCLUDE_GB)/debug.h\
			$(SRC_GB)/display.c
	$(CC) -c $(SRC_GB)/display.c $(CFLAGS)

debug.o : $(INCLUDE_GB)/debug.h \
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

renderer.o: $(INCLUDE_GBA)/renderer.h \
		  $(SRC_GBA)/renderer.c
	$(CC) -c $(SRC_GBA)/renderer.c $(CFLAGS)

# --------------------------------------------------------------------
imgui.o: imgui/imgui.cpp
	$(CPPC) -c imgui/imgui.cpp $(CFLAGS) -Iimgui
imgui_draw.o: imgui/imgui_draw.cpp
	$(CPPC) -c imgui/imgui_draw.cpp $(CFLAGS) -Iimgui
imgui_tables.o: imgui/imgui_tables.cpp
	$(CPPC) -c imgui/imgui_tables.cpp $(CFLAGS) -Iimgui
imgui_widgets.o: imgui/imgui_widgets.cpp
	$(CPPC) -c imgui/imgui_widgets.cpp $(CFLAGS) -Iimgui
imgui_impl_sdlrenderer2.o: imgui/backends/imgui_impl_sdlrenderer2.cpp
	$(CPPC) -c imgui/backends/imgui_impl_sdlrenderer2.cpp $(CFLAGS) -Iimgui
imgui_impl_sdl2.o: imgui/backends/imgui_impl_sdl2.cpp
	$(CPPC) -c imgui/backends/imgui_impl_sdl2.cpp $(CFLAGS) -Iimgui

# --------------------------------------------------------------------
tests: edge_sprite.o sound.o
	rgblink -o edge_sprite.gb edge_sprite.o
	rgblink -o sound.gb sound.o
	rgbfix -v -p 0xFF edge_sprite.gb
	rgbfix -v -p 0xFF sound.gb

	mkdir -p roms
	mv *.gb roms/

edge_sprite.o :
	rgbasm $(ASMFLAGS) -L -o edge_sprite.o $(DEBUG)/test_suite/edge_sprite.s

sound.o :
	rgbasm $(ASMFLAGS) -L -o sound.o $(DEBUG)/test_suite/sound.s

clean:
	rm *.o	

