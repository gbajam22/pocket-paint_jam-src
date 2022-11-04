	.cpu      arm7tdmi
	.section  ".iwram", "ax", %progbits

	.func   isr_switchboard
	.global isr_switchboard
	.type   isr_switchboard, %function
	.extern __isr_table;          @ void (*isr_table[16])(void)
	.arm
.Lswitchboard_return:
	pop     {r0, r3, lr}        @ {spsr_irq mixed, lr_irq, lr_usr}
	msr     cpsr_c, #0x92       @ nesting off, Back in IRQ mode
	mov     lr, r3
	mov     r0, r0, ror#16
	msr     spsr_cf, r0         @ the cf flag specifier will mask out embedded irq index
	mov     r0, #0x04000000
	@ fallthrough to check for more interrupts,
	@ to reduce BIOS context switching overhead.
isr_switchboard:
@	mov     r0, #0x04000000     @ r0 already has REG_BASE from GBA BIOS
	ldr     r1, [r0, #0x0208]   @ check if IME is cleared (can happen in called code
	tst     r1, #1              @ or when "timing agrees"). ignore all interrupts if so.
	ldrne   r1, [r0, #0x0200]!  @ 0x04000200: IE, and 0x04000202: IF
	andnes  r1, r1, lsr #16     @ r1: IE & IF
	bxeq    lr                  @ exit if no more request flags to service (or ignored).
	rsb     r2, r1, #0x00
	and     r1, r2, r1          @ get lowest set bit with (x & -x)
	@ r0: REG_IE, r1: chosen irq bit
	strh    r1, [r0, #0x2]      @ Acknowledge IRQ
	ldr     r2, [r0, #-0x200-8] @ Acknowledge BIOS's copy of IRQ Flags
	orr     r2, r2, r1
	str     r2, [r0, #-0x200-8]!

	@ compute isr address.
	add     r3, r0, #8-0x140    @ r2: REG_BASE-0x140 aka irq_table
	@ compute log2(r1) for the table index via a 16 bit de-Bruijn string
	adr     r0, .Ldebruijn_tab
	@ Note: the de-Bruijn string overlaps nesting types by 3 bits,
	@ if the cart nesting type bit is 1 that'll make irq types
	@ 14, and 15 invalid (which is fine because those don't exist anyway)
	ldr     r2, =0x0f2d3f79     @ mixed de-Bruijn string and isr nesting types
	tst     r1, r2              @ ~0x3f79: hblank, vcount, serial
	mul     r2, r1, r2          @ wider bit pattren on Rs so to be const timed
	ldrb    r0, [r0, r2, lsr #28]
	ldr     r12, [r3, r0, lsl #2]
	@ r0: irq index, r1/r2: junk, r3: REG_BASE-0x140, r12: isr address
	@ nesting type bit == 0: on IRQ stack, returns to BIOS, non nesting, low overhead.
	bxeq    r12

	@ else nesting type bit == 1: prepare for nesting
	@ get system clock kept at timer 2 and 3
	add     r1, r3, #0x140+0x108    @ 0x04000108: Timer 2 and 3 Counters
	ldmia   r1, {r1, r2}        @ fetch hardware timers
	add     r1, #1              @ correct cycle diffrence between words
	bic     r1, #0x00ff0000
	orr     r1, r1, r2, lsl#16
	add     r3, r3, #(14*4)     @ 0x03fffef8: saved 64 bit timer
	ldr     r2, [r3, #0]
	cmp     r2, r1              @ if (prev 32 bits)-(current 32 bits) > 0
	ldr     r2, [r3, #4]
	adc     r2, r2, #0          @ add that carry bit to high 32-bits
	stmia   r3, {r1, r2}        @ save 64-bit timer for next check

	@ push registers.
	mrs     r3, spsr            @ ran out of room so stuff spsr fields
	orr     r0, r0, r3, ror#16  @ with irq index (8-bit)
	mov     r3, lr
	msr     cpsr_c, #0x1f       @ switch to user stack, nesting IRQ are now enabled
	push    {r0, r3, lr}        @ {spsr_irq mixed, lr_irq, lr_usr}
	and     r0, r0, #0x000000ff
	adr     lr, .Lswitchboard_return
	@ r0: irq index, r1/r2: 64-bit system time, r3: lr_irq/0x138, r12: isr address
	@ a check on r3 can distinguish between nesting and non nesting modes
	@ "if (r3 < 0x4000) then is in nesting mode"
	bx      r12
	@ in total about 5 μs (or 30% hblank) from hardware interrupt
	.align
.Ldebruijn_tab:
	.byte   0, 1, 8, 2, 14, 9, 11, 3, 15, 7, 13, 10, 6, 12, 5, 4
	.pool
	.size   isr_switchboard,.-.Lswitchboard_return
	.endfunc


	.func   get_system_clock
	.global get_system_clock
	.type   get_system_clock, %function
@ u64 get_system_clock();
	.arm
get_system_clock:
	mov     r12, #0x04000000
@	str     r12, [r12, #0x0208]

	add     r0, r12, #0x108     @ 0x04000108: Timer 2 and 3 Counters
	ldmia   r0, {r0, r1}        @ fetch hardware timers
	add     r0, #1              @ correct cycle diffrence between words
	bic     r0, #0x00ff0000
	orr     r0, r0, r1, lsl#16

	add     r3, r12, #-0x140+(14*4)     @ 0x03fffef8: saved 64 bit timer
	ldmia   r3, {r1, r2}
	cmp     r1, r0              @ if (prev 32 bits)-(current 32 bits) > 0
	adc     r1, r2, #0          @ add that carry bit to high 32-bits
	stmia   r3, {r0, r1}        @ save 64-bit timer for next check

@	mov     r2, #1
@	str     r12, [r2, #0x0208]
	bx      lr
	.size   get_system_clock,.-get_system_clock
	.endfunc



	.func   unlz4_vram
	.global unlz4_vram
	.type   unlz4_vram, %function
@ void unlz4_vram(void* dest, const void* src, size_t src_size);
@ GBA vram has a 8 bit read bus, but only a 16 bit write bus.
@ this routine works around that by keeping track of a halfword write buffer.
@ Use only for trusted input, as this will not check for invalid blocks
@ including things like a 0 byte src length, offsets of 0, read/write out of
@ buffer bounds.
	.arm
unlz4_vram:
	push    {r4-r7, lr}
	add     r2, r2, r1
	@ r0: dest, r1: src, r2: src end, r3: halfword write buffer
	@ r4: copy ptr, r5: copy len, r6: token, r7: lsb of dest (aligned flag)
	@ r12: read scratch
	mov     r3, #0
	ands    r7, r0, #1          @ if dest addr already misaligned
	ldrneb  r3, [r0, #-1]!      @ then read low byte, and round down
	b       .Lget_token

.Lmatch_offset:
	ldrb    r4, [r1], #1
	ldrb    r12, [r1], #1
	orr     r4, r4, r12, LSL #8 @ read match offset
	ands    r5, r6, #0x0f
	blne    .Lget_len
	add     r5, r5, #4          @ add the min length
	cmp     r4, #1              @ if offset == 1
	beq     .Lwrite_run         @ then do a run instead of a match copy
	sub     r4, r0, r4
	add     r4, r4, r7          @ copy ptr = dest + dest_flag - offset
	bl      .Lcopy_data
.Lget_token:
	ldrb    r6, [r1], #1
	lsrs    r5, r6, #4
	blne    .Lget_len           @ "not equal" will skip literals if token == 0
	mov     r4, r1              @ .Lget_len normaly exits with Z=0, but
	blne    .Lcopy_data         @ if it got skiped, then Z is left as 1
	mov     r1, r4
	cmp     r1, r2              @ check if at end of compressed stream
	blt     .Lmatch_offset
.Lend:
	tst     r7, #1              @ if a byte remains in buffer
	ldrneb  r12, [r0, #1]       @ then load the upper dest byte,
	orrne   r3, r3, r12, LSL #8
	strneh  r3, [r0], #1        @ store it, and point dest to byte end
	pop     {r4-r7}
	pop     {lr}
	bx      lr

.Lget_len:
	cmp     r5, #15             @ if len == 15, then add next byte
.Lextend_len_loop:
	bxne    lr                  @ done if len != 15 or 255.
	ldrb    r12, [r1], #1
	add     r5, r5, r12
	cmp     r12, #255           @ if new byte == 255, add more.
	b       .Lextend_len_loop

.Lcopy_data:
	ldrb    r12, [r4], #1       @ read from literals/match offset
	orr     r3, r3, r12, LSL #8 @ into the write buffer.
	eors    r7, r7, #1          @ add 1 to dest + dest_flag
	streqh  r3, [r0], #2        @ if aligned write halfword
	lsr     r3, r3, #8          @ make room for next byte read
	subs    r5, r5, #1
	bne     .Lcopy_data
	bx      lr

.Lwrite_run:
	orr     r12, r3, r3, LSL #8
.Lrun_loop:
	eors    r7, r7, #1
	streqh  r12, [r0], #2
	subs    r5, r5, #1
	bne     .Lrun_loop
	b       .Lget_token

	.size   unlz4_vram,.-unlz4_vram
	.endfunc


	.func   load_default_palette
	.global load_default_palette
	.type   load_default_palette, %function
@ void load_default_palette(u16* memory);
@ print("""GIMP Palette
@ Name: RGBI Arranged.
@ Columns: 16
@ #""")
@ for i in range(256):
@     print(((((i>>4)&0xc)+i)&0xf)*17, (i&0xf)*17, ((((i>>2)&0xc)+i)&0xf)*17)
	.arm
load_default_palette:
	mov     r12, #256*2
	ldr     r2, =0x781e63d8
	ldr     r3, =0x42107bde
1:
	subs    r12, #2             @ r12:                  .... ...7 6543 210.
	mov     r1, r12, lsl #26    @   4321 0... .... .... .... .... .... ....
	orr     r1, r12, lsr #4     @   4321 0... .... .... .... .... ...7 6543
	orr     r1, r1,  lsl #12    @   4321 0... .... ...7 6543 .... ...7 6543
	orr     r1, r1,  lsr #21    @   4321 0... .... ...7 6543 .432 10.7 6543
	orr     r1, r12, lsl #16    @   4321 0..7 6543 2107 6543 .432 10.7 6543
	and     r1, r2              @   .321 0... ...3 210. .54. ..32 10.7 6...
	add     r1, r1, ror #16     @ + .54. ..32 10.7 6... .321 0... ...3 210. 
	                            @   xBBB B.GG GGxR RRR. xBBB B.GG GGxR RRR.
	and     r1, r3              @   .b.. ..g. ...r .... .BBB B.GG GG.R RRR.
	orr     r1, r1, lsr #20     @                       .BBB BbGG GGgR RRRr
	strh    r1, [r0, r12]
	bge     1b                  @ flags set by "subs" way above
	bx      lr
	.pool
	.align
	.size   load_default_palette,.-load_default_palette
	.endfunc


	.func   load_default_8bit_bitmap
	.global load_default_8bit_bitmap
	.type   load_default_8bit_bitmap, %function
@ void load_default_8bit_bitmap(void* memory);
	.arm
load_default_8bit_bitmap:
	push    {r4-r5}
	mov     r12, #1
2:
	ldr     r1, =0x0c0c0c0c
	mov     r2, r1
	mov     r3, r1
	sub     r4, r1, #0x01000000
	tst     r12, #0x01
	subeq   r2, r2, #0x01000000
	subeq   r4, r4, #0x01000000
	tst     r12, #0x07
	bne     3f
	tst     r12, #0x0f
	ldreq   r1, =0x0a0b0a0b
	ldrne   r1, =0x0b0c0b0c
	mov     r2, r1
	mov     r3, r1
	moveq   r4, r1
	subne   r4, r1, #0x01000000
3:
@	mov     r5, #16
	mov     r5, #15
1:
	stmia   r0!, {r1-r4}
	subs    r5, #1
	bne     1b

	add     r12, #1
@	cmp     r12, #248
	cmp     r12, #160
	ble     2b

	pop     {r4-r5}
	bx      lr
	.pool
	.align

	.size   load_default_8bit_bitmap,.-load_default_8bit_bitmap
	.endfunc


	.func   load_default_16bit_bitmap
	.global load_default_16bit_bitmap
	.type   load_default_16bit_bitmap, %function
@ void load_default_16bit_bitmap(void* memory);
@ writes a particular sequence of 60 KiB that's a gray grid
	.arm
load_default_16bit_bitmap:
	push    {r4-r8, r10}
	mov     r12, #1
2:
	tst     r12, #0x07
	ldreq   r1, .Lgray_25_gray_23
	ldrne   r1, .Lgray_25_gray_25
	tst     r12, #0x0f
	ldreq   r1, .Lgray_23_gray_21
	mov     r2, r1
	mov     r3, r1
	mov     r4, r1
	mov     r5, r1
	mov     r6, r1
	mov     r7, r1
	mov     r8, r1
	beq     3f
	tst     r12, #0x01
	ldr     r8, .Lgray_25_gray_23
	moveq   r4, r8
	ldreq   r8, .Lgray_25_gray_21
3:
@	mov     r10, #12
	mov     r10, #15
1:
	stmia   r0!, {r1-r8}
	subs    r10, #1
	bne     1b

	add     r12, #1
	cmp     r12, #160
	ble     2b

	pop     {r4-r8, r10}
	bx      lr

.Lgray_25_gray_25:
	.word 0x67396739
.Lgray_25_gray_23:
	.word 0x5ef76739
.Lgray_25_gray_21:
	.word 0x56b56739
.Lgray_23_gray_21:
	.word 0x56b55ef7

	.size   load_default_16bit_bitmap,.-load_default_16bit_bitmap
	.endfunc


	.func   plot_pixel
	.global plot_pixel
	.type   plot_pixel, %function
@ struct pixel_plot_surface {
@   volatile void* buffer;
@   // bits are laied out all in little endian order. the top-left pixel is lsb.
@   u32 bpp;            // bits to move right 1 pixel, must be 1, 2, 4, 8, or 16
@   u32 pitch;          // bytes to move down 1 pixel
@   u32 width;          // in pixels. exceeding this counts as a tile movement
@   u32 height;         // ...
@   u32 tile_pitch_h;   // bytes to move right 1 tile
@   u32 tile_pitch_v;   // bytes to move down 1 tile
@   bool vram_bus;      // buffer is pointing to VRAM which can't take byte writes
@ };
@ void plot_pixel (const pixel_plot_surface* surface, u32 x, u32 y, u32 color);
	.arm
plot_pixel:
	push    {r4-r10, lr}
	ldmia   r0, {r4-r10, r12}   @ r4: buffer / offset
	cmp     r5, #8
	movhi   r12, #1             @ bool write_halfword = (vram_bus || (bpp > 8))
	mov     r0, r1
	movs    r1, r7              @ flag for next branch
	mov     r7, r2
	push    {r3, r12}
	moveq   r1, r0              @ if (width == 0) skip divmod, and just use x
	beq     1f
	bl      __aeabi_uidivmod
	mla     r4, r0, r9, r4      @ buf += (x / tile_width) * tile_pitch_h
1:
	mul     r2, r1, r5
	and     r9, r2, #7          @ offset_bits = ((x % tile_width) * bpp) % 8
	add     r4, r4, r2, lsr#3   @ buf += ((x % tile_width) * bpp) >> 3
	mov     r0, r7
	movs    r1, r8
	moveq   r1, r0              @ if (height == 0) skip divmod, and just use y
	beq     1f
	bl      __aeabi_uidivmod
	mla     r4, r0, r10, r4     @ buf += (y / tile_height) * tile_pitch_v
1:
	mla     r4, r1, r6, r4      @ buf += (y % tile_height) * pitch
	pop     {r3, r12}
	cmp     r12, #0
	andne   r0, r4, #1
	orrne   r9, r9, r0, lsl#3   @ offset_bits |= (offset & 1) << 3;
	eorne   r4, r4, r0          @ offset &= ~1;
	ldreqb  r0, [r4]
	ldrneh  r0, [r4]
	mov     r1, #1
    rsb     r1, r1, r1, lsl r5  @ bitmask = (1 << bpp)-1
    bic     r0, r0, r1, lsl r9
    and     r3, r3, r1
    orr     r0, r0, r3, lsl r9
	streqb  r0, [r4]
	strneh  r0, [r4]
	pop     {r4-r10, lr}
	bx      lr
	.size   plot_pixel,.-plot_pixel
	.endfunc


	.func   read_pixel
	.global read_pixel
	.type   read_pixel, %function
@ u32 read_pixel (const pixel_plot_surface* surface, u32 x, u32 y);
	.arm
read_pixel:
	push    {r4-r10, lr}
	ldmia   r0, {r4-r10, r12}   @ r4: buffer / offset
	cmp     r5, #8
	movhi   r12, #1             @ bool write_halfword = (vram_bus || (bpp > 8))
	mov     r0, r1
	movs    r1, r7              @ flag for next branch
	mov     r7, r2
	push    {r12}
	moveq   r1, r0              @ if (width == 0) skip divmod, and just use x
	beq     1f
	bl      __aeabi_uidivmod
	mla     r4, r0, r9, r4      @ buf += (x / tile_width) * tile_pitch_h
1:
	mul     r2, r1, r5
	and     r9, r2, #7          @ offset_bits = ((x % tile_width) * bpp) % 8
	add     r4, r4, r2, lsr#3   @ buf += ((x % tile_width) * bpp) >> 3
	mov     r0, r7
	movs    r1, r8
	moveq   r1, r0              @ if (height == 0) skip divmod, and just use y
	beq     1f
	bl      __aeabi_uidivmod
	mla     r4, r0, r10, r4     @ buf += (y / tile_height) * tile_pitch_v
1:
	mla     r4, r1, r6, r4      @ buf += (y % tile_height) * pitch
	pop     {r12}
	cmp     r12, #0
	andne   r0, r4, #1
	orrne   r9, r9, r0, lsl#3   @ offset_bits |= (offset & 1) << 3;
	eorne   r4, r4, r0          @ offset &= ~1;
	ldreqb  r0, [r4]
	ldrneh  r0, [r4]			@ val = (vram_bus) ? (u16*)*buf : (u8*)*buf;
	mov     r1, #1
    rsb     r1, r1, r1, lsl r5  @ bitmask = (1 << bpp)-1
	and     r0, r1, r0, lsr r9  @ return (val >> offset_bits) & ((1 << bpp)-1)
	pop     {r4-r10, lr}
	bx      lr
	.size   read_pixel,.-read_pixel
	.endfunc


	.func   log2
	.global log2
	.type   log2, %function
@ u32 log2(u32 val);
	.thumb
log2:
	neg     r1, r0
	beq     .Lreturn_nan
	and     r0, r1
	ldr     r1, =0x077CB531
	mul     r0, r1
	lsr     r0, #27
	adr     r1, .Llog2_debruijn_table
	ldrb    r0, [r1, r0]
	bx      lr          @ return table[((x & -x) * 0x077CB531) >> 27]
.Lreturn_nan:
	sub     r0, #1      @ return -1
	bx      lr
	.align
.Llog2_debruijn_table:
	.byte   0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8
	.byte   31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	.pool
	.size   log2,.-log2
	.endfunc

	.end
