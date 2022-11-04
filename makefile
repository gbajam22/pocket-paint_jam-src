
all: pocket-paint.gba obj/main.s

obj/gba_crt0.o: gba_crt0.s
	arm-none-eabi-gcc -nostdlib -x assembler-with-cpp -marm -c gba_crt0.s -o obj/gba_crt0.o

obj/iwram_code.o: iwram_code.s
	arm-none-eabi-gcc -nostdlib -x assembler-with-cpp -marm -c iwram_code.s -o obj/iwram_code.o

obj/invert-table: invert-table.bin
	lz4 -c -l -12 invert-table.bin | tail -c +9 > obj/invert-table

obj/save-file-header: save-file-header.txt
	lz4 -c -l -12 save-file-header.txt | tail -c +9 > obj/save-file-header

obj/cursor-chr: cursor-chr.bin
	lz4 -c -l -12 cursor-chr.bin | tail -c +9 > obj/cursor-chr

obj/main.o: main.c gba_mem_def.h incbin.h obj/invert-table obj/cursor-chr obj/save-file-header
	arm-none-eabi-gcc -nostdlib -mthumb-interwork -mthumb -O2 -fno-strict-aliasing -Wall -save-temps=obj -c main.c -o obj/main.o

obj/pocket-paint.elf obj/pocket-paint.map: gba_mb.ld obj/gba_crt0.o obj/iwram_code.o obj/main.o
	arm-none-eabi-ld -Map obj/pocket-paint.map -T gba_mb.ld -o obj/pocket-paint.elf obj/gba_crt0.o obj/iwram_code.o obj/main.o

gbafix:
	gcc gbafix.c -o gbafix

pocket-paint.gba: gbafix obj/pocket-paint.elf
	arm-none-eabi-objcopy -O binary obj/pocket-paint.elf pocket-paint.gba
	./gbafix -p -t"Pocket Paint" -cAXPE -m00 pocket-paint.gba
# Game ID ripped from "Pokemon Sapphire"
# First character of Game ID could be 4 due to https://krikzz.com/pub/support/everdrive-gba/OS/changelist.txt


#obj/main.s: main.c
#	arm-none-eabi-gcc -nostdlib -mthumb-interwork -mthumb -O2 -fno-strict-aliasing -Wall -c main.c -S -o obj/main.s



