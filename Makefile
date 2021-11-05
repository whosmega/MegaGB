CC = gcc
CFLAGS = -O2 `pkg-config --cflags --libs gtk4`
EXE = megagbc

BIN = cartridge.o vm.o main.o debug.o display.o

$(EXE): $(BIN)
	$(CC) $(CFLAGS) $(BIN) -o $(EXE)
	mv *.o bin

cartridge.o : include/cartridge.h \
			  src/cartridge.c
	$(CC) $(CFLAGS) -c src/cartridge.c

vm.o : include/vm.h \
	   src/vm.c
	$(CC) $(CFLAGS) -c src/vm.c

main.o : include/vm.h include/cartridge.h \
		 src/main.c
	$(CC) $(CFLAGS) -c src/main.c

display.o : include/display.h \
			src/display.c
	$(CC) $(CFLAGS) -c src/display.c

debug.o : include/vm.h include/debug.h \
		 src/debug.c
	$(CC) $(CFLAGS) -c src/debug.c

clean:
	rm bin/*.o
	

