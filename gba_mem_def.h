#ifndef GBA_MEM_DEF_H
#define GBA_MEM_DEF_H

/* --------------------------------------------------------------
  Names and typedefs for working with the GBA harware,
  All ripped from libtonc: https://www.coranac.com/tonc/text/toc.htm
----------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------
  GCC Section attributes
----------------------------------------------------------------- */

#define IWRAM_DATA      __attribute__((section(".iwram")))
#define EWRAM_DATA      __attribute__((section(".ewram")))
#define EWRAM_BSS       __attribute__((section(".sbss")))
#define IWRAM_CODE      __attribute__((section(".iwram"), long_call))
#define EWRAM_CODE      __attribute__((section(".ewram"), long_call))
#define ALIGN(n)        __attribute__((aligned(n)))
#define ALIGN4          __attribute__((aligned(4)))
#define PACKED          __attribute__((packed))
#define DEPRECATED      __attribute__((deprecated))
#define INLINE          static inline

/* --------------------------------------------------------------
  Primary typedefs
----------------------------------------------------------------- */

typedef unsigned char   u8,  byte, uchar, echar;
typedef unsigned short  u16, hword, ushort, eshort;
typedef unsigned int    u32, word, uint, eint;
typedef unsigned long long  u64;

typedef signed char     s8;
typedef signed short    s16;
typedef signed int      s32;
typedef signed long long    s64;

typedef volatile u8     vu8;
typedef volatile u16    vu16;
typedef volatile u32    vu32;
typedef volatile u64    vu64;

typedef volatile s8     vs8;
typedef volatile s16    vs16;
typedef volatile s32    vs32;
typedef volatile s64    vs64;

typedef const u8        cu8;
typedef const u16       cu16;
typedef const u32       cu32;
typedef const u64       cu64;

typedef const s8        cs8;
typedef const s16       cs16;
typedef const s32       cs32;
typedef const s64       cs64;

typedef struct { u32 data[8]; }     BLOCK;

typedef const char * const  CSTR;

#ifndef __cplusplus
#ifndef bool
typedef enum { false, true } bool;
#endif
#endif

#ifndef BOOL
typedef u8 BOOL;                // C++ bool == u8 too, that's why
#define TRUE 1
#define FALSE 0
#endif

typedef void (*fnptr)(void);    // void foo() function pointer
typedef void (*fn_v_i)(int);    // void foo(int x) function pointer
typedef int (*fn_i_i)(int);     // int foo(int x) function pointer

#ifndef NULL
#define NULL (void*)0
#endif

/* --------------------------------------------------------------
  Macros for bitfields
----------------------------------------------------------------- */
#define BIT(n)                  ( 1<<(n) )
#define BIT_SHIFT(a, n)         ( (a)<<(n) )
#define BIT_MASK(len)           ( BIT(len)-1 )
#define BIT_SET(y, flag)        ( y |=  (flag) )
#define BIT_CLEAR(y, flag)      ( y &= ~(flag) )
#define BIT_FLIP(y, flag)       ( y ^=  (flag) )
#define BIT_EQ(y, flag)         ( ((y)&(flag)) == (flag) )

#define BF_MASK(shift, len)     ( BIT_MASK(len)<<(shift) )
#define _BF_GET(y, shift, len)  ( ((y)>>(shift))&BIT_MASK(len) )
#define _BF_PREP(x, shift, len) ( ((x)&BIT_MASK(len))<<(shift) )
#define _BF_SET(y, x, shift, len) \
	( y= ((y) &~ BF_MASK(shift, len)) | _BF_PREP(x, shift, len) )

INLINE u32 bf_get(u32 y, uint shift, uint len);
INLINE u32 bf_get(u32 y, uint shift, uint len)
{
	return (y>>shift) & ( (1<<len)-1 );
}

INLINE u32 bf_merge(u32 y, u32 x, uint shift, uint len);
INLINE u32 bf_merge(u32 y, u32 x, uint shift, uint len)
{
	u32 mask= ((u32)(1<<len)-1);
	return (y &~ (mask<<shift)) | (x & mask)<<shift;
}

INLINE u32 bf_clamp(int x, uint len);
INLINE u32 bf_clamp(int x, uint len)
{
	u32 y=x>>len;
	if(y)
		x= (~y)>>(32-len);
	return x;
}

INLINE int bit_tribool(u32 flags, uint plus, uint minus);
INLINE int bit_tribool(u32 flags, uint plus, uint minus)
{
	return ((flags>>plus)&1) - ((flags>>minus)&1);
}

INLINE u32 ROR(u32 x, uint ror);
INLINE u32 ROR(u32 x, uint ror)
{
	return (x<<(32-ror)) | (x>>ror);
}

INLINE uint align(uint x, uint width);
INLINE uint align(uint x, uint width)
{
	return (x+width-1)/width*width;
}

INLINE u16 dup8(u8 x);
INLINE u16 dup8(u8 x)
{
	return x|(x<<8);
}

INLINE u32 dup16(u16 x);
INLINE u32 dup16(u16 x)
{
	return x|(x<<16);
}

INLINE u32 quad8(u8 x);
INLINE u32 quad8(u8 x)
{
	return x*0x01010101;
}

INLINE u32 octup(u8 x);
INLINE u32 octup(u8 x)
{
	return x*0x11111111;
}

INLINE u16 bytes2hword(u8 b0, u8 b1);
INLINE u16 bytes2hword(u8 b0, u8 b1)
{
	return b0 | b1<<8;
}

INLINE u32 bytes2word(u8 b0, u8 b1, u8 b2, u8 b3);
INLINE u32 bytes2word(u8 b0, u8 b1, u8 b2, u8 b3)
{
	return b0 | b1<<8 | b2<<16 | b3<<24;
}

INLINE u32 hword2word(u16 h0, u16 h1);
INLINE u32 hword2word(u16 h0, u16 h1)
{
	return h0 | h1<<16;
}

#define countof(_array)     ( sizeof(_array)/sizeof(_array[0]) )

/* --------------------------------------------------------------
  GBA Specific typedefs
----------------------------------------------------------------- */

typedef s32 FIXED;
typedef u16 COLOR;
typedef u16 SCR_ENTRY, SE;
typedef u8  SCR_AFF_ENTRY, SAE;
typedef struct { u32 data[8];  } TILE, TILE4;
typedef struct { u32 data[16]; } TILE8;

typedef struct AFF_SRC
{
	s16 sx;         // Horizontal zoom         (8.8f)
	s16 sy;         // Vertical zoom           (8.8f)
	u16 alpha;      // Counter-clockwise angle ( range [0, 0xFFFF] )
} ALIGN4 AFF_SRC, ObjAffineSource;

typedef struct AFF_SRC_EX
{
	s32 tex_x;      // Texture-space anchor, x coordinate (.8f)
	s32 tex_y;      // Texture-space anchor, y coordinate (.8f)
	s16 scr_x;      // Screen-space anchor, x coordinate  (.0f)
	s16 scr_y;      // Screen-space anchor, y coordinate  (.0f)
	s16 sx;         // Horizontal zoom                    (8.8f)
	s16 sy;         // Vertical zoom                      (8.8f)
	u16 alpha;      // Counter-clockwise angle ( range [0, 0xFFFF] )
} ALIGN4 AFF_SRC_EX, BgAffineSource;

typedef struct AFF_DST
{
	s16 pa, pb;
	s16 pc, pd;
} ALIGN4 AFF_DST, ObjAffineDest;

typedef struct AFF_DST_EX
{
	s16 pa, pb;
	s16 pc, pd;
	s32 dx, dy;
} ALIGN4 AFF_DST_EX, BgAffineDest;

typedef struct POINT16 { s16 x, y; } ALIGN4 POINT16, BG_POINT;

typedef struct AFF_DST_EX BG_AFFINE;

typedef struct DMA_REC
{
	const void *src;
	void *dst;
	u32 cnt;
} DMA_REC;

typedef struct TMR_REC 
{
	union { u16 start, count; } PACKED;
	u16 cnt;
} ALIGN4 TMR_REC;

