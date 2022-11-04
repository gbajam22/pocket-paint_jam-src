	.cpu      arm7tdmi
	.section  ".crt0", "ax", %progbits
	.global   _start
_start:
cart_vector:
	.arm
	b       joybus_vector
	.fill 188                   @ gba header to be fixed up by gbafix
multiboot_vector:
	.arm
	b       joybus_vector
cart_gpio:
	.fill   6                   @ I/O Port for RTC etc
	.fill   6                   @ pad to 0x080000d0
	.ascii  "FLASH1M_Vnnn\0\0\0\0"      @ save memory magic string
joybus_vector:
	.arm
	@ from here the enviorment is assumed to be either from the GBA BIOS logo
	@ or from the BIOS calls from this program's soft_reset
	@ both of which calls RegisterRamReset that disables IRQs from their sources.
	@ sets the stack pointers to the defaults, and starts with the cpu IRQ bit to allow.

	@ switch to thumb first, as we don't need the arm only
	@ msr instruction for swi and irq stack setting
	adr     r3, .Lswitch_to_thumb + 1
	bx      r3
.Lswitch_to_thumb:
	.thumb
	@ Turn on IME, acknowledge and *enable* all interrupts,
	@ and set standard cart wait states all in one go.
	@ All source IRQs are assumed off due to RegisterRamReset
	adr     r0, .Lsys_init_magic
	ldmia   r0!, {r4, r5, r6, r7}
	stmia   r4!, {r5, r6, r7}
	@ start timer 2 at system clock rate and cascade to timer 3, no irq
	ldmia   r0!, {r5, r6, r7}
	stmia   r5!, {r6, r7}

	@ Relocate to WRAM
	lsr     r3, #27             @ if boot pc < 0x08000000
	beq     init                @ skip copying self, already at WRAM
	@ r3 is left as 1 from shift above
	lsl     r0, r3, #25         @ dest: 0x02000000
	lsl     r1, r3, #27         @ src:  0x08000000
	lsl     r2, r3, #14         @ num:  16 KiB
	ldr     r3, .Lmemcpy_in_rom_addr
	bl      blx_r3_stub
	ldr     r3, .Linit_in_wram_addr
blx_r3_stub:
	bx      r3
	.align
.Lsys_init_magic:
	.word   0x04000200, 0x3fff3fff, 0x00004317, 0x00000001
.Ltimers_begin:
	.word   0x04000108, 0x00800000, 0x00840000
.Lmemcpy_in_rom_addr:
	.word   __iwram_lma - 0x02000000 + 0x08000000 + 4
.Linit_in_wram_addr:
	.word   0x02000000 + init - _start + 1


init:
	adr     r4, .Lmemory_init_routines
	mov     r5, #4
1:
	ldmia   r4!, {r0, r1, r2, r3}
	bl      blx_r3_stub
	sub     r5, #1
	bne     1b

	@ place 0x01 at SoftReset Re-entry Flag (via a mirror at 0x03FFFFFA)
	@ to make it boot to 0x02000000 by default.
	mov     r0, #1
	lsl     r1, r0, #26
	sub     r1, #6              @ r1 = 0x03FFFFFA
	strb    r0, [r1]

	mov     r0, #7
	lsl     r0, #24
	bl      init_oam

	@ init addresss of all isr_switchboard slots to do nothing.
	mov     r0, #1
	lsl     r1, r0, #26
	sub     r1, #4              @ r1 = 0x03FFFFFC
	ldr     r0, =isr_switchboard
	str     r0, [r1]

	mov     r0, #0              @ also takes care of argc and argv
	mov     r1, #0
	ldr     r2, =nop_subroutine
	mov     r3, sp
	sub     r3, #64             @ __isr_table is at the bottom of the stack
	push    {r0, r1}            @ init save spot for ISR system timer
1:
	push    {r2}
	cmp     r3, sp
	blt     1b

	@ begin main
