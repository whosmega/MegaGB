CC = gcc
CFLAGS = -O3 `sdl2-config --cflags`
LFLAGS = -O3 `sdl2-config --libs`
EXE = megagbc


BIN = cartridge.o vm.o main.o debug.o display.o cpu.o mbc.o mbc1.o mbc2.o

$(EXE): $(BIN)
	$(CC) $(BIN) $(LFLAGS) -o $(EXE)
	mv *.o bin

cartridge.o : include/cartridge.h \
			  src/cartridge.c
	$(CC) -c src/cartridge.c $(CFLAGS)

vm.o : include/vm.h \
	   src/vm.c
	$(CC) -c src/vm.c $(CFLAGS)

main.o : include/vm.h include/cartridge.h \
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

debug.o : include/vm.h include/debug.h \
		 src/debug.c
	$(CC) -c src/debug.c $(CFLAGS)

clean:
	rm bin/*.o
	rm megagbc
	