typedef COLOR PALBANK[16];

typedef SCR_ENTRY       SCREENLINE[32];
typedef SCR_ENTRY       SCREENMAT[32][32];
typedef SCR_ENTRY       SCREENBLOCK[1024];

typedef COLOR           M3LINE[240];
typedef u8              M4LINE[240];  // NOTE: u8, not u16!!
typedef COLOR           M5LINE[160];

typedef TILE            CHARBLOCK[512];
typedef TILE8           CHARBLOCK8[256];

typedef struct OBJ_ATTR
{
	u16 attr0;
	u16 attr1;
	u16 attr2;
	s16 fill;
} ALIGN4 OBJ_ATTR;

typedef struct OBJ_AFFINE
{
	u16 fill0[3];
	s16 pa;
	u16 fill1[3];
	s16 pb;
	u16 fill2[3];
	s16 pc;
	u16 fill3[3];
	s16 pd;
} ALIGN4 OBJ_AFFINE;

/* --------------------------------------------------------------
  GBA Memory map
----------------------------------------------------------------- */

#define MEM_EWRAM       0x02000000  // External work RAM
#define MEM_IWRAM       0x03000000  // Internal work RAM
#define MEM_IO          0x04000000  // I/O registers
#define MEM_PAL         0x05000000  // Palette. Note: no 8bit write !!
#define MEM_VRAM        0x06000000  // Video RAM. Note: no 8bit write !!
#define MEM_OAM         0x07000000  // Object Attribute Memory (OAM) Note: no 8bit write !!
#define MEM_ROM         0x08000000  // ROM. No write at all (duh)
#define MEM_SRAM        0x0E000000  // Static RAM. 8bit write only

#define EWRAM_SIZE      0x40000
#define IWRAM_SIZE      0x08000
#define PAL_SIZE        0x00400
#define VRAM_SIZE       0x18000
#define OAM_SIZE        0x00400
#define SRAM_SIZE       0x10000

#define PAL_BG_SIZE     0x00200     // BG palette size
#define PAL_OBJ_SIZE    0x00200     // Object palette size
#define CBB_SIZE        0x04000     // Charblock size
#define SBB_SIZE        0x00800     // Screenblock size
#define VRAM_BG_SIZE    0x10000     // BG VRAM size
#define VRAM_OBJ_SIZE   0x08000     // Object VRAM size
#define M3_SIZE         0x12C00     // Mode 3 buffer size
#define M4_SIZE         0x09600     // Mode 4 buffer size
#define M5_SIZE         0x0A000     // Mode 5 buffer size
#define VRAM_PAGE_SIZE  0x0A000     // Bitmap page size

#define MEM_PAL_BG      (MEM_PAL)                   // Background palette address
#define MEM_PAL_OBJ     (MEM_PAL + PAL_BG_SIZE)     // Object palette address
#define MEM_VRAM_FRONT  (MEM_VRAM)                  // Front page address
#define MEM_VRAM_BACK   (MEM_VRAM + VRAM_PAGE_SIZE) // Back page address
#define MEM_VRAM_OBJ    (MEM_VRAM + VRAM_BG_SIZE)   // Object VRAM address

/* --------------------------------------------------------------
  Structured access to GBA data types in memory
----------------------------------------------------------------- */

#define pal_bg_mem      ((volatile COLOR*)MEM_PAL)
#define pal_obj_mem     ((volatile COLOR*)MEM_PAL_OBJ)
#define pal_bg_bank     ((volatile PALBANK*)MEM_PAL)
#define pal_obj_bank    ((volatile PALBANK*)MEM_PAL_OBJ)
#define tile_mem        ((volatile CHARBLOCK*)MEM_VRAM)
#define tile8_mem       ((volatile CHARBLOCK8*)MEM_VRAM)
#define tile_mem_obj    ((volatile CHARBLOCK*)MEM_VRAM_OBJ)
#define tile8_mem_obj   ((volatile CHARBLOCK8*)MEM_VRAM_OBJ)
#define se_mem          ((volatile SCREENBLOCK*)MEM_VRAM)
#define se_mat          ((volatile SCREENMAT*)MEM_VRAM)
#define vid_mem         ((volatile COLOR*)MEM_VRAM)
#define m3_mem          ((volatile M3LINE*)MEM_VRAM)
#define vid_mem_front   ((volatile COLOR*)MEM_VRAM)
#define vid_mem_back    ((volatile COLOR*)MEM_VRAM_BACK)
#define m4_mem          ((volatile M4LINE*)MEM_VRAM)
#define m4_mem_back     ((volatile M4LINE*)MEM_VRAM_BACK)
#define m5_mem          ((volatile M5LINE*)MEM_VRAM)
#define m5_mem_back     ((volatile M5LINE*)MEM_VRAM_BACK)
#define oam_mem         ((volatile OBJ_ATTR*)MEM_OAM)
#define obj_mem         ((volatile OBJ_ATTR*)MEM_OAM)
#define obj_aff_mem     ((volatile OBJ_AFFINE*)MEM_OAM)
#define rom_mem         ((vu16*)MEM_ROM)
#define sram_mem        ((vu8*)MEM_SRAM)

/* --------------------------------------------------------------
  Memory mapped IO with Bitfield definitions
----------------------------------------------------------------- */

#define REG_BASE        MEM_IO

#define REG_IF_BIOS     *(vu16*)(REG_BASE-0x0008)       // IRQ ack for IntrWait functions
// (see REG_IF for bitdefs)
#define REG_RESET_DST   *(vu16*)(REG_BASE-0x0006)       // Destination for after SoftReset
#define REG_ISR_MAIN    *(fnptr*)(REG_BASE-0x0004)      // IRQ handler address

/* --------------------------------------------------------------
  LCD Display
----------------------------------------------------------------- */

#define REG_DISPCNT     *(vu32*)(REG_BASE+0x0000)       // Display control
#define DCNT_MODE0           0      // Mode 0; bg 0-4: reg
#define DCNT_MODE1      0x0001      // Mode 1; bg 0-1: reg; bg 2: affine
#define DCNT_MODE2      0x0002      // Mode 2; bg 2-3: affine
#define DCNT_MODE3      0x0003      // Mode 3; bg2: 240x160\@16 bitmap
#define DCNT_MODE4      0x0004      // Mode 4; bg2: 240x160\@8 bitmap
#define DCNT_MODE5      0x0005      // Mode 5; bg2: 160x128\@16 bitmap
#define DCNT_GB         0x0008      // (R) GBC indicator
#define DCNT_PAGE       0x0010      // Page indicator
#define DCNT_OAM_HBL    0x0020      // Allow OAM updates in HBlank
#define DCNT_OBJ_2D          0      // OBJ-VRAM as matrix
#define DCNT_OBJ_1D     0x0040      // OBJ-VRAM as array
#define DCNT_BLANK      0x0080      // Force screen blank
#define DCNT_BG0        0x0100      // Enable bg 0
#define DCNT_BG1        0x0200      // Enable bg 1
#define DCNT_BG2        0x0400      // Enable bg 2
#define DCNT_BG3        0x0800      // Enable bg 3
#define DCNT_OBJ        0x1000      // Enable objects
#define DCNT_WIN0       0x2000      // Enable window 0
#define DCNT_WIN1       0x4000      // Enable window 1
#define DCNT_WINOBJ     0x8000      // Enable object window
#define DCNT_GREEN_SWAP 0x00010000  // Undocumented GBA quirk

#define DCNT_MODE_MASK  0x0007
#define DCNT_MODE_SHIFT      0
#define DCNT_MODE(n)    ((n)<<DCNT_MODE_SHIFT)

#define DCNT_LAYER_MASK 0x1F00
#define DCNT_LAYER_SHIFT     8
#define DCNT_LAYER(n)   ((n)<<DCNT_LAYER_SHIFT)

#define DCNT_WIN_MASK   0xE000
#define DCNT_WIN_SHIFT      13
#define DCNT_WIN(n)     ((n)<<DCNT_WIN_SHIFT)