@	mov     r0, #0              @ int argc
@	mov     r1, #0              @ char *argv[]
	mov     r2, #0              @ and clear out register garbage
	ldr     r3, =main
@   mov     r12, #0             @ r12 is 0 from memcpy
	bl      blx_r3_stub
	b exit

	.align
.Lmemory_init_routines:
	@ copy iram routines, but with a iram routine located in wram
	.word   __iwram_start__, __iwram_lma, __iwram_size__, __iwram_lma + 4
	.word   __bss_start__, 0, __bss_size__, memset
	.word   __sbss_start__, 0, __sbss_size__, memset
	.word   __data_start__, __data_lma, __data_size__, memcpy
	.pool

	.global exit
	.type exit, %function
exit:
	tst     r0, r0
	bne     soft_reset          @ just do soft reset if return != 0

	bl      cart_is_present
	beq     soft_reset          @ soft reset if cart header is open bus.

exit_to_flash_cart:
	@ Disable interrupts and DMA before doing cart pokes
	mov     r0, #0xE0           @ Reset just the Registers, (includes DMA and IME)
	swi     #0x01

	@ Attempt to exit to flash cart.
	@ magic poke lists thanks to Dwedit, from file visoly.s of PocketNES
	@ https://www.dwedit.org/gba/pocketnes.php
	mov     r7, #0
	sub     r7, #2              @ r7: -2 so that ldsh r3 will be pre-inc
	adr     r6, .Lwrite_list_rle_ctrl
	adr     r5, .Lwrite_list_addresses
	adr     r4, .Lwrite_list_data
.Lnext:
	add     r6, #2
	ldsh    r3, [r6, r7]        @ read rle_ctrl
	asr     r2, r3, #8          @ r2: repeat count
	bpl     1f
	lsl     r2, #1
	sub     r2, #11             @ 499 (assuming 0xff was read)
1:
	lsl     r3, r3, #24         @ r3: read/write mode and pair count
	asr     r3, r3, #24
	bmi     .Lwrite
	beq     .Ldone

.Lread:
	neg     r3, r3
0:
	ldr     r0, [r5]            @ addresses
	add     r5, #4
1:
	ldrh    r1, [r0]            @ do flash cart read
	add     r3, #1
	bmi     0b
	sub     r2, #1
	bpl     1b
	b       .Lnext

.Lwrite:
0:
	ldr     r0, [r5]            @ addresses
	add     r5, #4
	ldrh    r1, [r4]            @ data
	add     r4, #2
1:
	strh    r1, [r0]            @ do flash cart write
	add     r3, #1
	bmi     0b
	sub     r2, #1
	bpl     1b                  @ if applicable, repeat last write
	b       .Lnext

.Ldone:
	@ direct BIOS SoftReset to goto 0x08000000
	mov     r0, #1
	lsl     r0, r0, #26
	sub     r0, #6              @ r0 = 0x03FFFFFA
	strb    r3, [r0]            @ r3 has 0x00 from "beq .Ldone" above
soft_reset:
	@ flag at 0x03007FFA is assumed set
	mov     r0, #0xfe           @ clear all but WRAM
	swi     #0x01               @ BIOS RegisterRamReset
	mov     r0, #0x01           @ SoundBiasChange: bias of 0x200
	swi     #0x19               @ BIOS SoundBiasChange
	swi     #0x00               @ BIOS SoftReset

	.align
.Lwrite_list_rle_ctrl:
	@ low signed byte negitive is to write that many address/data pairs
	@ low signed byte positive is to read that many address
	@ high byte repeats the last pair that many times, >127 = 499
	@ 0 ends the list
	.hword  (256-7) | 1<<8
	.hword  (256-1) | 1<<8
	.hword  (256-2) | 255<<8
	.hword  (256-2) | 255<<8
	.hword  (256-5) | 255<<8
	.hword  (256-2) | 0<<8
	@ read list
	.hword  11 | 1<<8
	.hword  1 | 1<<8
	.hword  3 | 2<<8
	.hword  1 | 2<<8
	.hword  1 | 2<<8
	.hword  3 | 0<<8
	.hword  0

	.align
