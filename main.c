#include "gba_mem_def.h"

#define INCBIN_PREFIX
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_ALIGNMENT_INDEX 2
#include "incbin.h"

#define assert(condition) ((void)0)

INCBIN(invert_table_lz4, "obj/invert-table");
INCBIN(save_file_header, "obj/save-file-header");
INCBIN(cursor_chr_lz4, "obj/cursor-chr");

extern void unlz4_vram(volatile void* dest, const void* src, u32 compressed_size);
extern void* memcpy(volatile void* dest, const void* src, u32 num);
extern void* memset(volatile void * ptr, u32 value, u32 num);
extern void (*__isr_table[14])(u8, u64, u32);
extern void load_default_palette(volatile void* pal);
extern void load_default_8bit_bitmap(volatile void* bitmap);
extern void load_default_16bit_bitmap(volatile void* bitmap);
extern u64 get_system_clock();
extern bool cart_is_present();
extern void init_oam(volatile void*);

/*u32 rng_seed;
u8 rng() {
	for (int i = 0; i < 8; ++i) {
		u32 lsb = rng_seed & 0x80000000;
		rng_seed <<= 1;
		if (lsb) rng_seed ^= 0b11000101;
	}
	return rng_seed & 0xff;
}*/

void vsync(void) {
	REG_IF_BIOS = REG_IF_BIOS & ~0x01;
	for (;;) {
		__asm__ __volatile__ ("swi #2" ::: "r0", "r1", "r2", "r3");
		REG_IME = 0;
		u16 flags = REG_IF_BIOS;
		REG_IF_BIOS = flags & ~0x01;
		REG_IME = 1;
		// TODO: do event loop here??
		if (flags & 0x01) return;
	}
}

typedef struct image_save
{
	u32 magic;
	u32 save_count;    // shared between the 2 save slots
	s32 cursor_x;
	s32 cursor_y;
	u16 cursor_rgb;
	u16 cursor_pal_flags;
	u8  layer_type[4];
	u16 bg_top_rgb;
	u16 bg_bot_rgb;
	s32 zoom_scroll_x;
	s32 zoom_scroll_y;
} image_save;

void set_color_picker_palette(volatile COLOR* mem, COLOR cur) {
	for (u32 x = 0; x < 32; ++x) {
		mem[0+x]  = (cur & 0b0111111111100000) + x;
		mem[32+x] = (cur & 0b0111110000011111) + (x<<5);
		mem[64+x] = (cur & 0b0000001111111111) + (x<<10);
	}
}

void set_main_cursor_palette(volatile COLOR* mem, COLOR cur, COLOR bg_color) {
	mem[0] = RGB15(31, 0, 31);  // sprite transperency
	mem[1] = RGB15(0, 0, 0);    // black
	mem[2] = RGB15(29, 26, 21); // subpixel anti-alias, black left, white right
	mem[3] = RGB15(26, 26, 26); // gray
	mem[4] = RGB15(21, 26, 29); // subpixel anti-alias, white left, black right
	mem[5] = RGB15(31, 31, 31); // white
	// 6 unused
	mem[7] = bg_color;          // always bg color 0
	// 8,9,10,11 unused
	mem[12] = (cur & 0b0000000000011111); // rgb widget R
	mem[13] = (cur & 0b1000001111100000); // rgb widget G
	mem[14] = (cur & 0b0111110000000000); // rgb widget B
	mem[15] = cur; // rgb widget color

	mem[16] = RGB15(31, 0, 31);  // sprite transperency
	mem[17] = RGB15(31, 31, 31); // inverse from above
	mem[18] = RGB15(21, 26, 29);
	mem[19] = RGB15(26, 26, 26);
	mem[20] = RGB15(29, 26, 21);
	mem[21] = RGB15(0, 0, 0);
	//22 unused
	mem[23] = RGB15(8, 8, 8);    // BG border
	for (int x = 0; x < 4; ++x) {
		mem[24+x] = RGB15(0, 0, 0);
		mem[24+x+4] = RGB15(31, 31, 31);      // marching ants
	}

//	pal_obj_bank[0][5] = RGB15(0, 12, 31);  // window title gradent
//	pal_obj_bank[0][6] = RGB15(0, 10, 31);
//	pal_obj_bank[0][7] = RGB15(0, 8, 31);
//	pal_obj_bank[0][8] = RGB15(0, 6, 31);
//	pal_obj_bank[0][9] = RGB15(0, 4, 31);
//	pal_obj_bank[0][10] = RGB15(0, 2, 31);
//	pal_obj_bank[0][11] = RGB15(0, 0, 31);
}

typedef struct pixel_plot_surface
{
	volatile void* buffer;
	// bits are laied out all in little endian order. the top-left pixel is lsb.
	u32 bpp;            // bits to move right 1 pixel, must be 1, 2, 4, 8, or 16
	u32 pitch;          // bytes to move down 1 pixel
	u32 width;          // in pixels. exceeding this counts as a tile movement
	u32 height;         //     if == 0, then not tiled.
	u32 tile_pitch_h;   // bytes to move right 1 tile, 0 to warp in place
	u32 tile_pitch_v;   // bytes to move down 1 tile
	bool vram_bus;      // buffer is pointing to VRAM which can't take byte writes
} pixel_plot_surface;


extern void plot_pixel(const pixel_plot_surface* surface, u32 x, u32 y, u32 color);

extern u32 read_pixel(const pixel_plot_surface* surface, u32 x, u32 y);

u8 invert_table[4096];
INLINE u32 get_invert_table_bit (u16 color){
	return ((invert_table[(color&0x7fff) >> 3]) >> (color&7)) & 1;
}

enum cursor_state {
	CUR_ST_IDLE,
	CUR_ST_MOVE,
	CUR_ST_DRAW
};

void draw_line(const pixel_plot_surface* surface, s32 x0, s32 y0, s32 x1, s32 y1, u32 color) {
	x0 /= 65536;
	x1 /= 65536;
	y0 /= 65536;
	y1 /= 65536;
	s32 dx = (x0 < x1) ? x1 - x0 : x0 - x1;
	s32 sx = (x0 < x1) ? 1 : -1;
	s32 dy = (y0 < y1) ? -(y1 - y0) : -(y0 - y1);
	s32 sy = (y0 < y1) ? 1 : -1;
	s32 error = dx + dy;

	while (1) {
		plot_pixel(surface, x0, y0, color);
		if ((x0 == x1) && (y0 == y1)) break;
		s32 e2 = 2 * error;
		if (e2 >= dy) {
			if (x0 == x1) break;
			error = error + dy;
			x0 = x0 + sx;
		}
		if (e2 <= dx) {
			if (y0 == y1) break;
			error = error + dx;
			y0 = y0 + sy;
		}
	}
}

