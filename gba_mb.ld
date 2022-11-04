OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY {
	wram	: ORIGIN = 0x02000000, LENGTH = 262144
	iwram	: ORIGIN = 0x03000000, LENGTH = 32768
	vram	: ORIGIN = 0x06000000, LENGTH = 98304
	rom	: ORIGIN = 0x08000000, LENGTH = 167772160
}



__text_start	=	ORIGIN(wram);
__eheap_end	=	ORIGIN(wram) + LENGTH(wram);
__iwram_start	=	ORIGIN(iwram);
__iwram_top	=	ORIGIN(iwram) + LENGTH(iwram);;

__irq_flags	=	__iwram_top - 0x8;
__isr_table	=	__iwram_top - 0x100 - 0x40;
__sp_svc	=	__iwram_top - 0x20;
__sp_irq	=	__iwram_top - 0x60;
__sp_usr	=	__iwram_top - 0x140;

SECTIONS
{
	. = __text_start;
	. = __text_start;
	.crt0 :
	{
		KEEP (*(.crt0))
		. = ALIGN(4);
	} >wram =0xff

	.init :
	{
		KEEP (*(SORT_NONE(.init)))
	} >wram

	.plt :
	{
		*(.plt)
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >wram


	.text  ALIGN (4):
	{
		*(EXCLUDE_FILE (*.iwram*) .text*)
		*(.gnu.linkonce.t.*)
		KEEP (*(.text.*personality*))
		/* .gnu.warning sections are handled specially by elf32.em.  */
		*(.gnu.warning)
		*(.glue_7t) *(.glue_7) *(.vfp11_veneer)
		. = ALIGN(4);  /* REQUIRED. LD is flaky without it. */
	} >wram = 0xff

	__text_end = .;
	.fini           :
	{
		KEEP (*(.fini))
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >wram =0

	.rodata :
	{
		*(.rodata)
		*all.rodata*(*)
		*(.roda)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
		SORT(CONSTRUCTORS)
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >wram = 0xff

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >wram
   __exidx_start = .;
  .ARM.exidx   : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } >wram
   __exidx_end = .;
  /* Ensure the __preinit_array_start label is properly aligned.  We
     could instead move the label definition inside the section, but
     the linker would then create the section even if it turns out to
     be empty, which isn't pretty.  */
  . = ALIGN(32 / 8);
  PROVIDE (__preinit_array_start = .);
  .preinit_array     : { KEEP (*(.preinit_array)) } >wram = 0xff
  PROVIDE (__preinit_array_end = .);
  PROVIDE (__init_array_start = .);
  .init_array     : { KEEP (*(.init_array)) } >wram = 0xff
  PROVIDE (__init_array_end = .);
  PROVIDE (__fini_array_start = .);
  .fini_array     : { KEEP (*(.fini_array)) } >wram = 0xff
  PROVIDE (__fini_array_end = .);
	.ctors :
	{
		/*	gcc uses crtbegin.o to find the start of the constructors, so
			we make sure it is first.  Because this is a wildcard, it
			doesn't matter if the user does not actually link against
			crtbegin.o; the linker won't look for a file to match a
			wildcard.  The wildcard also means that it doesn't matter which
			directory crtbegin.o is in.  */
		KEEP (*crtbegin.o(.ctors))
		KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
		KEEP (*(SORT(.ctors.*)))
		KEEP (*(.ctors))
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >wram = 0

	.dtors :
	{
		KEEP (*crtbegin.o(.dtors))
		KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
		KEEP (*(SORT(.dtors.*)))
		KEEP (*(.dtors))
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >wram = 0

	.jcr            : { KEEP (*(.jcr)) } >wram
	.eh_frame :
	{
		KEEP (*(.eh_frame))
	. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >wram = 0

	.gcc_except_table :
	{
		*(.gcc_except_table)
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
	} >wram = 0

	__iwram_lma = .;

	.iwram __iwram_start : AT (__iwram_lma)
	{
		__iwram_start__ = ABSOLUTE(.) ;
		*(.iwram)
		*iwram.*(.text*)
		. = ALIGN(4);   /* REQUIRED. LD is flaky without it. */
		__iwram_end__ = ABSOLUTE(.) ;
		__iwram_size__ = ABSOLUTE(__iwram_end__ - __iwram_start__) ;
	} >iwram = 0xff

	__data_lma = __iwram_lma + SIZEOF(.iwram) ;

	.bss ALIGN(4) (NOLOAD):
	{
		__bss_start__ = ABSOLUTE(.);
		*(.dynbss)
		*(.gnu.linkonce.b*)
		*(.bss*)
		*(COMMON)
		. = ALIGN(4);    /* REQUIRED. LD is flaky without it. */
		__bss_end__ = ABSOLUTE(.) ;
		__bss_size__ = ABSOLUTE(__bss_end__ - __bss_start__) ;
	}

	.data ALIGN(4) : AT (__data_lma)
	{
		__data_start__ = ABSOLUTE(.);
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		CONSTRUCTORS
		. = ALIGN(4);  /* REQUIRED. LD is flaky without it. */
		__data_end__  =  ABSOLUTE(.);
		__data_size__ = ABSOLUTE(__data_end__ - __data_start__) ;
	} >iwram = 0xff

	__iwram_overlay_lma = __data_lma + SIZEOF(.data);

	PROVIDE (edata = .);
	__iwram_overlay_start = . ;

	OVERLAY ALIGN(4) : NOCROSSREFS AT (__iwram_overlay_lma)
	{
	   .iwram0 { *(.iwram0) . = ALIGN(4);}
	   .iwram1 { *(.iwram1) . = ALIGN(4);}
	   .iwram2 { *(.iwram2) . = ALIGN(4);}
	   .iwram3 { *(.iwram3) . = ALIGN(4);}
	   .iwram4 { *(.iwram4) . = ALIGN(4);}
	   .iwram5 { *(.iwram5) . = ALIGN(4);}
	   .iwram6 { *(.iwram6) . = ALIGN(4);}
	   .iwram7 { *(.iwram7) . = ALIGN(4);}
	   .iwram8 { *(.iwram8) . = ALIGN(4);}
	   .iwram9 { *(.iwram9) . = ALIGN(4);}
	} >iwram = 0xff

	__wram_lma = LOADADDR(.iwram0) + SIZEOF(.iwram0)+SIZEOF(.iwram1)+SIZEOF(.iwram2)+SIZEOF(.iwram3)+SIZEOF(.iwram4)+SIZEOF(.iwram5)+SIZEOF(.iwram6)+SIZEOF(.iwram7)+SIZEOF(.iwram8)+SIZEOF(.iwram9);

	__iwram_overlay_end = __wram_lma ;

	/* v1.3 */
	__wram_start =  __wram_lma ;

	.wram __wram_start : AT (__wram_lma)
	{
		*(.wram)
		. = ALIGN(4);  /* REQUIRED. LD is flaky without it. */
		__wram_end = ABSOLUTE(.);
	} >wram = 0xff

	__wram_overlay_lma = __wram_lma + SIZEOF(.wram);

	.sbss ALIGN(4)(NOLOAD):
 	{
		__sbss_start__ = ABSOLUTE(.);
		*(.sbss)
		. = ALIGN(4);
		__sbss_end__  = ABSOLUTE(.);
		__sbss_size__ = ABSOLUTE(__sbss_end__ - __sbss_start__) ;
		__end__ = ABSOLUTE(.);
		__eheap_start = ABSOLUTE(.);
	}

	OVERLAY ALIGN(4): NOCROSSREFS AT (__wram_overlay_lma)
	{
		.wram0 { *(.wram0) . = ALIGN(4);}
		.wram1 { *(.wram1) . = ALIGN(4);}
		.wram2 { *(.wram2) . = ALIGN(4);}
		.wram3 { *(.wram3) . = ALIGN(4);}
		.wram4 { *(.wram4) . = ALIGN(4);}
		.wram5 { *(.wram5) . = ALIGN(4);}
		.wram6 { *(.wram6) . = ALIGN(4);}
		.wram7 { *(.wram7) . = ALIGN(4);}
		.wram8 { *(.wram8) . = ALIGN(4);}
		.wram9 { *(.wram9) . = ALIGN(4);}
	} >wram = 0xff
	__wram_overlay_end  = ABSOLUTE(.);

	__eheap_start = __wram_overlay_end ;

	_end = __wram_overlay_end;
	__end__ = __wram_overlay_end;
	__rom_end__ = __wram_overlay_end;

	/* Stabs debugging sections.  */
	.stab 0 : { *(.stab) }
	.stabstr 0 : { *(.stabstr) }
	.stab.excl 0 : { *(.stab.excl) }
	.stab.exclstr 0 : { *(.stab.exclstr) }
	.stab.index 0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment 0 : { *(.comment) }
	/* DWARF debug sections.
	   Symbols in the DWARF debugging sections are relative to the beginning
	   of the section so we begin them at 0.  */
	/* DWARF 1 */
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }
	/* GNU DWARF 1 extensions */
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }
	/* DWARF 1.1 and DWARF 2 */
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	/* DWARF 2 */
	.debug_info     0 : { *(.debug_info) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
	/* SGI/MIPS DWARF 2 extensions */
	.debug_weaknames 0 : { *(.debug_weaknames) }
	.debug_funcnames 0 : { *(.debug_funcnames) }
	.debug_typenames 0 : { *(.debug_typenames) }
	.debug_varnames  0 : { *(.debug_varnames) }
	.stack 0x80000 : { _stack = .; *(.stack) }
	/* These must appear regardless of  .  */
}