.Lwrite_list_addresses:
	@ ez flash
	.word   0x09FE0000
	.word   0x08000000
	.word   0x08020000
	.word   0x08040000
	.word   0x09880000
	.word   0x09FC0000

	@ SC
	.word   0x09FFFFFE
	.word   0x09FFFFFE

	@ Visoly
	.word   (0x00987654 * 2) | 0x08000000
	.word   (0x00012345 * 2) | 0x08000000
	.word   (0x00012345 * 2) | 0x08000000
	.word   (0x00012345 * 2) | 0x08000000
	.word   (0x00987654 * 2) | 0x08000000
	.word   (0x00012345 * 2) | 0x08000000
	.word   (0x00765400 * 2) | 0x08000000
	.word   (0x00013450 * 2) | 0x08000000
	.word   (0x00012345 * 2) | 0x08000000
	.word   (0x00987654 * 2) | 0x08000000
	.word   0x096B592E

	@ M3 (reads)
	.word   0x08E00002
	.word   0x0800000E
	.word   0x08801FFC
	.word   0x0800104A
	.word   0x08800612
	.word   0x08000000
	.word   0x08801B66
	.word   0x08800008
	.word   0x0800080E
	.word   0x08000000
	.word   0x080001E4
	.word   0x08000188
	
	@G6 (reads)
	.word   0x09000000
	.word   0x09FFFFE0
	.word   0x09FFFFEC
	.word   0x09FFFFFC
	.word   0x09FFFF4A
	.word   0x09200000
	.word   0x09FFFFF0
	.word   0x09FFFFE8

	.align
.Lwrite_list_data:
	@ ez_flash
	.hword  0xD200
	.hword  0x1500
	.hword  0xD200
	.hword  0x1500
	.hword  0x8000
	.hword  0x1500

	@ SC
	.hword  0xA55A
	.hword  0

	@ visoly
	.hword  0x5354
	.hword  0x1234
	.hword  0x5354
	.hword  0x5678
	.hword  0x5354
	.hword  0x5354
	.hword  0x5678
	.hword  0x1234
	.hword  0xABCD
	.hword  0x5354
	.hword  0

	.global cart_is_present
	.type cart_is_present, %function
@ bool cart_is_present();
	.thumb
cart_is_present:
	@ checking the part of the cart header that's a fixed value of 0x0096.
	mov     r1, #1
	lsl     r1, #27
	add     r1, #0xb2           @ 0x080000b2
	ldrh    r0, [r1]
	sub     r0, #0x96
	neg     r1, r0
	adc     r0, r1              @ 0 = true (1), else false (0)
	bx      lr

	.global init_oam
	.type   init_oam, %function
@ void init_oam(void* oam_mem);
	.thumb
init_oam:
	push    {r4, r5}
	mov     r1, #128
	ldr     r2, .Loam_default_val
	mov     r3, #1
	lsl     r3, #24             @ identity matrix dx
	mov     r4, r2
	mov     r5, #0x00000000     @ identity matrix dmx
0:
	stmia   r0!, {r2-r5}
	eor     r3, r5
	eor     r5, r3
	eor     r3, r5              @ identity matrix xor swap to dy, dmy
	sub     r1, #2
	bne     0b
	pop     {r4, r5}
	bx      lr
	.align
.Loam_default_val:
	.word 0x018002e4            @ X: -128, Y: 228, 8x8 no scale, OBJ Disabled
	
	.pool


@------------------------------------------------------------------------------
	.section ".iwram", "ax", %progbits
	.arm

	.global nop_subroutine
	.type nop_subroutine, %function