#define DCNT_BUILD(mode, layer, win, obj1d, objhbl) \
( \
	    (((win)&7)<<13) | (((layer)&31)<<8) | (((obj1d)&1)<<6) \
	| (((objhbl)&1)<<5) | ((mode)&7) \
)


#define REG_DISPSTAT    *(vu16*)(REG_BASE+0x0004)       // Display status
#define DSTAT_IN_VBL    0x0001      // Now in VBlank
#define DSTAT_IN_HBL    0x0002      // Now in HBlank
#define DSTAT_IN_VCT    0x0004      // Now in set VCount
#define DSTAT_VBL_IRQ   0x0008      // Enable VBlank irq
#define DSTAT_HBL_IRQ   0x0010      // Enable HBlank irq
#define DSTAT_VCT_IRQ   0x0020      // Enable VCount irq

#define DSTAT_VCT_MASK  0xFF00
#define DSTAT_VCT_SHIFT      8
#define DSTAT_VCT(n)    ((n)<<DSTAT_VCT_SHIFT)


#define REG_VCOUNT      *(vu16*)(REG_BASE+0x0006)       // Scanline count
#define REG_BGCNT       ((vu16*)(REG_BASE+0x0008))      // Bg control array
#define REG_BG0CNT      *(vu16*)(REG_BASE+0x0008)       // Bg0 control
#define REG_BG1CNT      *(vu16*)(REG_BASE+0x000A)       // Bg1 control
#define REG_BG2CNT      *(vu16*)(REG_BASE+0x000C)       // Bg2 control
#define REG_BG3CNT      *(vu16*)(REG_BASE+0x000E)       // Bg3 control
#define BG_MOSAIC       0x0040      // Enable Mosaic
#define BG_4BPP              0      // 4bpp (16 color) bg (no effect on affine bg)
#define BG_8BPP         0x0080      // 8bpp (256 color) bg (no effect on affine bg)
#define BG_WRAP         0x2000      // Wrap around edges of affine bgs
#define BG_SIZE0             0
#define BG_SIZE1        0x4000
#define BG_SIZE2        0x8000
#define BG_SIZE3        0xC000
#define BG_REG_32x32         0      // reg bg, 32x32 (256x256 px)
#define BG_REG_64x32    0x4000      // reg bg, 64x32 (512x256 px)
#define BG_REG_32x64    0x8000      // reg bg, 32x64 (256x512 px)
#define BG_REG_64x64    0xC000      // reg bg, 64x64 (512x512 px)
#define BG_AFF_16x16         0      // affine bg, 16x16 (128x128 px)
#define BG_AFF_32x32    0x4000      // affine bg, 32x32 (256x256 px)
#define BG_AFF_64x64    0x8000      // affine bg, 64x64 (512x512 px)
#define BG_AFF_128x128  0xC000      // affine bg, 128x128 (1024x1024 px)

#define BG_PRIO_MASK    0x0003
#define BG_PRIO_SHIFT        0
#define BG_PRIO(n)      ((n)<<BG_PRIO_SHIFT)

#define BG_CBB_MASK     0x000C
#define BG_CBB_SHIFT         2
#define BG_CBB(n)       ((n)<<BG_CBB_SHIFT)

#define BG_SBB_MASK     0x1F00
#define BG_SBB_SHIFT         8
#define BG_SBB(n)       ((n)<<BG_SBB_SHIFT)

#define BG_SIZE_MASK    0xC000
#define BG_SIZE_SHIFT       14
#define BG_SIZE(n)      ((n)<<BG_SIZE_SHIFT)

