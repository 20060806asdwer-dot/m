/* 【23 芯片内存布局：程序62KiB，启动日志2KiB】
 * 阅读导航：docs/代码阅读地图.md。
 */
/* Genuine F103C8: 64 KiB flash, 20 KiB SRAM. Last 2 KiB reserved for boot journal. */
ENTRY(Reset_Handler)
MEMORY { FLASH(rx): ORIGIN = 0x08000000, LENGTH = 62K
         RAM(xrw): ORIGIN = 0x20000000, LENGTH = 20K }
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 0x800;
PHDRS { flash PT_LOAD FLAGS(5); ram PT_LOAD FLAGS(6); }
SECTIONS {
 .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } >FLASH :flash
 .text : { . = ALIGN(4); *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .; } >FLASH
 .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } >FLASH
 .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } >FLASH
 .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } >FLASH
 .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } >FLASH
 _sidata = LOADADDR(.data);
 .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } >RAM AT>FLASH :ram
 .bss (NOLOAD) : { . = ALIGN(8); _sbss = .; __bss_start__ = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; __bss_end__ = .; } >RAM
 .heap_stack (NOLOAD) : { . = ALIGN(8); end = .; _end = .; __end__ = .; _heap_start = .; . += 512; _heap_end = .; . += _Min_Stack_Size; . = ALIGN(8); } >RAM
 .ARM.attributes 0 : { *(.ARM.attributes) }
 ASSERT(_heap_end + _Min_Stack_Size <= _estack, "Not enough stack space")
}