nop_subroutine:
	bx      lr


	.global memcpy
	.type memcpy, %function
memcpy:
	.global memmove
	.type memmove, %function
memmove:
	push    {r0, lr}
	bl      __aeabi_memcpy
	pop     {r0, lr}
	bx      lr

	.global __aeabi_memmove
	.type __aeabi_memmove, %function
__aeabi_memmove:
	.global __aeabi_memmove8
	.type __aeabi_memmove8, %function
__aeabi_memmove8:
	.global __aeabi_memmove4
	.type __aeabi_memmove4, %function
__aeabi_memmove4:
	subs    r3, r0, r1
	subgts  r3, r2, r3          @ if src < dst < src+len
	bgt     .Lcopy_bytes_reverse

	.global __aeabi_memcpy
	.type __aeabi_memcpy, %function
__aeabi_memcpy:
	eor     r3, r0, r1
	movs    r3, r3, lsl #31
	bmi     .Lcopy_bytes
	bcs     .Lcopy_halves

	@ Check if r0 (or r1) needs word aligning
	rsbs    r3, r0, #4
	movs    r3, r3, lsl #31

	@ Copy byte head to align
	ldrmib  r3, [r1], #1
	strmib  r3, [r0], #1
	submi   r2, r2, #1
	@ r0, r1 are now half aligned

	@ Copy half head to align
	ldrcsh  r3, [r1], #2
	strcsh  r3, [r0], #2
	subcs   r2, r2, #2
	@ r0, r1 are now word aligned

	.global __aeabi_memcpy8
	.type __aeabi_memcpy8, %function
__aeabi_memcpy8:
	.global __aeabi_memcpy4
	.type __aeabi_memcpy4, %function
__aeabi_memcpy4:
	cmp     r2, #32
	blt     .Lcopy_words

	@ Word aligned, 32-byte copy
	push    {r4-r10, r12}       @ r12 for alignment
.Lloop_32:
	subs    r2, r2, #32
	ldmgeia r1!, {r3-r10}
	stmgeia r0!, {r3-r10}
	bgt     .Lloop_32
	pop     {r4-r10, r12}       @ r12 for alignment
	bxeq    lr

	@ < 32 bytes remaining to be copied
	add     r2, r2, #32

.Lcopy_words:
	cmp     r2, #4
	blt     .Lcopy_halves
.Lloop_4:
	subs    r2, r2, #4
	ldrge   r3, [r1], #4
	strge   r3, [r0], #4
	bgt     .Lloop_4
	bxeq    lr

	@ Copy byte & half tail
	@ This test still works when r2 is negative
	movs    r2, r2, lsl #31
	@ Copy half
	ldrcsh  r3, [r1], #2
	strcsh  r3, [r0], #2
	@ Copy byte
	ldrmib  r3, [r1]
	strmib  r3, [r0]
	bx      lr

.Lcopy_halves:
	@ Copy byte head to align
	tst     r0, #1
	ldrneb  r3, [r1], #1
	strneb  r3, [r0], #1
	subne   r2, r2, #1
	@ r0, r1 are now half aligned

.Lloop_2:
	subs    r2, r2, #2
	ldrgeh  r3, [r1], #2
	strgeh  r3, [r0], #2
	bgt     .Lloop_2
	bxeq    lr

	@ Copy byte tail
	adds    r2, r2, #2
	ldrneb  r3, [r1]
	strneb  r3, [r0]
	bx      lr

.Lcopy_bytes:
	subs    r2, r2, #1
	ldrgeb  r3, [r1], #1
	strgeb  r3, [r0], #1
	bgt     .Lcopy_bytes
	bx      lr

.Lcopy_bytes_reverse:
	subs    r2, r2, #1
	ldrgeb  r3, [r1, r2]
	strgeb  r3, [r0, r2]
	bgt     .Lcopy_bytes_reverse
	bx      lr


	.global memset
	.type memset, %function