u8* save_data = (u8*)(MEM_EWRAM + EWRAM_SIZE - 0x20000);
#define SAVE_DATA_SIZE 0x20000
#define SAVE_DATA_MAGIC_OFFSET 0x1C00
#define SAVE_DATA_PAL_OFFSET 0x1E00
#define SAVE_DATA_M4_OFFSET 0x2000
#define SAVE_DATA_M3_OFFSET 0xC000

const u8 save_magic[8] = "\x1A\xC1\xBF\nAXPE";

u32 flash_get_chip_info() {
	REG_IME = 0;
	u64 current_time;

	// proactively save sram values in case bus is acually to a SRAM chip. 
//	u8 sram_restore_byte_1 = *(vu8 *)(0x0E005555);  // this also flags mGBA that we want flash
//	u8 sram_restore_byte_2 = *(vu8 *)(0x0E002AAA);

	// Enter chip ID mode
	*(vu8 *)(0x0E005555) = 0xAA;
	*(vu8 *)(0x0E002AAA) = 0x55;
	*(vu8 *)(0x0E005555) = 0x90;

	// wait 20ms
	current_time = get_system_clock();
	while (get_system_clock() - current_time < 335872) {;}

	// get IDs
	u32 chip_id = *(vu8 *)(0x0E000001) << 8;
	chip_id |= *(vu8 *)(0x0E000000);

    // Leave chip ID mode
	*(vu8 *)(0x0E005555) = 0xAA;
	*(vu8 *)(0x0E002AAA) = 0x55;
	*(vu8 *)(0x0E005555) = 0xF0;

	// special case where sanyo 128K flash needs 2 writes of CMD 0xf0 to exit
	if ((chip_id & 0xff) == 0xc2) *(vu8 *)(0x0E005555) = 0xF0;

	// wait 20ms
	current_time = get_system_clock();
	while (get_system_clock() - current_time < 335872) {;}

	const u32 known_flash_chips[6] = {0x0100D4BF, 0x01001CC2, 0x01001B32, 0x01003D1F, 0x02001362, 0x020009C2};
	for (u32 i = 0; i < 6; i++) {
        if ((chip_id & 0xffff) == (known_flash_chips[i] & 0xffff)) {
            chip_id = known_flash_chips[i];
            break;
        }
    }
//    if ((chip_id >> 16) == 0) {
//    	// flash chip not found, proceed as SRAM
//    	// by first restoring overwritted values
//		*(vu8 *)(0x0E002AAA) = sram_restore_byte_2;
//		*(vu8 *)(0x0E005555) = sram_restore_byte_1;
//	}

	REG_IME = 1;

	return chip_id;
}

void flash_load() {
	if (!cart_is_present()) return;
	u32 chip_id = flash_get_chip_info();
	REG_IME = 0;

	if ((chip_id >> 16) == 0x0200) {
		// first bank of 128K
		*(vu8 *)(0x0E005555) = 0xAA;
		*(vu8 *)(0x0E002AAA) = 0x55;
		*(vu8 *)(0x0E005555) = 0xB0;
		*(vu8 *)(0x0E000000) = 0;
	}

	for (u32 i = 0; i < 65536; i++) {
		save_data[i] = sram_mem[i];
	}

	if ((chip_id >> 16) == 0x0200) {
		// second bank of 128K
		*(vu8 *)(0x0E005555) = 0xAA;
		*(vu8 *)(0x0E002AAA) = 0x55;
		*(vu8 *)(0x0E005555) = 0xB0;
		*(vu8 *)(0x0E000000) = 1;

		for (u32 i = 0; i < 65536; i++) {
			save_data[i+65536] = sram_mem[i];
		}
	}

	REG_IME = 1;
}

void flash_save() {
	if (!cart_is_present()) return;
	u64 current_time;
	u32 chip_id = flash_get_chip_info();
	REG_IME = 0;

	if ((chip_id >> 16) == 0) {
		// we got SRAM
		for (u32 i = 0; i < 65536; i++) {
			sram_mem[i] = save_data[i];
		}
		REG_IME = 1;
		return;
	}

	// erase whole chip
	*(vu8 *)(0x0E005555) = 0xAA;
	*(vu8 *)(0x0E002AAA) = 0x55;
	*(vu8 *)(0x0E005555) = 0x80;
	*(vu8 *)(0x0E005555) = 0xAA;
	*(vu8 *)(0x0E002AAA) = 0x55;
	*(vu8 *)(0x0E005555) = 0x10;

	current_time = get_system_clock();
	while ((*(vu8 *)(0x0E000000) != 0xff) && (get_system_clock() - current_time < 335872)) {;}
	if ((*(vu8 *)(0x0E000000) != 0xff) && (*(vu8 *)(0x0E000000) != 0x00)) {
		// error, could not erase flash
		// issue terminate comand for Macronix chips
		if ((chip_id & 0xff) == 0xc2) *(vu8 *)(0x0E005555) = 0xF0;
		REG_IME = 1;
		return;
	}

	if ((chip_id >> 16) == 0x0200) {
		// first bank of 128K
		*(vu8 *)(0x0E005555) = 0xAA;
		*(vu8 *)(0x0E002AAA) = 0x55;
		*(vu8 *)(0x0E005555) = 0xB0;
		*(vu8 *)(0x0E000000) = 0;
	}

	for (u32 i = 0; i < 65536; i++) {
		*(vu8 *)(0x0E005555) = 0xAA;
		*(vu8 *)(0x0E002AAA) = 0x55;
		*(vu8 *)(0x0E005555) = 0xA0;
		*(vu8 *)(0x0E000000 + i) = save_data[i];

		current_time = get_system_clock();
		while ((*(vu8 *)(0x0E000000 + i) != save_data[i]) && (get_system_clock() - current_time < 335872)) {;}
		if (*(vu8 *)(0x0E000000 + i) != save_data[i]) {
			// error, could not write byte
			// issue terminate comand for Macronix chips
			if ((chip_id & 0xff) == 0xc2) *(vu8 *)(0x0E005555) = 0xF0;
			//REG_IME = 1;
			//return;
		}
	}

	if ((chip_id >> 16) == 0x0200) {
		// second bank of 128K
		*(vu8 *)(0x0E005555) = 0xAA;
		*(vu8 *)(0x0E002AAA) = 0x55;
		*(vu8 *)(0x0E005555) = 0xB0;
		*(vu8 *)(0x0E000000) = 1;

		for (u32 i = 0; i < 65536; i++) {
			*(vu8 *)(0x0E005555) = 0xAA;
			*(vu8 *)(0x0E002AAA) = 0x55;
			*(vu8 *)(0x0E005555) = 0xA0;
			*(vu8 *)(0x0E000000 + i) = save_data[i+65536];

			current_time = get_system_clock();
			while ((*(vu8 *)(0x0E000000 + i) != save_data[i+65536]) && (get_system_clock() - current_time < 335872)) {;}
			if (*(vu8 *)(0x0E000000 + i) != save_data[i+65536]) {
				// error, could not write byte
				// issue terminate comand for Macronix chips
				if ((chip_id & 0xff) == 0xc2) *(vu8 *)(0x0E005555) = 0xF0;
				//REG_IME = 1;
				//return;
			}
		}
	}

	REG_IME = 1;
}