#define BG_BUILD(cbb, sbb, size, bpp, prio, mos, wrap) \
( \
	   ((size)<<14)  | (((wrap)&1)<<13) | (((sbb)&31)<<8 \
	| (((bpp)&8)<<4) | (((mos)&1)<<6)   | (((cbb)&3)<<2) \
	| ((prio)&3) \
)


#define REG_BG_OFS      ((volatile BG_POINT*)(REG_BASE+0x0010))  // Bg scroll array
#define REG_BG0HOFS     *(vu16*)(REG_BASE+0x0010)       // Bg0 horizontal scroll
#define REG_BG0VOFS     *(vu16*)(REG_BASE+0x0012)       // Bg0 vertical scroll
#define REG_BG1HOFS     *(vu16*)(REG_BASE+0x0014)       // Bg1 horizontal scroll
#define REG_BG1VOFS     *(vu16*)(REG_BASE+0x0016)       // Bg1 vertical scroll
#define REG_BG2HOFS     *(vu16*)(REG_BASE+0x0018)       // Bg2 horizontal scroll
#define REG_BG2VOFS     *(vu16*)(REG_BASE+0x001A)       // Bg2 vertical scroll
#define REG_BG3HOFS     *(vu16*)(REG_BASE+0x001C)       // Bg3 horizontal scroll
#define REG_BG3VOFS     *(vu16*)(REG_BASE+0x001E)       // Bg3 vertical scroll
#define REG_BG_AFFINE   ((volatile BG_AFFINE*)(REG_BASE+0x0000)) // Bg affine array
#define REG_BG2PA       *(vs16*)(REG_BASE+0x0020)       // Bg2 matrix.pa
#define REG_BG2PB       *(vs16*)(REG_BASE+0x0022)       // Bg2 matrix.pb
#define REG_BG2PC       *(vs16*)(REG_BASE+0x0024)       // Bg2 matrix.pc
#define REG_BG2PD       *(vs16*)(REG_BASE+0x0026)       // Bg2 matrix.pd
#define REG_BG2X        *(vs32*)(REG_BASE+0x0028)       // Bg2 x scroll
#define REG_BG2Y        *(vs32*)(REG_BASE+0x002C)       // Bg2 y scroll
#define REG_BG3PA       *(vs16*)(REG_BASE+0x0030)       // Bg3 matrix.pa.
#define REG_BG3PB       *(vs16*)(REG_BASE+0x0032)       // Bg3 matrix.pb
#define REG_BG3PC       *(vs16*)(REG_BASE+0x0034)       // Bg3 matrix.pc
#define REG_BG3PD       *(vs16*)(REG_BASE+0x0036)       // Bg3 matrix.pd
#define REG_BG3X        *(vs32*)(REG_BASE+0x0038)       // Bg3 x scroll
#define REG_BG3Y        *(vs32*)(REG_BASE+0x003C)       // Bg3 y scroll
#define REG_WIN0H       *(vu16*)(REG_BASE+0x0040)       // win0 right, left (0xLLRR)
#define REG_WIN1H       *(vu16*)(REG_BASE+0x0042)       // win1 right, left (0xLLRR)
#define REG_WIN0V       *(vu16*)(REG_BASE+0x0044)       // win0 bottom, top (0xTTBB)
#define REG_WIN1V       *(vu16*)(REG_BASE+0x0046)       // win1 bottom, top (0xTTBB)
#define REG_WININ       *(vu16*)(REG_BASE+0x0048)       // win0, win1 control
#define REG_WINOUT      *(vu16*)(REG_BASE+0x004A)       // winOut, winObj control
#define WIN_BG0         0x0001      // Windowed bg 0
#define WIN_BG1         0x0002      // Windowed bg 1
#define WIN_BG2         0x0004      // Windowed bg 2
#define WIN_BG3         0x0008      // Windowed bg 3
#define WIN_OBJ         0x0010      // Windowed objects
#define WIN_ALL         0x001F      // All layers in window.
#define WIN_BLD         0x0020      // Windowed blending

#define WIN_LAYER_MASK  0x003F
#define WIN_LAYER_SHIFT      0
#define WIN_LAYER(n)    ((n)<<WIN_LAYER_SHIFT)

#define WIN_BUILD(low, high)        ( ((high)<<8) | (low) )
#define WININ_BUILD(win0, win1)     WIN_BUILD(win0, win1)
#define WINOUT_BUILD(out, obj)      WIN_BUILD(out, obj)

#define REG_MOSAIC      *(vu32*)(REG_BASE+0x004C)       // Mosaic control
#define MOS_BH_MASK     0x000F
#define MOS_BH_SHIFT         0
#define MOS_BH(n)       ((n)<<MOS_BH_SHIFT)

#define MOS_BV_MASK     0x00F0
#define MOS_BV_SHIFT         4
#define MOS_BV(n)       ((n)<<MOS_BV_SHIFT)

#define MOS_OH_MASK     0x0F00
#define MOS_OH_SHIFT         8
#define MOS_OH(n)       ((n)<<MOS_OH_SHIFT)

#define MOS_OV_MASK     0xF000
#define MOS_OV_SHIFT        12
#define MOS_OV(n)       ((n)<<MOS_OV_SHIFT)

#define MOS_BUILD(bh, bv, oh, ov) \
	( (((ov)&15)<<12) | (((oh)&15)<<8) | (((bv)&15)<<4)| ((bh)&15) )


#define REG_BLDCNT      *(vu16*)(REG_BASE+0x0050)       // Alpha control
#define BLD_BG0         0x0001      // Blend bg 0
#define BLD_BG1         0x0002      // Blend bg 1
#define BLD_BG2         0x0004      // Blend bg 2
#define BLD_BG3         0x0008      // Blend bg 3
#define BLD_OBJ         0x0010      // Blend objects
#define BLD_ALL         0x001F      // All layers (except backdrop)
#define BLD_BACKDROP    0x0020      // Blend backdrop
#define BLD_OFF              0      // Blend mode is off
#define BLD_STD         0x0040      // Normal alpha blend (with REG_EV)
#define BLD_WHITE       0x0080      // Fade to white (with REG_Y)
#define BLD_BLACK       0x00C0      // Fade to black (with REG_Y)

#define BLD_TOP_MASK    0x003F
#define BLD_TOP_SHIFT        0
#define BLD_TOP(n)      ((n)<<BLD_TOP_SHIFT)

#define BLD_MODE_MASK   0x00C0
#define BLD_MODE_SHIFT       6
#define BLD_MODE(n)     ((n)<<BLD_MODE_SHIFT)

#define BLD_BOT_MASK    0x3F00
#define BLD_BOT_SHIFT        8
#define BLD_BOT(n)      ((n)<<BLD_BOT_SHIFT)

#define BLD_BUILD(top, bot, mode) \
	( (((bot)&63)<<8) | (((mode)&3)<<6) | ((top)&63) )


#define REG_BLDALPHA    *(vu16*)(REG_BASE+0x0052)       // Fade level
#define BLD_EVA_MASK    0x001F
#define BLD_EVA_SHIFT        0
#define BLD_EVA(n)      ((n)<<BLD_EVA_SHIFT)

#define BLD_EVB_MASK    0x1F00
#define BLD_EVB_SHIFT        8
#define BLD_EVB(n)      ((n)<<BLD_EVB_SHIFT)

#define BLDA_BUILD(eva, evb) \
	( ((eva)&31) | (((evb)&31)<<8) )


#define REG_BLDY        *(vu16*)(REG_BASE+0x0054)       // Blend levels
#define BLDY_MASK       0x001F
#define BLDY_SHIFT           0
#define BLDY(n)         ((n)<<BLD_EY_SHIFT)

#define BLDY_BUILD(ey)  ( (ey)&31 )


/* --------------------------------------------------------------
  Sound
----------------------------------------------------------------- */

#define REG_SND1SWEEP   *(vu16*)(REG_BASE+0x0060)       // Channel 1 Sweep
#define SSW_INC              0          // Increasing sweep rate
#define SSW_DEC         0x0008      // Decreasing sweep rate
#define SSW_OFF         0x0008      // Disable sweep altogether

#define SSW_SHIFT_MASK  0x0007
#define SSW_SHIFT_SHIFT      0
#define SSW_SHIFT(n)    ((n)<<SSW_SHIFT_SHIFT)

#define SSW_TIME_MASK   0x0070
#define SSW_TIME_SHIFT       4
#define SSW_TIME(n)     ((n)<<SSW_TIME_SHIFT)

#define SSW_BUILD(shift, dir, time) \
	( (((time)&7)<<4) | ((dir)<<3) | ((shift)&7) )

#define REG_SND1CNT     *(vu16*)(REG_BASE+0x0062)       // Channel 1 Control
#define SSQR_DUTY1_8         0      // 12.5% duty cycle (#-------)
#define SSQR_DUTY1_4    0x0040      // 25% duty cycle (##------)
#define SSQR_DUTY1_2    0x0080      // 50% duty cycle (####----)
#define SSQR_DUTY3_4    0x00C0      // 75% duty cycle (######--) Equivalent to 25%
#define SSQR_INC             0      // Increasing volume
#define SSQR_DEC        0x0800      // Decreasing volume

#define SSQR_LEN_MASK   0x003F
#define SSQR_LEN_SHIFT       0
#define SSQR_LEN(n)     ((n)<<SSQR_LEN_SHIFT)

#define SSQR_DUTY_MASK  0x00C0
#define SSQR_DUTY_SHIFT      6
#define SSQR_DUTY(n)    ((n)<<SSQR_DUTY_SHIFT)

#define SSQR_TIME_MASK  0x0700
#define SSQR_TIME_SHIFT      8
#define SSQR_TIME(n)    ((n)<<SSQR_TIME_SHIFT)

#define SSQR_IVOL_MASK  0xF000
#define SSQR_IVOL_SHIFT     12
#define SSQR_IVOL(n)    ((n)<<SSQR_IVOL_SHIFT)

#define SSQR_ENV_BUILD(ivol, dir, time) \
	(  ((ivol)<<12) | ((dir)<<11) | (((time)&7)<<8) )

#define SSQR_BUILD(ivol, dir, step, duty, len) \
	( SSQR_ENV_BUILD(ivol,dir,step) | (((duty)&3)<<6) | ((len)&63) )


#define REG_SND1FREQ    *(vu16*)(REG_BASE+0x0064)       // Channel 1 frequency
#define SFREQ_HOLD           0      // Continuous play
#define SFREQ_TIMED     0x4000      // Timed play
#define SFREQ_RESET     0x8000      // Reset sound

#define SFREQ_RATE_MASK 0x07FF
#define SFREQ_RATE_SHIFT     0
#define SFREQ_RATE(n)   ((n)<<SFREQ_RATE_SHIFT)

#define SFREQ_BUILD(rate, timed, reset) \
	( ((rate)&0x7FF) | ((timed)<<14) | ((reset)<<15) )


#define REG_SND2CNT     *(vu16*)(REG_BASE+0x0068)       // Channel 2 control
#define REG_SND2FREQ    *(vu16*)(REG_BASE+0x006C)       // Channel 2 frequency
#define REG_SND3SEL     *(vu16*)(REG_BASE+0x0070)       // Channel 3 wave select
#define REG_SND3CNT     *(vu16*)(REG_BASE+0x0072)       // Channel 3 control
#define REG_SND3FREQ    *(vu16*)(REG_BASE+0x0074)       // Channel 3 frequency
#define REG_SND4CNT     *(vu16*)(REG_BASE+0x0078)       // Channel 4 control
#define REG_SND4FREQ    *(vu16*)(REG_BASE+0x007C)       // Channel 4 frequency
#define REG_SNDCNT      *(vu32*)(REG_BASE+0x0080)       // Main sound control
#define REG_SNDDMGCNT   *(vu16*)(REG_BASE+0x0080)       // DMG channel control
#define SDMG_LSQR1      0x0100      // Enable channel 1 on left
#define SDMG_LSQR2      0x0200      // Enable channel 2 on left
#define SDMG_LWAVE      0x0400      // Enable channel 3 on left
#define SDMG_LNOISE     0x0800      // Enable channel 4 on left
#define SDMG_RSQR1      0x1000      // Enable channel 1 on right
#define SDMG_RSQR2      0x2000      // Enable channel 2 on right
#define SDMG_RWAVE      0x4000      // Enable channel 3 on right
#define SDMG_RNOISE     0x8000      // Enable channel 4 on right

#define SDMG_LVOL_MASK  0x0007
#define SDMG_LVOL_SHIFT      0
#define SDMG_LVOL(n)    ((n)<<SDMG_LVOL_SHIFT)

#define SDMG_RVOL_MASK  0x0070
#define SDMG_RVOL_SHIFT      4
#define SDMG_RVOL(n)    ((n)<<SDMG_RVOL_SHIFT)

#define SDMG_SQR1       0x01        // Unshifted values
#define SDMG_SQR2       0x02
#define SDMG_WAVE       0x04
#define SDMG_NOISE      0x08

#define SDMG_BUILD(_lmode, _rmode, _lvol, _rvol) \
	( ((_rmode)<<12) | ((_lmode)<<8) | (((_rvol)&7)<<4) | ((_lvol)&7) )

#define SDMG_BUILD_LR(_mode, _vol) SDMG_BUILD(_mode, _mode, _vol, _vol)


#define REG_SNDDSCNT    *(vu16*)(REG_BASE+0x0082)       // Direct Sound control
#define SDS_DMG25            0      // Tone generators at 25% volume
#define SDS_DMG50       0x0001      // Tone generators at 50% volume
#define SDS_DMG100      0x0002      // Tone generators at 100% volume
#define SDS_A50              0      // Direct Sound A at 50% volume
#define SDS_A100        0x0004      // Direct Sound A at 100% volume
#define SDS_B50              0      // Direct Sound B at 50% volume
#define SDS_B100        0x0008      // Direct Sound B at 100% volume
#define SDS_AR          0x0100      // Enable Direct Sound A on right
#define SDS_AL          0x0200      // Enable Direct Sound A on left
#define SDS_ATMR0            0      // Direct Sound A to use timer 0
#define SDS_ATMR1       0x0400      // Direct Sound A to use timer 1
#define SDS_ARESET      0x0800      // Reset FIFO of Direct Sound A
#define SDS_BR          0x1000      // Enable Direct Sound B on right
#define SDS_BL          0x2000      // Enable Direct Sound B on left
#define SDS_BTMR0            0      // Direct Sound B to use timer 0
#define SDS_BTMR1       0x4000      // Direct Sound B to use timer 1
#define SDS_BRESET      0x8000      // Reset FIFO of Direct Sound B


#define REG_SNDSTAT     *(vu16*)(REG_BASE+0x0084)       // Sound status
#define SSTAT_SQR1      0x0001      // (R) Channel 1 status
#define SSTAT_SQR2      0x0002      // (R) Channel 2 status
#define SSTAT_WAVE      0x0004      // (R) Channel 3 status
#define SSTAT_NOISE     0x0008      // (R) Channel 4 status
#define SSTAT_DISABLE        0      // Disable sound
#define SSTAT_ENABLE    0x0080      // Enable sound. NOTE: enable before using any other sound regs


#define REG_SNDBIAS     *(vu16*)(REG_BASE+0x0088)       // Sound bias
#define REG_WAVE_RAM    (vu32*)(REG_BASE+0x0090)        // Channel 3 wave buffer
#define REG_WAVE_RAM0   *(vu32*)(REG_BASE+0x0090)
#define REG_WAVE_RAM1   *(vu32*)(REG_BASE+0x0094)
#define REG_WAVE_RAM2   *(vu32*)(REG_BASE+0x0098)
#define REG_WAVE_RAM3   *(vu32*)(REG_BASE+0x009C)
#define REG_FIFO_A      *(vu32*)(REG_BASE+0x00A0)       // DSound A FIFO
#define REG_FIFO_B      *(vu32*)(REG_BASE+0x00A4)       // DSound B FIFO

/* --------------------------------------------------------------
  DMA Transfer
----------------------------------------------------------------- */

#define REG_DMA         ((volatile DMA_REC*)(REG_BASE+0x00B0))  // DMA as DMA_REC array
#define REG_DMA0SAD     *(vu32*)(REG_BASE+0x00B0)       // DMA 0 Source address
#define REG_DMA0DAD     *(vu32*)(REG_BASE+0x00B4)       // DMA 0 Destination address
#define REG_DMA0CNT     *(vu32*)(REG_BASE+0x00B8)       // DMA 0 Control
#define REG_DMA1SAD     *(vu32*)(REG_BASE+0x00BC)       // DMA 1 Source address
#define REG_DMA1DAD     *(vu32*)(REG_BASE+0x00C0)       // DMA 1 Destination address
#define REG_DMA1CNT     *(vu32*)(REG_BASE+0x00C4)       // DMA 1 Control
#define REG_DMA2SAD     *(vu32*)(REG_BASE+0x00C8)       // DMA 2 Source address
#define REG_DMA2DAD     *(vu32*)(REG_BASE+0x00CC)       // DMA 2 Destination address
#define REG_DMA2CNT     *(vu32*)(REG_BASE+0x00D0)       // DMA 2 Control
#define REG_DMA3SAD     *(vu32*)(REG_BASE+0x00D4)       // DMA 3 Source address
#define REG_DMA3DAD     *(vu32*)(REG_BASE+0x00D8)       // DMA 3 Destination address
#define REG_DMA3CNT     *(vu32*)(REG_BASE+0x00DC)       // DMA 3 Control

// REG_DMAxCNT
#define DMA_DST_INC              0  // Incrementing destination address
#define DMA_DST_DEC     0x00200000  // Decrementing destination
#define DMA_DST_FIXED   0x00400000  // Fixed destination 
#define DMA_DST_RELOAD  0x00600000  // Increment destination, reset after full run
#define DMA_SRC_INC              0  // Incrementing source address
#define DMA_SRC_DEC     0x00800000  // Decrementing source address
#define DMA_SRC_FIXED   0x01000000  // Fixed source address
#define DMA_REPEAT      0x02000000  // Repeat transfer at next start condition 
#define DMA_16                   0  // Transfer by halfword
#define DMA_32          0x04000000  // Transfer by word
#define DMA_AT_NOW               0  // Start transfer now
#define DMA_GAMEPAK     0x08000000  // Gamepak DRQ
#define DMA_AT_VBLANK   0x10000000  // Start transfer at VBlank
#define DMA_AT_HBLANK   0x20000000  // Start transfer at HBlank
#define DMA_AT_SPECIAL  0x30000000  // Start copy at 'special' condition. Channel dependent
#define DMA_AT_FIFO     0x30000000  // Start at FIFO empty (DMA0/DMA1)
#define DMA_AT_REFRESH  0x30000000  // VRAM special; start at VCount=2 (DMA3)
#define DMA_IRQ         0x40000000  // Enable DMA irq
#define DMA_ENABLE      0x80000000  // Enable DMA

#define DMA_COUNT_MASK  0x0000FFFF
#define DMA_COUNT_SHIFT          0
#define DMA_COUNT(n)    ((n)<<DMA_COUNT_SHIFT)

#define DMA_NOW     (DMA_ENABLE  | DMA_AT_NOW)
#define DMA_16NOW   (DMA_NOW | DMA_16)
#define DMA_32NOW   (DMA_NOW | DMA_32)

#define DMA_CPY16   (DMA_NOW | DMA_16)
#define DMA_CPY32   (DMA_NOW | DMA_32)

#define DMA_FILL16  (DMA_NOW | DMA_SRC_FIXED | DMA_16)
#define DMA_FILL32  (DMA_NOW | DMA_SRC_FIXED | DMA_32)

#define DMA_HDMA    (DMA_ENABLE | DMA_REPEAT | DMA_AT_HBLANK | DMA_DST_RELOAD)


/* --------------------------------------------------------------
  Timers Registers
----------------------------------------------------------------- */

#define REG_TM          ((volatile TMR_REC*)(REG_BASE+0x0100))  // Timers as TMR_REC array
#define REG_TM0D        *(vu16*)(REG_BASE+0x0100)       // Timer 0 data
#define REG_TM0CNT      *(vu16*)(REG_BASE+0x0102)       // Timer 0 control
#define REG_TM1D        *(vu16*)(REG_BASE+0x0104)       // Timer 1 data
#define REG_TM1CNT      *(vu16*)(REG_BASE+0x0106)       // Timer 1 control
#define REG_TM2D        *(vu16*)(REG_BASE+0x0108)       // Timer 2 data
#define REG_TM2CNT      *(vu16*)(REG_BASE+0x010A)       // Timer 2 control
#define REG_TM3D        *(vu16*)(REG_BASE+0x010C)       // Timer 3 data
#define REG_TM3CNT      *(vu16*)(REG_BASE+0x010E)       // Timer 3 control

// REG_TMxCNT
#define TM_FREQ_SYS          0      // System clock timer (16.7 Mhz)
#define TM_FREQ_1            0      // 1 cycle/tick (16.7 Mhz)
#define TM_FREQ_64      0x0001      // 64 cycles/tick (262 kHz)
#define TM_FREQ_256     0x0002      // 256 cycles/tick (66 kHz)
#define TM_FREQ_1024    0x0003      // 1024 cycles/tick (16 kHz)
#define TM_CASCADE      0x0004      // Increment when preceding timer overflows
#define TM_IRQ          0x0040      // Enable timer irq
#define TM_ENABLE       0x0080      // Enable timer

#define TM_FREQ_MASK    0x0003
#define TM_FREQ_SHIFT        0
#define TM_FREQ(n)      ((n)<<TM_FREQ_SHIFT)

/* --------------------------------------------------------------
  Serial Communication
----------------------------------------------------------------- */

#define REG_SIOCNT      *(vu16*)(REG_BASE+0x0128)       // Serial IO control (Normal/MP/UART)
#define REG_SIODATA     ((vu32*)(REG_BASE+0x0120))
#define REG_SIODATA32   *(vu32*)(REG_BASE+0x0120)       // Normal/UART 32bit data
#define REG_SIODATA8    *(vu16*)(REG_BASE+0x012A)       // Normal/UART 8bit data
#define REG_SIOMULTI    ((vu16*)(REG_BASE+0x0120))      // Multiplayer data array
#define REG_SIOMULTI0   *(vu16*)(REG_BASE+0x0120)       // MP master data
#define REG_SIOMULTI1   *(vu16*)(REG_BASE+0x0122)       // MP Slave 1 data
#define REG_SIOMULTI2   *(vu16*)(REG_BASE+0x0124)       // MP Slave 2 data 
#define REG_SIOMULTI3   *(vu16*)(REG_BASE+0x0126)       // MP Slave 3 data
#define REG_SIOMLT_RECV *(vu16*)(REG_BASE+0x0120)       // MP data receiver
#define REG_SIOMLT_SEND *(vu16*)(REG_BASE+0x012A)       // MP data sender

#define SIO_MODE_8BIT   0x0000      // Normal comm mode, 8-bit.
#define SIO_MODE_32BIT  0x1000      // Normal comm mode, 32-bit.
#define SIO_MODE_MULTI  0x2000      // Multi-play comm mode.
#define SIO_MODE_UART   0x3000      // UART comm mode.

#define SIO_SI_HIGH     0x0004
#define SIO_IRQ         0x4000      // Enable serial irq.

#define SIO_MODE_MASK   0x3000
#define SIO_MODE_SHIFT      12
#define SIO_MODE(n)     ((n)<<SIO_MODE_SHIFT)

#define SION_CLK_EXT    0x0000      // Slave unit; use external clock (default).
#define SION_CLK_INT    0x0001      // Master unit; use internal clock.

#define SION_256KHZ     0x0000      // 256 kHz clockspeed (default).
#define SION_2MHZ       0x0002      // 2 MHz clockspeed.

#define SION_RECV_HIGH  0x0004      // SI high; opponent ready to receive (R).
#define SION_SEND_HIGH  0x0008      // SO high; ready to transfer.

#define SION_ENABLE     0x0080      // Start transfer/transfer enabled.

#define SIOM_9600       0x0000      // Baud rate,   9.6 kbps.
#define SIOM_38400      0x0001      // Baud rate,  38.4 kbps.
#define SIOM_57600      0x0002      // Baud rate,  57.6 kbps.
#define SIOM_115200     0x0003      // Baud rate, 115.2 kbps.

#define SIOM_SI         0x0004      // SI port (R).
#define SIOM_SLAVE      0x0004      // Not the master (R).
#define SIOM_SD         0x0008      // SD port (R).
#define SIOM_CONNECTED  0x0008      // All GBAs connected (R)

#define SIOM_ERROR      0x0040      // Error in transfer (R).
#define SIOM_ENABLE     0x0080      // Start transfer/transfer enabled.

#define SIOM_BAUD_MASK  0x0003
#define SIOM_BAUD_SHIFT      0
#define SIOM_BAUD(n)    ((n)<<SIOM_BAUD_SHIFT)

#define SIOM_ID_MASK    0x0030      // Multi-player ID mask (R)
#define SIOM_ID_SHIFT        4
#define SIOM_ID(n)      ((n)<<SIOM_ID_SHIFT)

#define SIOU_9600       0x0000      // Baud rate,   9.6 kbps.
#define SIOU_38400      0x0001      // Baud rate,  38.4 kbps.
#define SIOU_57600      0x0002      // Baud rate,  57.6 kbps.
#define SIOU_115200     0x0003      // Baud rate, 115.2 kbps.

#define SIOU_CTS        0x0004      // CTS enable.
#define SIOU_PARITY_EVEN    0x0000  // Use even parity.
#define SIOU_PARITY_ODD 0x0008      // Use odd parity.
#define SIOU_SEND_FULL  0x0010      // Send data is full (R).
#define SIOU_RECV_EMPTY 0x0020      // Receive data is empty (R).
#define SIOU_ERROR      0x0040      // Error in transfer (R).
#define SIOU_7BIT       0x0000      // Data is 7bits long.
#define SIOU_8BIT       0x0080      // Data is 8bits long.
#define SIOU_SEND       0x0100      // Start sending data.
#define SIOU_RECV       0x0200      // Start receiving data.

#define SIOU_BAUD_MASK  0x0003
#define SIOU_BAUD_SHIFT      0
#define SIOU_BAUD(n)    ((n)<<SIOU_BAUD_SHIFT)

#define R_MODE_NORMAL   0x0000      // Normal mode.
#define R_MODE_MULTI    0x0000      // Multiplayer mode.
#define R_MODE_UART     0x0000      // UART mode.
#define R_MODE_GPIO     0x8000      // General purpose mode.
#define R_MODE_JOYBUS   0xC000      // JOY mode.

#define R_MODE_MASK     0xC000
#define R_MODE_SHIFT        14
#define R_MODE(n)       ((n)<<R_MODE_SHIFT)

#define GPIO_SC         0x0001      // Data
#define GPIO_SD         0x0002
#define GPIO_SI         0x0004
#define GPIO_SO         0x0008
#define GPIO_SC_IO      0x0010      // Select I/O
#define GPIO_SD_IO      0x0020
#define GPIO_SI_IO      0x0040
#define GPIO_SO_IO      0x0080
#define GPIO_SC_INPUT   0x0000      // Input setting
#define GPIO_SD_INPUT   0x0000
#define GPIO_SI_INPUT   0x0000
#define GPIO_SO_INPUT   0x0000
#define GPIO_SC_OUTPUT  0x0010      // Output setting
#define GPIO_SD_OUTPUT  0x0020
#define GPIO_SI_OUTPUT  0x0040
#define GPIO_SO_OUTPUT  0x0080

#define GPIO_IRQ        0x0100      // Interrupt on SI.

/* --------------------------------------------------------------
  Keypad and JOY Bus
----------------------------------------------------------------- */

#define REG_KEYINPUT    *(vu16*)(REG_BASE+0x0130)       // Key status (read only??)
#define KEY_A           0x0001      // Button A
#define KEY_B           0x0002      // Button B
#define KEY_SELECT      0x0004      // Select button
#define KEY_START       0x0008      // Start button
#define KEY_RIGHT       0x0010      // Right D-pad
#define KEY_LEFT        0x0020      // Left D-pad
#define KEY_UP          0x0040      // Up D-pad
#define KEY_DOWN        0x0080      // Down D-pad
#define KEY_R           0x0100      // Shoulder R
#define KEY_L           0x0200      // Shoulder L
#define KEY_ACCEPT      0x0009      // Accept buttons: A or start
#define KEY_CANCEL      0x0002      // Cancel button: B (well, it usually is)
#define KEY_RESET       0x030C      // St+Se+L+R
#define KEY_FIRE        0x0003      // Fire buttons: A or B
#define KEY_SPECIAL     0x000C      // Special buttons: Select or Start
#define KEY_DIR         0x00F0      // Directions: left, right, up down
#define KEY_SHOULDER    0x0300      // L or R
#define KEY_ANY         0x03FF      // Here's the Any key :)
#define KEY_MASK        0x03FF


#define REG_KEYCNT      *(vu16*)(REG_BASE+0x0132)       // Key IRQ control
#define KCNT_IRQ        0x4000      // Enable key irq
#define KCNT_OR              0      // Interrupt on any of selected keys
#define KCNT_AND        0x8000      // Interrupt on all of selected keys


#define REG_RCNT        *(vu16*)(REG_BASE+0x0134)       // SIO Mode Select/General Purpose Data
#define REG_JOYCNT      *(vu16*)(REG_BASE+0x0140)       // JOY bus control
#define REG_JOY_RECV    *(vu32*)(REG_BASE+0x0150)       // JOY bus receiever
#define REG_JOY_TRANS   *(vu32*)(REG_BASE+0x0154)       // JOY bus transmitter
#define REG_JOYSTAT     *(vu16*)(REG_BASE+0x0158)       // JOY bus status

/* --------------------------------------------------------------
  Interrupts and bus control
----------------------------------------------------------------- */

#define REG_IE          *(vu16*)(REG_BASE+0x0200)       // IRQ enable
#define REG_IF          *(vu16*)(REG_BASE+0x0202)       // IRQ status/acknowledge
#define IRQ_VBLANK      0x0001      // Catch VBlank irq
#define IRQ_HBLANK      0x0002      // Catch HBlank irq
#define IRQ_VCOUNT      0x0004      // Catch VCount irq
#define IRQ_TIMER0      0x0008      // Catch timer 0 irq
#define IRQ_TIMER1      0x0010      // Catch timer 1 irq
#define IRQ_TIMER2      0x0020      // Catch timer 2 irq
#define IRQ_TIMER3      0x0040      // Catch timer 3 irq
#define IRQ_SERIAL      0x0080      // Catch serial comm irq
#define IRQ_DMA0        0x0100      // Catch DMA 0 irq
#define IRQ_DMA1        0x0200      // Catch DMA 1 irq
#define IRQ_DMA2        0x0400      // Catch DMA 2 irq
#define IRQ_DMA3        0x0800      // Catch DMA 3 irq
#define IRQ_KEYPAD      0x1000      // Catch key irq
#define IRQ_GAMEPAK     0x2000      // Catch cart irq

typedef enum eIrqIndex
{
	II_VBLANK=0,II_HBLANK,	II_VCOUNT,	II_TIMER0,
	II_TIMER1,	II_TIMER2,	II_TIMER3,	II_SERIAL,
	II_DMA0,	II_DMA1,	II_DMA2,	II_DMA3,
	II_KEYPAD,	II_GAMEPAK,	II_MAX
} eIrqIndex;

#define REG_WAITCNT     *(vu16*)(REG_BASE+0x0204)       // Waitstate control
#define WS_SRAM_4            0
#define WS_SRAM_3       0x0001
#define WS_SRAM_2       0x0002
#define WS_SRAM_8       0x0003
#define WS_ROM0_N4           0
#define WS_ROM0_N3      0x0004
#define WS_ROM0_N2      0x0008
#define WS_ROM0_N8      0x000C
#define WS_ROM0_S2           0
#define WS_ROM0_S1      0x0010
#define WS_ROM1_N4           0
#define WS_ROM1_N3      0x0020
#define WS_ROM1_N2      0x0040
#define WS_ROM1_N8      0x0060
#define WS_ROM1_S4           0
#define WS_ROM1_S1      0x0080
#define WS_ROM2_N4           0
#define WS_ROM2_N3      0x0100
#define WS_ROM2_N2      0x0200
#define WS_ROM2_N8      0x0300
#define WS_ROM2_S8           0
#define WS_ROM2_S1      0x0400
#define WS_PHI_OFF           0
#define WS_PHI_4        0x0800
#define WS_PHI_2        0x1000
#define WS_PHI_1        0x1800
#define WS_PREFETCH     0x4000
#define WS_GBA               0
#define WS_CGB          0x8000
#define WS_STANDARD     0x4317

#define REG_IME         *(vu16*)(REG_BASE+0x0208)       // IRQ master enable

#define REG_PAUSE       *(vu16*)(REG_BASE+0x0300)       // Pause system (?)


/* --------------------------------------------------------------
  VRAM, OAM, and Palette bitfield definitions
----------------------------------------------------------------- */

// screen size
#define SCREEN_WIDTH        240
#define SCREEN_HEIGHT       160
#define SCREEN_LINES        228     // total scanlines
#define M3_WIDTH            SCREEN_WIDTH
#define M3_HEIGHT           SCREEN_HEIGHT
#define M4_WIDTH            SCREEN_WIDTH
#define M4_HEIGHT           SCREEN_HEIGHT
#define M5_WIDTH            160
#define M5_HEIGHT           128
#define SCREEN_WIDTH_T      (SCREEN_WIDTH/8)    // size in tiles
#define SCREEN_HEIGHT_T     (SCREEN_HEIGHT/8)

// Palette color
#define RED_MASK            0x001F
#define RED_SHIFT                0
#define GREEN_MASK          0x03E0
#define GREEN_SHIFT              5
#define BLUE_MASK           0x7C00
#define BLUE_SHIFT              10

INLINE COLOR RGB15(int red, int green, int blue)
{
	return red + (green<<5) + (blue<<10);
}

INLINE COLOR RGB15_SAFE(int red, int green, int blue)
{
	return (red&31) + ((green&31)<<5) + ((blue&31)<<10);
}

INLINE COLOR RGB8(u8 red, u8 green, u8 blue)
{
	return  (red>>3) + ((green>>3)<<5) + ((blue>>3)<<10);
}

// Screen entries
#define SE_HFLIP        0x0400      // Horizontal flip
#define SE_VFLIP        0x0800      // Vertical flip

#define SE_ID_MASK      0x03FF
#define SE_ID_SHIFT          0
#define SE_ID(n)        ((n)<<SE_ID_SHIFT)

#define SE_FLIP_MASK    0x0C00
#define SE_FLIP_SHIFT       10
#define SE_FLIP(n)      ((n)<<SE_FLIP_SHIFT)

#define SE_PALBANK_MASK 0xF000
#define SE_PALBANK_SHIFT    12
#define SE_PALBANK(n)   ((n)<<SE_PALBANK_SHIFT)

#define SE_BUILD(id, PALBANK, hflip, vflip) \
	( ((id)&0x03FF) | (((hflip)&1)<<10) | (((vflip)&1)<<11) | ((PALBANK)<<12) )


// OAM attribute 0

#define ATTR0_REG            0      // Regular object
#define ATTR0_AFF       0x0100      // Affine object
#define ATTR0_HIDE      0x0200      // Inactive object
#define ATTR0_AFF_DBL   0x0300      // Double-size affine object
#define ATTR0_AFF_DBL_BIT   0x0200
#define ATTR0_BLEND     0x0400      // Enable blend
#define ATTR0_WINDOW    0x0800      // Use for object window
#define ATTR0_MOSAIC    0x1000      // Enable mosaic
#define ATTR0_4BPP           0      // Use 4bpp (16 color) tiles
#define ATTR0_8BPP      0x2000      // Use 8bpp (256 color) tiles
#define ATTR0_SQUARE         0      // Square shape
#define ATTR0_WIDE      0x4000      // Wide shape (height < width)
#define ATTR0_TALL      0x8000      // Tall shape (height > width)

#define ATTR0_Y_MASK    0x00FF
#define ATTR0_Y_SHIFT        0
#define ATTR0_Y(n)      ((n)<<ATTR0_Y_SHIFT)

#define ATTR0_MODE_MASK 0x0300
#define ATTR0_MODE_SHIFT     8
#define ATTR0_MODE(n)   ((n)<<ATTR0_MODE_SHIFT)

#define ATTR0_SHAPE_MASK    0xC000
#define ATTR0_SHAPE_SHIFT       14
#define ATTR0_SHAPE(n)      ((n)<<ATTR0_SHAPE_SHIFT)

#define ATTR0_BUILD(y, shape, bpp, mode, mos, bld, win) \
( \
	((y)&255) | (((mode)&3)<<8) | (((bld)&1)<<10) | (((win)&1)<<11) \
	| (((mos)&1)<<12) | (((bpp)&8)<<10)| (((shape)&3)<<14) \
)


// OAM attribute 1

#define ATTR1_HFLIP     0x1000      // Horizontal flip (reg obj only)
#define ATTR1_VFLIP     0x2000      // Vertical flip (reg obj only)
// Base sizes
#define ATTR1_SIZE_8         0
#define ATTR1_SIZE_16   0x4000
#define ATTR1_SIZE_32   0x8000
#define ATTR1_SIZE_64   0xC000
// Square sizes
#define ATTR1_SIZE_8x8           0  // Size flag for  8x8 px object
#define ATTR1_SIZE_16x16    0x4000  // Size flag for 16x16 px object
#define ATTR1_SIZE_32x32    0x8000  // Size flag for 32x32 px object
#define ATTR1_SIZE_64x64    0xC000  // Size flag for 64x64 px object
// Tall sizes
#define ATTR1_SIZE_8x16          0  // Size flag for  8x16 px object
#define ATTR1_SIZE_8x32     0x4000  // Size flag for  8x32 px object
#define ATTR1_SIZE_16x32    0x8000  // Size flag for 16x32 px object
#define ATTR1_SIZE_32x64    0xC000  // Size flag for 32x64 px object
// Wide sizes
#define ATTR1_SIZE_16x8          0  // Size flag for 16x8 px object
#define ATTR1_SIZE_32x8     0x4000  // Size flag for 32x8 px object
#define ATTR1_SIZE_32x16    0x8000  // Size flag for 32x16 px object
#define ATTR1_SIZE_64x32    0xC000  // Size flag for 64x64 px object

#define ATTR1_X_MASK    0x01FF
#define ATTR1_X_SHIFT        0
#define ATTR1_X(n)      ((n)<<ATTR1_X_SHIFT)

#define ATTR1_AFF_ID_MASK   0x3E00
#define ATTR1_AFF_ID_SHIFT       9
#define ATTR1_AFF_ID(n)     ((n)<<ATTR1_AFF_ID_SHIFT)

#define ATTR1_FLIP_MASK 0x3000
#define ATTR1_FLIP_SHIFT    12
#define ATTR1_FLIP(n)   ((n)<<ATTR1_FLIP_SHIFT)

#define ATTR1_SIZE_MASK 0xC000
#define ATTR1_SIZE_SHIFT    14
#define ATTR1_SIZE(n)   ((n)<<ATTR1_SIZE_SHIFT)

#define ATTR1_BUILDR(x, size, hflip, vflip) \
	( ((x)&511) | (((hflip)&1)<<12) | (((vflip)&1)<<13) | (((size)&3)<<14) )

#define ATTR1_BUILDA(x, size, affid) \
	( ((x)&511) | (((affid)&31)<<9) | (((size)&3)<<14) )


// OAM attribute 2

#define ATTR2_ID_MASK   0x03FF
#define ATTR2_ID_SHIFT       0
#define ATTR2_ID(n)     ((n)<<ATTR2_ID_SHIFT)

#define ATTR2_PRIO_MASK 0x0C00
#define ATTR2_PRIO_SHIFT    10
#define ATTR2_PRIO(n)   ((n)<<ATTR2_PRIO_SHIFT)

#define ATTR2_PALBANK_MASK  0xF000
#define ATTR2_PALBANK_SHIFT     12
#define ATTR2_PALBANK(n)    ((n)<<ATTR2_PALBANK_SHIFT)

#define ATTR2_BUILD(id, pb, prio) \
	( ((id)&0x3FF) | (((pb)&15)<<12) | (((prio)&3)<<10) )


/* --------------------------------------------------------------
  SWI routine flags and structs
----------------------------------------------------------------- */
// SWI 0x00 SoftReset
#define ROM_RESTART     0x00        // Restart from ROM entry point.
#define RAM_RESTART     0x01        // Restart from RAM entry point.
// SWI 0x01 RegisterRamReset
#define RESET_EWRAM     0x0001      // Clear 256K on-board WRAM
#define RESET_IWRAM     0x0002      // Clear 32K in-chip WRAM
#define RESET_PALETTE   0x0004      // Clear Palette
#define RESET_VRAM      0x0008      // Clear VRAM
#define RESET_OAM       0x0010      // Clear OAM. does NOT disable OBJs!
#define RESET_REG_SIO   0x0020      // Switches to general purpose mode
#define RESET_REG_SOUND 0x0040      // Reset Sound registers
#define RESET_REG       0x0080      // All other registers
#define RESET_MEM_MASK  0x001F
#define RESET_REG_MASK  0x00E0
#define RESET_GFX       0x001C      // Clear all gfx-related memory
// SWI 0x0B CpuSet / SWI 0x0C CpuFastSet
#define CS_CPY               0      // Copy mode
#define CS_FILL        (1<<24)      // Fill mode
#define CS_CPY16             0      // Copy in halfwords
#define CS_CPY32       (1<<26)      // Copy words
#define CS_FILL32      (5<<24)      // Fill words
#define CFS_CPY         CS_CPY      // Copy words
#define CFS_FILL       CS_FILL      // Fill words
// SWI 0x0F ObjAffineSet
#define BG_AFF_OFS           2      // BgAffineDest offsets
#define OBJ_AFF_OFS          8      // ObjAffineDest offsets
// Decompression routines
#define BUP_ALL_OFS     (1<<31)
#define LZ_TYPE         0x00000010
#define LZ_SIZE_MASK    0xFFFFFF00
#define LZ_SIZE_SHIFT            8
#define HUF_BPP_MASK    0x0000000F
#define HUF_TYPE        0x00000020
#define HUF_SIZE_MASK   0xFFFFFF00
#define HUF_SIZE_SHIFT           8
#define RL_TYPE         0x00000030
#define RL_SIZE_MASK    0xFFFFFF00
#define RL_SIZE_SHIFT            8
#define DIF_8           0x00000001
#define DIF_16          0x00000002
#define DIF_TYPE        0x00000080
#define DIF_SIZE_MASK   0xFFFFFF00
#define DIF_SIZE_SHIFT           8
// Multiboot modes
#define MBOOT_NORMAL    0x00
#define MBOOT_MULTI     0x01
#define MBOOT_FAST      0x02
// SWI 10h BitUpPack
typedef struct BUP
{
    u16 src_len;    // source length (bytes)
    u8 src_bpp;     // source bitdepth (1,2,4,8)
    u8 dst_bpp;     // destination bitdepth (1,2,4,8,16,32)
    u32 dst_ofs;    // {0-30}: added offset {31}: zero-data offset flag
} BUP;

// SWI 25h MultiBoot
typedef struct
{
	u32 reserved1[5];
	u8  handshake_data;
	u8  padding;
	u16 handshake_timeout;
	u8  probe_count;
	u8  client_data[3];
	u8  palette_data;
	u8  response_bit;
	u8  client_bit;
	u8  reserved2;
	u8  *boot_srcp;
	u8  *boot_endp;
	u8  *masterp;
	u8  *reserved3[3];
	u32 system_work2[4];
	u8  sendflag;
	u8  probe_target_bit;
	u8  check_wait;
	u8  server_type;
} MultiBootParam;


#ifdef __cplusplus
};
#endif

#endif // GBA_MEM_DEF_H