memset:
	mov     r3, r1
	mov     r1, r2
	mov     r2, r3
	push    {r0, lr}
	bl      __aeabi_memset
	pop     {r0, lr}
	bx      lr

	.global __aeabi_memclr8
	.type __aeabi_memclr8, %function
__aeabi_memclr8:
	.global __aeabi_memclr4
	.type __aeabi_memclr4, %function
__aeabi_memclr4:
	.global __aeabi_memclr
	.type __aeabi_memclr, %function
__aeabi_memclr:
	mov     r2, #0

	.global __aeabi_memset8
	.type __aeabi_memset8, %function
__aeabi_memset8:
	.global __aeabi_memset4
	.type __aeabi_memset4, %function
__aeabi_memset4:
	.global __aeabi_memset
	.type __aeabi_memset, %function
__aeabi_memset:
	and     r2, r2, #0xff
	orr     r2, r2, r2, lsl #16
	orr     r2, r2, r2, lsl #8

.Lclear_align_head:
	rsbs    r3, r0, #4          @ ((4-r0) mod 4) will be bytes needed to align
	movs    r3, r3, lsl #31     @ bit 1 -> C, bit 0 -> N
	strmib  r2, [r0], #1
	submi   r1, r1, #1
	strcsh  r2, [r0], #2
	subcs   r1, r1, #2

	cmp     r1, #32
	blt     .Lclear_words

	push    {r4-r8}
	mov     r3, r2
	mov     r4, r2
	mov     r5, r2
	mov     r6, r2
	mov     r7, r2
	mov     r8, r2
	mov     r12, r2

.Lclear_loop_32:
	stmia   r0!, {r2-r8, r12}
	subs    r1, r1, #32
	bgt     .Lclear_loop_32
	pop     {r4-r8}
	bxeq    lr

.Lclear_words:
	cmp     r1, #4
	blt     .Lclear_align_tail

.Lclear_loop_4:
	str     r2, [r0], #4
	subs    r1, r1, #4
	bgt     .Lclear_loop_4
	bxeq    lr

.Lclear_align_tail:
	movs    r1, r1, lsl #31
	strcsh  r2, [r0], #2
	strmib  r2, [r0], #1
	bx      lr


	.global __aeabi_lmul
	.type __aeabi_lmul, %function
__aeabi_lmul:
	mul     r3, r0, r3
	mla     r1, r2, r1, r3
	umull   r0, r3, r2, r0
	add     r1, r1, r3
	bx      lr

	.global __aeabi_llsl
	.type __aeabi_llsl, %function
__aeabi_llsl:
	subs    r3, r2, #32
	rsb     r12, r2, #32
	lslmi   r1, r1, r2
	lslpl   r1, r0, r3
	orrmi   r1, r1, r0, lsr r12
	lsl     r0, r0, r2
	bx      lr

	.global __aeabi_llsr
	.type __aeabi_llsr, %function
__aeabi_llsr:
	subs    r3, r2, #32
	rsb     r12, r2, #32
	lsrmi   r0, r0, r2
	lsrpl   r0, r1, r3
	orrmi   r0, r0, r1, lsl r12
	lsr     r1, r1, r2
	bx      lr

	.global __aeabi_lasr
	.type __aeabi_lasr, %function
__aeabi_lasr:
	subs    r3, r2, #32
	rsb     r12, r2, #32
	lsrmi   r0, r0, r2
	asrpl   r0, r1, r3
	orrmi   r0, r0, r1, lsl r12
	asr     r1, r1, r2
	bx      lr


	.global __aeabi_uidivmod
	.type __aeabi_uidivmod, %function
__aeabi_uidivmod:
	.global __aeabi_uidiv
	.type __aeabi_uidiv, %function
__aeabi_uidiv:
	@ Check for division by zero
	cmp     r1, #0
	beq     __aeabi_idiv0

