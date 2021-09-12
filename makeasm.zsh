rgbasm -L -o test.o test.asm
rgblink -o test.gb test.o
rgbfix -v -p 0xFF test.gb