COLOR magnify_palette[256];

int main()
{
	bool quit = false;
	u16 reset_keys_up = 0;
	u16 keys_was_up = 0;
	u16 keys_now_down = 0xffff;

	// turn down sound bias due to no sound in this jam version.
	__asm__ __volatile__ ("mov r0, #0\nswi #0x19" ::: "r0", "r1", "r2", "r3");  

	load_default_palette(pal_bg_mem);
	load_default_palette(pal_obj_mem);
	//load_default_8bit_bitmap(m4_mem);
	load_default_16bit_bitmap(m3_mem);

	unlz4_vram(invert_table, invert_table_lz4_data, invert_table_lz4_size);

	memset(save_data, '\n', SAVE_DATA_MAGIC_OFFSET);
	unlz4_vram(save_data, save_file_header_data, save_file_header_size);
	memcpy(&save_data[SAVE_DATA_MAGIC_OFFSET], save_magic, 8);
	load_default_palette(&save_data[SAVE_DATA_PAL_OFFSET]);
	load_default_8bit_bitmap(&save_data[SAVE_DATA_M4_OFFSET]);
	load_default_16bit_bitmap(&save_data[SAVE_DATA_M3_OFFSET]);

	set_main_cursor_palette(&pal_obj_bank[0][0], 0x6511, pal_bg_bank[0][0]);
	set_color_picker_palette(&pal_obj_bank[2][0], 0x6511);

	const pixel_plot_surface screen_m3_surf = {m3_mem, 16, 480, 0, 0, 0, 0, true};
	const pixel_plot_surface screen_m4_surf = {m4_mem, 8, 240, 0, 0, 0, 0, true};
	(void)screen_m4_surf;

	// all this pixel ploting code for widgets is compiled into this file
	unlz4_vram(&(tile_mem_obj[0][512]), cursor_chr_lz4_data, cursor_chr_lz4_size);

/*	const pixel_plot_surface widgets_4bbp = {&(tile_mem_obj[0][512]), 4, 4, 8, 8, 32, 1024, true};
	const pixel_plot_surface widgets_8bbp = {&(tile_mem_obj[0][512]), 8, 8, 8, 8, 64, 1024, true};

	// color edit box
	for(int x = 0; x < 32; ++x) {
		plot_pixel(&widgets_8bbp, x, 2, 5);
		for(int y = 24; y <= 31; ++y) {
			plot_pixel(&widgets_8bbp, x, y, 5);
		}
		for(int y = 0; y < 7; ++y) {
			plot_pixel(&widgets_8bbp, x, 3+y, 32*1+x);
			plot_pixel(&widgets_8bbp, x, 10+y, 32*2+x);
			plot_pixel(&widgets_8bbp, x, 17+y, 32*3+x);
		}
	}
	for(int y = 25; y <= 30; ++y) {
		for(int x = 1; x <= 6; ++x) {
			plot_pixel(&widgets_8bbp, x, y, 15);
		}
	}
	for(int x = 25; x <= 30; ++x) {
		plot_pixel(&widgets_8bbp, x, 25, 12);
		plot_pixel(&widgets_8bbp, x, 26, 12);
		plot_pixel(&widgets_8bbp, x, 27, 13);
		plot_pixel(&widgets_8bbp, x, 28, 13);
		plot_pixel(&widgets_8bbp, x, 29, 14);
		plot_pixel(&widgets_8bbp, x, 30, 14);
	}
	plot_pixel(&widgets_8bbp, 0, 2, 2);
	plot_pixel(&widgets_8bbp, 31, 2, 4);
	plot_pixel(&widgets_8bbp, 0, 24, 2);
	plot_pixel(&widgets_8bbp, 31, 24, 4);
	plot_pixel(&widgets_8bbp, 0, 31, 2);
	plot_pixel(&widgets_8bbp, 31, 31, 4);

	// black boarder for color edit box
	for (int x = 0; x < 34; ++x){
		for (int y = 32; y < 64; ++y) {
			plot_pixel(&widgets_4bbp, x, y, 1);
		}
	}

	// small cursor
	for (int x = 0; x < 3; ++x){
		plot_pixel(&widgets_4bbp, 64+4,   0+x, 1);
		plot_pixel(&widgets_4bbp, 64+0+x, 4,   1);
		plot_pixel(&widgets_4bbp, 64+4,   6+x, 1);
		plot_pixel(&widgets_4bbp, 64+6+x, 4,   1);
	}

	// large cursor
	for (int x = 0; x < 5; ++x){
		for (int y = 0; y < 2; ++y) {
			plot_pixel(&widgets_4bbp, 64+ 7+y, 16+ 0+x, 1);
			plot_pixel(&widgets_4bbp, 64+ 0+x, 16+ 7+y, 1);
			plot_pixel(&widgets_4bbp, 64+ 7+y, 16+11+x, 1);
			plot_pixel(&widgets_4bbp, 64+11+x, 16+ 7+y, 1);
		}
	}

	// color edit active cursor
	const u8 active_cursor_pixels[] = {
		1,1,1,1,1,0,0,0,
		0,1,1,1,0,0,0,0,
		0,0,1,0,0,0,0,0,
		0,0,0,0,0,0,0,0,
		0,0,1,0,0,0,0,0,
		0,1,1,1,0,0,0,0,
		1,1,1,1,1,0,0,0,
		0,0,0,0,0,0,0,0,
	};
	for (int x = 0; x < 8; ++x){
		for (int y = 0; y < 8; ++y) {
			plot_pixel(&widgets_4bbp, x+128, y+8, active_cursor_pixels[y*8+x]);
		}
	}
	// color edit inactive cursor
	plot_pixel(&widgets_4bbp, 136+2, 8+1, 1);
	plot_pixel(&widgets_4bbp, 136+2, 8+2, 1);
	plot_pixel(&widgets_4bbp, 136+2, 8+4, 1);
	plot_pixel(&widgets_4bbp, 136+2, 8+5, 1);

	// color swatch
	for (int x = 0; x < 10; ++x){
		for (int y = 6; y < 16; ++y) {
			plot_pixel(&widgets_4bbp, 112+x, y, 1);
		}
	}
	for (int x = 1; x < 9; ++x){
		for (int y = 7; y < 15; ++y) {
			plot_pixel(&widgets_4bbp, 112+x, y, 5);
		}
	}
	for (int x = 2; x < 8; ++x){
		for (int y = 8; y < 14; ++y) {
			plot_pixel(&widgets_4bbp, 112+x, y, 15);
		}
	}
	plot_pixel(&widgets_4bbp, 112+1, 7, 2);
	plot_pixel(&widgets_4bbp, 112+1,14, 2);
	plot_pixel(&widgets_4bbp, 112+8, 7, 4);
	plot_pixel(&widgets_4bbp, 112+8,14, 4);

	// color swatch with 2 digits
	for (int x = 0; x < 18; ++x){
		for (int y = 6; y < 16; ++y) {
			plot_pixel(&widgets_4bbp, 80+x, y, 1);
		}
	}
	for (int x = 1; x < 17; ++x){
		for (int y = 7; y < 15; ++y) {
			plot_pixel(&widgets_4bbp, 80+x, y, 5);
		}
	}
	for (int x = 2; x < 8; ++x){
		for (int y = 8; y < 14; ++y) {
			plot_pixel(&widgets_4bbp, 80+x, y, 15);
		}
	}
	plot_pixel(&widgets_4bbp, 80+1,  7, 2);
	plot_pixel(&widgets_4bbp, 80+1, 14, 2);
	plot_pixel(&widgets_4bbp, 80+16, 7, 4);
	plot_pixel(&widgets_4bbp, 80+16,14, 4);

	// color swatch with 4 digits
	for (int x = 0; x < 26; ++x){
		for (int y = 6; y < 16; ++y) {
			plot_pixel(&widgets_4bbp, 80+x, 16+y, 1);
		}
	}
	for (int x = 1; x < 25; ++x){
		for (int y = 7; y < 15; ++y) {
			plot_pixel(&widgets_4bbp, 80+x, 16+y, 5);
		}
	}
	for (int x = 2; x < 8; ++x){
		for (int y = 8; y < 14; ++y) {
			plot_pixel(&widgets_4bbp, 80+x, 16+y, 15);
		}
	}
	plot_pixel(&widgets_4bbp, 80+1, 16+ 7, 2);
	plot_pixel(&widgets_4bbp, 80+1, 16+14, 2);
	plot_pixel(&widgets_4bbp, 80+24,16+ 7, 4);
	plot_pixel(&widgets_4bbp, 80+24,16+14, 4);

	// digits
	const u8 digits_pixels[] = {
		4,1,2,0,1,5,1,0,1,5,1,0,1,5,1,0,4,1,2,0,
		5,1,5,0,4,1,5,0,5,1,5,0,5,1,5,0,4,1,2,0,
		1,1,2,0,5,5,1,0,5,1,2,0,1,5,5,0,1,1,1,0,
		1,1,2,0,5,5,1,0,5,1,2,0,5,5,1,0,1,1,2,0,
		1,5,1,0,1,5,1,0,1,1,1,0,5,5,1,0,5,5,1,0,
		1,1,1,0,1,5,5,0,1,1,2,0,5,5,1,0,1,1,2,0,
		4,1,1,0,1,5,5,0,1,1,2,0,1,5,1,0,4,1,2,0,
		1,1,1,0,5,5,1,0,5,4,2,0,5,1,5,0,5,1,5,0,
		4,1,2,0,1,5,1,0,4,1,2,0,1,5,1,0,4,1,2,0,
		4,1,2,0,1,5,1,0,4,1,1,0,5,5,1,0,5,5,1,0,
		4,1,2,0,1,5,1,0,1,1,1,0,1,5,1,0,1,5,1,0,
		1,1,5,0,1,5,1,0,1,1,5,0,1,5,1,0,1,1,5,0,
		4,1,1,0,1,5,5,0,1,5,5,0,1,5,5,0,4,1,1,0,
		1,1,5,0,1,5,1,0,1,5,1,0,1,5,1,0,1,1,5,0,
		1,1,1,0,1,5,5,0,1,1,1,0,1,5,5,0,1,1,1,0,
		1,1,1,0,1,5,5,0,1,1,1,0,1,5,5,0,1,5,5,0
	};
	for (int d = 0; d < 16; ++d) {
		for (int x = 0; x < 4; ++x){
			for (int y = 0; y < 5; ++y) {
				plot_pixel(&widgets_4bbp, 128+(d*8)+x, y, digits_pixels[(d*4*5)+(y*4)+x]);
			}
		}
	}

	// saving and loading icons
	const u8 save_load_icons[] = {
		0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,1,2,5,5,1,4,1,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,1,5,5,5,1,5,1,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,1,2,5,5,5,4,1,3,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,3,1,1,1,1,1,3,3,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,1,1,1,1,1,1,1,1,1,1,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,1,2,5,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
		1,3,1,5,1,1,5,5,1,1,1,5,5,1,1,5,1,1,5,1,5,1,5,1,1,5,1,1,5,5,1,1,
		1,3,1,5,1,5,1,1,5,1,5,1,1,5,1,5,1,1,5,1,5,1,5,4,1,5,1,5,1,1,5,1,
		1,3,1,5,1,5,1,1,1,1,5,1,1,5,1,5,1,1,5,1,5,1,5,5,1,5,1,5,1,1,1,1,
		1,3,1,5,1,1,5,5,1,1,5,5,5,5,1,5,1,1,5,1,5,1,5,5,5,5,1,5,1,5,5,1,
		1,3,1,5,1,1,1,1,5,1,5,1,1,5,1,5,1,1,5,1,5,1,5,1,5,5,1,5,1,1,5,1,
		1,3,1,5,1,5,1,1,5,1,5,1,1,5,1,1,5,5,1,1,5,1,5,1,2,5,1,5,1,1,5,1,
		1,3,1,2,1,1,5,5,1,1,5,1,1,5,1,1,5,5,1,1,5,1,5,1,1,5,1,1,5,5,4,1,
		0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
		0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,1,2,5,5,1,4,1,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,1,5,5,5,1,5,1,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,1,2,5,5,5,4,1,3,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,3,1,1,1,1,1,3,3,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,3,3,3,3,3,3,3,3,3,3,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,3,3,1,1,1,1,1,1,1,1,1,1,3,3,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
		1,5,1,1,1,1,5,5,1,1,1,5,5,1,1,5,5,4,1,1,5,1,5,1,1,5,1,1,5,5,1,1,
		1,5,1,1,1,5,1,1,5,1,5,1,1,5,1,5,1,2,4,1,5,1,5,4,1,5,1,5,1,1,5,1,
		1,5,1,1,1,5,1,1,5,1,5,1,1,5,1,5,1,1,5,1,5,1,5,5,1,5,1,5,1,1,1,1,
		1,5,1,1,1,5,1,1,5,1,5,5,5,5,1,5,1,1,5,1,5,1,5,5,5,5,1,5,1,5,5,1,
		1,5,1,1,1,5,1,1,5,1,5,1,1,5,1,5,1,1,5,1,5,1,5,1,5,5,1,5,1,1,5,1,
		1,5,1,1,1,5,1,1,5,1,5,1,1,5,1,5,1,2,4,1,5,1,5,1,2,5,1,5,1,1,5,1,
		1,5,5,5,1,1,5,5,1,1,5,1,1,5,1,5,5,4,1,1,5,1,5,1,1,5,1,1,5,5,4,1,
		0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
	};
	for (int x = 0; x < 32; ++x){
		for (int y = 0; y < 32; ++y) {
			plot_pixel(&widgets_4bbp, x+0, y+64, save_load_icons[y*32+x]);
		}
	}

	// magnifying glass frame
	for (int x = 0; x < 64; ++x){
		plot_pixel(&widgets_4bbp, 64+x, 32, 1);
		plot_pixel(&widgets_4bbp, 64+x, 32+63, 1);
		plot_pixel(&widgets_4bbp, 64, 32+x, 1);
		plot_pixel(&widgets_4bbp, 64+63, 32+x, 1);
	}
	for (int x = 0; x < 62; ++x){
		plot_pixel(&widgets_4bbp, 64+x+1, 32+1, 5);
		plot_pixel(&widgets_4bbp, 64+x+1, 32+62, 5);
		plot_pixel(&widgets_4bbp, 64+1, 32+x+1, 5);
		plot_pixel(&widgets_4bbp, 64+62, 32+x+1, 5);
	}
	plot_pixel(&widgets_4bbp, 64+1, 32+ 1, 2);
	plot_pixel(&widgets_4bbp, 64+1, 32+62, 2);
	plot_pixel(&widgets_4bbp, 64+62,32+ 1, 4);
	plot_pixel(&widgets_4bbp, 64+62,32+62, 4);

	// magnifying contents
	for(int y = 0; y < 11; ++y) {
		for(int x = 0; x < 11; ++x) {
			for(int ky = 0; ky < 3; ++ky) {
				for(int kx = 0; kx < 3; ++kx) {
					plot_pixel(&widgets_8bbp, 64+x*3+kx, 32+y*3+ky, (y*11+x)|0x80);
				}
			}
		}
	}
*/

	REG_IF = IRQ_VBLANK;
	REG_DISPSTAT = DSTAT_VBL_IRQ;
	REG_DISPCNT = DCNT_MODE3 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_2D | DCNT_OAM_HBL | DCNT_WIN0;

	s32 pen_x = 66*0x10000;
	s32 pen_y = 80*0x10000;
	s32 pen_dx = 0;
	s32 pen_dy = 0;
	//u32 pen_color = 0x5d4f;
	u32 pen_color = 0x6511;
	u32 pen_state = 0;       // 0: idle, 1: free move, 2: momentum move
	u32 pen_held_timer = 0;
	u32 pen_cursor_invert = 0;
	u32 pen_select_old_color = 0x6511;
	u32 pen_select_pos = 0;
	u32 pen_display_color_text = false;

	while (!quit) {
		vsync();
		keys_was_up = ~keys_now_down;
		keys_now_down = ~REG_KEYINPUT;
		u16 keys_pressed = keys_now_down & keys_was_up;
		u16 keys_released = ~(keys_now_down | keys_was_up);
		reset_keys_up |= keys_released;

		u32 old_pen_px_x = pen_x>>16;
		u32 old_pen_px_y = pen_y>>16;

		bool compute_new_pen_invert = false;
		switch (pen_state)
		{
			case 0:
				pen_dx = 0;
				pen_dy = 0;
				pen_x = pen_x >> 16;
				pen_y = pen_y >> 16;
				if (keys_pressed & KEY_UP) pen_y = (pen_y + 160-1)%160;
				if (keys_pressed & KEY_DOWN) pen_y = (pen_y + 1)%160;
				if (keys_pressed & KEY_LEFT) pen_x = (pen_x + 240-1)%240;
				if (keys_pressed & KEY_RIGHT) pen_x = (pen_x + 1)%240;
				if (((keys_now_down & KEY_B) && (keys_pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT))) || (keys_pressed & KEY_B)) {
					pen_color = read_pixel(&screen_m3_surf, pen_x, pen_y);
					pen_select_old_color = pen_color;
					compute_new_pen_invert = true;
					pen_display_color_text = true;
				} else if (((keys_now_down & KEY_A) && (keys_pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT))) || (keys_pressed & KEY_A)) {
					plot_pixel(&screen_m3_surf, pen_x, pen_y, pen_color);
				}
				pen_x = (pen_x << 16) + 0x8000;
				pen_y = (pen_y << 16) + 0x8000;
				if (keys_now_down & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
					if (pen_held_timer < 24-1) ++pen_held_timer;
				} else {
					if (pen_held_timer > 0) --pen_held_timer;
				}
				if (keys_pressed & KEY_B) pen_held_timer = 0;
				if (pen_held_timer >= 15) pen_state = 1;
				if (keys_pressed & KEY_R) {
					pen_held_timer = 0;
					pen_state = 2;
				}
			break; case 1:
				if (keys_now_down & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) {
					if (pen_held_timer < 24-1) ++pen_held_timer;
				} else {
					if (pen_held_timer > 0) --pen_held_timer;
				}
				if ((~keys_now_down & KEY_B) && (keys_now_down & KEY_A)) {
					pen_dx *= 7;
					pen_dy *= 7;
					s32 dx = 0;
					s32 dy = 0;
					if (keys_now_down & KEY_LEFT) dx -= 1;
					if (keys_now_down & KEY_RIGHT) dx += 1;
					if (keys_now_down & KEY_UP) dy -= 1;
					if (keys_now_down & KEY_DOWN) dy += 1;
					dx = (dy == 0) ? dx*0x16a0a : dx*0x10000;
					dy = (dx == 0) ? dy*0x16a0a : dy*0x10000;
					pen_dx += dx + ((pen_dx < 0) ? -4 : 4);
					pen_dy += dy + ((pen_dy < 0) ? -4 : 4);
					pen_dx /= 8;
					pen_dy /= 8;
					s32 i;
					i = pen_dx - dx;
					if (-7 < i && i < 7) pen_dx = dx;   // clamp to dead zone
					i = pen_dy - dy;
					if (-7 < i && i < 7) pen_dy = dy;
				} else {
					pen_dx *= 3;
					pen_dy *= 3;
					s32 dx = 0;
					s32 dy = 0;
					if (keys_now_down & KEY_LEFT) dx -= 1;
					if (keys_now_down & KEY_RIGHT) dx += 1;
					if (keys_now_down & KEY_UP) dy -= 1;
					if (keys_now_down & KEY_DOWN) dy += 1;
					dx = (dy == 0) ? dx*0x2d414 : dx*0x20000;
					dy = (dx == 0) ? dy*0x2d414 : dy*0x20000;
					pen_dx += dx + ((pen_dx < 0) ? -2 : 2);
					pen_dy += dy + ((pen_dy < 0) ? -2 : 2);
					pen_dx /= 4;
					pen_dy /= 4;
					s32 i;
					i = pen_dx - dx;
					if (-3 < i && i < 3) pen_dx = dx;   // clamp to dead zone
					i = pen_dy - dy;
					if (-3 < i && i < 3) pen_dy = dy;
				}
				u32 old_pen_x = pen_x;
				u32 old_pen_y = pen_y;
				pen_x += pen_dx;
				pen_y += pen_dy;
				// TODO: split this warp into 2 draw line commands
				if (pen_x < 0) {
					if (keys_pressed & KEY_LEFT) {
						old_pen_x = 240*0x10000-1;
						pen_x += 240*0x10000;
					} else {
						pen_x = 0;
					}
				}
				if (pen_x >= 240*0x10000) {
					if (keys_pressed & KEY_RIGHT) {
						old_pen_x = 0;
						pen_x -= 240*0x10000;
					} else {
						pen_x = 240*0x10000-1;
					}
				}
				if (pen_y < 0) {
					if (keys_pressed & KEY_UP) {
						old_pen_y = 160*0x10000-1;
						pen_y += 160*0x10000;
					} else {
						pen_y = 0;
					}
				}
				if (pen_y >= 160*0x10000) {
					if (keys_pressed & KEY_DOWN) {
						old_pen_y = 0;
						pen_y -= 160*0x10000;
					} else {
						pen_y = 160*0x10000-1;
					}
				}
				if (keys_now_down & KEY_B) {
					pen_color = read_pixel(&screen_m3_surf, pen_x>>16, pen_y>>16);
					pen_select_old_color = pen_color;
					compute_new_pen_invert = true;
					pen_display_color_text = true;
				} else if (keys_now_down & KEY_A) {
					draw_line(&screen_m3_surf, old_pen_x, old_pen_y, pen_x, pen_y, pen_color);
					//plot_pixel(&test_screen_surf, pen_x>>16, pen_y>>16, pen_color);
				}
				if (keys_pressed & KEY_B) pen_held_timer = 0;
				if ((~keys_now_down & KEY_B) && (keys_now_down & KEY_A)) {
					if (pen_held_timer < 9) pen_state = 0;
				} else {
					if (pen_held_timer < 15) pen_state = 0;
				}
				if (keys_pressed & KEY_R) {
					pen_held_timer = 0;
					pen_state = 2;
				}
			break; case 2:
				pen_dx = 0;
				pen_dy = 0;
				pen_display_color_text = true;
				// Delay Auto Shift
				if (keys_now_down & (KEY_LEFT | KEY_RIGHT)) {
					++pen_held_timer;
					if (pen_held_timer > 12) {
						keys_pressed |= (keys_now_down & (KEY_LEFT | KEY_RIGHT));
						pen_held_timer -= 3;
					}
				} else {
					pen_held_timer = 0;
				}
				if (keys_pressed & KEY_UP) pen_select_pos = (pen_select_pos + 3-1)%3;
				if (keys_pressed & KEY_DOWN) pen_select_pos = (pen_select_pos + 1)%3;
				if (pen_select_pos == 0) {
					u32 c = (pen_color & 0b0000000000011111);
					if (keys_pressed & KEY_RIGHT) {
						c = (c + 1) & 31;
					}
					if (keys_pressed & KEY_LEFT) {
						c = (c + 32-1) & 31;
					}
					pen_color = (pen_color & 0b0111111111100000) | c;
				} else if (pen_select_pos == 1) {
					u32 c = (pen_color & 0b0000001111100000) >> 5;
					if (keys_pressed & KEY_RIGHT) {
						c = (c + 1) & 31;
					}
					if (keys_pressed & KEY_LEFT) {
						c = (c + 32-1) & 31;
					}
					pen_color = (pen_color & 0b0111110000011111) | (c << 5);
				} else if (pen_select_pos == 2) {
					u32 c = (pen_color & 0b0111110000000000) >> 10;
					if (keys_pressed & KEY_RIGHT) {
						c = (c + 1) & 31;
					}
					if (keys_pressed & KEY_LEFT) {
						c = (c + 32-1) & 31;
					}
					pen_color = (pen_color & 0b0000001111111111) | (c << 10);
				}
				if (keys_pressed & KEY_B) pen_color = pen_select_old_color;
				if (keys_pressed & (KEY_R | KEY_A | KEY_B)) {
					pen_held_timer = 0;
					pen_state = 0;
				}
			break; default:
		}
		
		u32 pen_px_x = pen_x>>16;
		u32 pen_px_y = pen_y>>16;

		if (old_pen_px_x != pen_px_x) compute_new_pen_invert = true;
		if (old_pen_px_y != pen_px_y) compute_new_pen_invert = true;

		if (compute_new_pen_invert) {
			const u8 convolution_kernel[13*13] = {
			0, 0, 0, 0, 0, 4, 8, 4, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 4, 35, 71, 35, 4, 0, 0, 0, 0,
			0, 0, 0, 0, 12, 98, 199, 98, 12, 0, 0, 0, 0,
			0, 0, 0, 1, 15, 126, 255, 126, 15, 1, 0, 0, 0,
			0, 4, 12, 15, 24, 102, 200, 102, 24, 15, 12, 4, 0,
			4, 35, 98, 126, 102, 70, 79, 70, 102, 126, 98, 35, 4,
			8, 71, 199, 255, 200, 79, 31, 79, 200, 255, 199, 71, 8,
			4, 35, 98, 126, 102, 70, 79, 70, 102, 126, 98, 35, 4,
			0, 4, 12, 15, 24, 102, 200, 102, 24, 15, 12, 4, 0,
			0, 0, 0, 1, 15, 126, 255, 126, 15, 1, 0, 0, 0,
			0, 0, 0, 0, 12, 98, 199, 98, 12, 0, 0, 0, 0,
			0, 0, 0, 0, 4, 35, 71, 35, 4, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 4, 8, 4, 0, 0, 0, 0, 0};
			u32 C = 0;
			for (int ky = 0; ky < 13; ++ky) {
				for (int kx = 0; kx < 13; ++kx) {
					if (convolution_kernel[ky*13+kx] == 0) continue;
					s32 px = pen_px_x+kx-6;
					s32 py = pen_px_y+ky-6;
					if ((px < 0) || (px >= 240) | (py < 0) || (py >= 160)) {
						C += convolution_kernel[ky*13+kx];
						continue;
					}
					u32 c = read_pixel(&screen_m3_surf, pen_px_x+kx-6, pen_px_y+ky-6);
					C += get_invert_table_bit(c) * convolution_kernel[ky*13+kx] * 2;
				}
			}
			pen_cursor_invert = (C > 6827) ? 0 : 1;
		}

		init_oam(oam_mem);
		REG_BLDCNT = BLD_OFF;
		REG_BLDY = 0;
		REG_WININ = WININ_BUILD(WIN_OBJ, 0);
		REG_WINOUT = WINOUT_BUILD(WIN_ALL|WIN_BLD, WIN_ALL|WIN_BLD);
		REG_WIN0H = (120<<8) | (120);
		REG_WIN0V = (80<<8) | (80);

		if (~keys_now_down & KEY_L) {
			obj_mem[1].attr0 = ((pen_px_y-4) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
			obj_mem[1].attr1 = ((pen_px_x-4) & 0x1ff) | ATTR1_SIZE_16x16;
			obj_mem[1].attr2 = ATTR2_BUILD(520, pen_cursor_invert, 0);

			if (pen_state == 2) {
				u32 pen_swatch_pin = 1;  // 0: top left, 1: top right ...
				if (pen_px_x < 240-48) {
					pen_swatch_pin |= 0b01;
				} else {
					pen_swatch_pin &= 0b10;
				}
				if (pen_px_y >= 46) {
					pen_swatch_pin &= 0b01;
				} else {
					pen_swatch_pin |= 0b10;
				}
				const int x_swatch_off[] = {-37,4,-37,4};
				const int y_swatch_off[] = {-35,-35,4,4};

				int i = get_invert_table_bit(pen_color) ? 0 : 1;
				obj_mem[2].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+25) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[2].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+9) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[2].attr2 = 528 + ((pen_color&0xf000)>>12);

				obj_mem[3].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+25) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[3].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+13) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[3].attr2 = 528 + ((pen_color&0x0f00)>>8);

				obj_mem[4].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+25) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[4].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+17) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[4].attr2 = 528 + ((pen_color&0x00f0)>>4);

				obj_mem[5].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+25) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[5].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+21) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[5].attr2 = 528 + (pen_color&0x000f);

				int x;
				x = (pen_color&0b0000000000011111);
				obj_mem[6].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+2) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[6].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]-1+x) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[6].attr2 = ATTR2_BUILD(560 + ((pen_select_pos!=0) ? 1 : 0), i, 0);

				x = (pen_color&0b0000001111100000) >> 5;
				obj_mem[7].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+9) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[7].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]-1+x) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[7].attr2 = ATTR2_BUILD(560 + ((pen_select_pos!=1) ? 1 : 0), i, 0);

				x = (pen_color&0b0111110000000000) >> 10;
				obj_mem[8].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+16) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[8].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]-1+x) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[8].attr2 = ATTR2_BUILD(560 + ((pen_select_pos!=2) ? 1 : 0), i, 0);

				obj_mem[9].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]-1) & 0xff) | ATTR0_8BPP | ATTR0_SQUARE;
				obj_mem[9].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+1) & 0x1ff) | ATTR1_SIZE_32x32;
				obj_mem[9].attr2 = 512;

				obj_mem[10].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]) & 0xff) | ATTR0_4BPP | ATTR0_WIDE;
				obj_mem[10].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]) & 0x1ff) | ATTR1_SIZE_32x64;
				obj_mem[10].attr2 = 640;
			} else if (pen_display_color_text) {
				u32 pen_swatch_pin = 1;  // 0: top left, 1: top right ...
				if (pen_px_x < 240-40) {
					pen_swatch_pin |= 0b01;
				} else {
					pen_swatch_pin &= 0b10;
				}
				if (pen_px_y >= 24) {
					pen_swatch_pin &= 0b01;
				} else {
					pen_swatch_pin |= 0b10;
				}
				const int x_swatch_off[] = {-29,4,-29,4};
				const int y_swatch_off[] = {-19,-19,-2,-2};

				obj_mem[2].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+9) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[2].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+9) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[2].attr2 = 528 + ((pen_color&0xf000)>>12);

				obj_mem[3].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+9) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[3].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+13) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[3].attr2 = 528 + ((pen_color&0x0f00)>>8);

				obj_mem[4].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+9) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[4].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+17) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[4].attr2 = 528 + ((pen_color&0x00f0)>>4);

				obj_mem[5].attr0 = ((pen_px_y+y_swatch_off[pen_swatch_pin]+9) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[5].attr1 = ((pen_px_x+x_swatch_off[pen_swatch_pin]+21) & 0x1ff) | ATTR1_SIZE_8x8;
				obj_mem[5].attr2 = 528 + (pen_color&0x000f);


				obj_mem[6].attr0 = ((pen_px_y + y_swatch_off[pen_swatch_pin]) & 0xff) | ATTR0_4BPP | ATTR0_WIDE;
				obj_mem[6].attr1 = ((pen_px_x + x_swatch_off[pen_swatch_pin]) & 0x1ff) | ATTR1_SIZE_16x32;
				obj_mem[6].attr2 = ATTR2_BUILD(586, 0, 0);
			} else {
				u32 pen_swatch_pin = 1;  // 0: top left, 1: top right ...
				if (pen_px_x < 240-24) {
					pen_swatch_pin |= 0b01;
				} else {
					pen_swatch_pin &= 0b10;
				}
				if (pen_px_y >= 24) {
					pen_swatch_pin &= 0b01;
				} else {
					pen_swatch_pin |= 0b10;
				}
				const int x_swatch_off[] = {-13,4,-13,4};
				const int y_swatch_off[] = {-19,-19,-2,-2};

				obj_mem[2].attr0 = ((pen_px_y + y_swatch_off[pen_swatch_pin]) & 0xff) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[2].attr1 = ((pen_px_x + x_swatch_off[pen_swatch_pin]) & 0x1ff) | ATTR1_SIZE_16x16;
				obj_mem[2].attr2 = ATTR2_BUILD(526, 0, 0);
			}
			if (old_pen_px_x != pen_px_x) pen_display_color_text = false;
			if (old_pen_px_y != pen_px_y) pen_display_color_text = false;


			u32 mag_pen_cursor_invert = 0;
			mag_pen_cursor_invert += get_invert_table_bit(read_pixel(&screen_m3_surf, pen_px_x, pen_px_y)) * 2;
			mag_pen_cursor_invert += (pen_px_x > 0) ? get_invert_table_bit(read_pixel(&screen_m3_surf, pen_px_x-1, pen_px_y)) * 2 : 1;
			mag_pen_cursor_invert += (pen_px_x < 240-1) ? get_invert_table_bit(read_pixel(&screen_m3_surf, pen_px_x+1, pen_px_y)) * 2 : 1;
			mag_pen_cursor_invert += (pen_px_y > 0) ? get_invert_table_bit(read_pixel(&screen_m3_surf, pen_px_x, pen_px_y-1)) * 2 : 1;
			mag_pen_cursor_invert += (pen_px_y < 160-1) ? get_invert_table_bit(read_pixel(&screen_m3_surf, pen_px_x, pen_px_y+1)) * 2 : 1;
			mag_pen_cursor_invert = (mag_pen_cursor_invert < 5) ? 1 : 0;
			
			u32 mag_mod6_x = (pen_x*6/65536)%6;
			u32 mag_mod6_y = (pen_y*6/65536)%6;

			obj_aff_mem[0].pa = 0x0080;
			obj_aff_mem[0].pd = 0x0080;
			if (pen_px_x < 120) {
				REG_WIN0H = (168<<8) | (168+64);
				REG_WIN0V = (88<<8) | (88+64);

				obj_mem[14].attr0 = (88+24) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[14].attr1 = (168+24) | ATTR1_SIZE_16x16;
				obj_mem[14].attr2 = ATTR2_BUILD(584, mag_pen_cursor_invert, 0);

				obj_mem[15].attr0 = (88) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[15].attr1 = (168) | ATTR1_SIZE_64x64;
				obj_mem[15].attr2 = ATTR2_BUILD(648, 0, 0);

				obj_mem[16].attr0 = ((88+2-mag_mod6_y)&0xff) | ATTR0_8BPP | ATTR0_AFF_DBL | ATTR0_SQUARE;
				obj_mem[16].attr1 = ((168+2-mag_mod6_x)&0x1ff) | ATTR1_SIZE_64x64;
				obj_mem[16].attr2 = ATTR2_BUILD(656, 0, 1);
			} else {
				REG_WIN0H = (8<<8) | (8+64);
				REG_WIN0V = (88<<8) | (88+64);

				obj_mem[14].attr0 = (88+24) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[14].attr1 = (8+24) | ATTR1_SIZE_16x16;
				obj_mem[14].attr2 = ATTR2_BUILD(584, mag_pen_cursor_invert, 0);

				obj_mem[15].attr0 = (88) | ATTR0_4BPP | ATTR0_SQUARE;
				obj_mem[15].attr1 = (8) | ATTR1_SIZE_64x64;
				obj_mem[15].attr2 = ATTR2_BUILD(648, 0, 0);

				obj_mem[16].attr0 = ((88+2-mag_mod6_y)&0xff) | ATTR0_8BPP | ATTR0_AFF_DBL | ATTR0_SQUARE;
				obj_mem[16].attr1 = ((8+2-mag_mod6_x)&0x1ff) | ATTR1_SIZE_64x64;
				obj_mem[16].attr2 = ATTR2_BUILD(656, 0, 1);
			}

			volatile COLOR* pal_ptr = &pal_obj_mem[128];
			for (int ky = 0; ky < 11; ++ky) {
				for (int kx = 0; kx < 11; ++kx) {
					s32 px = pen_px_x+kx-5;
					s32 py = pen_px_y+ky-5;
					if ((px < 0) || (px >= 240) | (py < 0) || (py >= 160)) {
						*pal_ptr = RGB15(4,4,4);
						++pal_ptr;
						continue;
					}
					*pal_ptr = read_pixel(&screen_m3_surf, px, py);
					++pal_ptr;
				}
			}

			set_main_cursor_palette(&pal_obj_bank[0][0], pen_color, pal_bg_bank[0][0]);
			set_color_picker_palette(&pal_obj_bank[2][0], pen_color);

		}

		if ((keys_now_down & KEY_L) && (keys_pressed & KEY_SELECT)) {
			init_oam(oam_mem);
			obj_mem[0].attr0 = (72 & 0xff) | ATTR0_4BPP | ATTR0_WIDE;
			obj_mem[0].attr1 = (104 & 0x1ff) | ATTR1_SIZE_16x32;
			obj_mem[0].attr2 = ATTR2_BUILD(832, 0, 0);
			REG_BLDCNT = BLD_BG2 | BLD_WHITE;
			REG_BLDY = 0xa;
			flash_load();
			memcpy(m3_mem, &save_data[SAVE_DATA_M3_OFFSET], M3_SIZE);
		}

		if ((keys_now_down & KEY_L) && (keys_pressed & KEY_START)) {
			init_oam(oam_mem);
			obj_mem[0].attr0 = (72 & 0xff) | ATTR0_4BPP | ATTR0_WIDE;
			obj_mem[0].attr1 = (104 & 0x1ff) | ATTR1_SIZE_16x32;
			obj_mem[0].attr2 = ATTR2_BUILD(768, 0, 0);
			REG_BLDCNT = BLD_BG2 | BLD_WHITE;
			REG_BLDY = 0xa;
			memcpy(&save_data[SAVE_DATA_M3_OFFSET], (const void*)m3_mem, M3_SIZE);
			flash_save();
		}

		if ((
			(reset_keys_up & (KEY_A | KEY_B | KEY_START | KEY_SELECT)) == 
			(KEY_A | KEY_B | KEY_START | KEY_SELECT)
		) && (
			(keys_now_down & (KEY_A | KEY_B | KEY_START | KEY_SELECT)) == 
			(KEY_A | KEY_B | KEY_START | KEY_SELECT)
		)) quit = true;
	}

	return 0;
}