.Luidiv:
@ core division code from
@ https://www.chiark.greenend.org.uk/~theom/riscos/docs/ultimate/a252div.txt
@ parameters r0: numerator / r1: denominator
@ returns r0: quotient, r1: modulo
	@ If n < d, just bail out as well
	cmp     r0, r1              @ n, d
	movlo   r1, r0              @ mod = n
	movlo   r0, #0              @ quot = 0
	bxlo    lr

	@ Move the denominator to r2 and start to build a counter that
	@ counts the difference on the number of bits on each numerator
	@ and denominator
	@ From now on: r0 = quot/num, r1 = mod, r2 = denom, r3 = counter
	mov     r2, r1
	mov     r3, #28             @ first guess on difference
	mov     r1, r0, lsr #4      @ r1 = num >> 4

	@ Iterate three times to get the counter up to 4-bit precision
	cmp     r2, r1, lsr #12
	suble   r3, r3, #16
	movle   r1, r1, lsr #16

	cmp     r2, r1, lsr #4
	suble   r3, r3, #8
	movle   r1, r1, lsr #8

	cmp     r2, r1
	suble   r3, r3, #4
	movle   r1, r1, lsr #4

	@ shift the numerator by the counter and flip the sign of the denom
	mov     r0, r0, lsl r3
	adds    r0, r0, r0
	rsb     r2, r2, #0

	@ dynamically jump to the exact copy of the iteration
	add     r3, r3, r3, lsl #1  @ counter *= 3
	add     pc, pc, r3, lsl #2  @ jump
	mov     r0, r0              @ pipelining issues

	@ here, r0 = num << (r3 + 1), r1 = num >> (32-r3), r2 = -denom
	@ now, the real iteration part
	.rept 32
	adcs    r1, r2, r1, lsl #1
	sublo   r1, r1, r2
	adcs    r0, r0, r0
	.endr

	@ and then finally quit
	@ r0 = quotient, r1 = remainder
	bx      lr

@ r0: the numerator / r1: the denominator
@ after it, r0 has the quotient and r1 has the modulo
	.global __aeabi_idivmod
	.type __aeabi_idivmod, %function
__aeabi_idivmod:
	.global __aeabi_idiv
	.type __aeabi_idiv, %function
__aeabi_idiv:
	@ Test division by zero
	cmp     r1, #0
	beq     __aeabi_idiv0

	mov     r12, #0
	@ r12 bit 30 is whether the numerator is negative
	cmp     r0, #0
	rsblt   r0, #0
	orrlt   r12, #1 << 30

	@ r12 bit 31 is whether the denominator is negative
	cmp     r1, #0
	rsblt   r1, #0
	orrlt   r12, #1 << 31

	@ Call the unsigned division
	push    {lr}
	bl      .Luidiv
	pop     {lr}

	@ This moves "numerator is negative" to sign flag and
	@ "denominator is negative" to carry flag
	movs    r12, r12, lsl #1

	@ If numerator was negative (sign flag is set), negate both quotient and modulo
	rsbmi   r0, r0, #0
	rsbmi   r1, r1, #0

	@ If denominator was negative (carry flag is set), negate quotient
	rsbcs   r0, r0, #0

	bx      lr


@ routines missing:
@ __aeabi_uldiv, __aeabi_uldivmod,
@ __aeabi_ldiv, __aeabi_ldivmod
@ __aeabi_lcmp, __aeabi_ulcmp
@ __aeabi_uread4, __aeabi_uwrite4, __aeabi_uread8, __aeabi_uwrite8


	.global __aeabi_idiv0
	.type __aeabi_idiv0, %function
__aeabi_idiv0:
	@ log location of div-by-0 to unused BIOS memory for debuging
	mov     r1, #0x04000000
	str     lr, [r1, #-12]
	@ and return {0, numerator}
	mov     r1, r0
	mov     r0, #0
	bx      lr

	.end

