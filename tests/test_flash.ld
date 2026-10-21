/* 【19 纯C测试内存布局：不烧入真实芯片】
 * 阅读导航：docs/代码阅读地图.md。
 */
ENTRY(test_main)
MEMORY { FLASH(rx): ORIGIN = 0x08000000, LENGTH = 64K
         RAM(rw): ORIGIN = 0x20000000, LENGTH = 20K }
SECTIONS { .text : { *(.text*) *(.rodata*) } >FLASH
 .ARM.exidx : { *(.ARM.exidx*) } >FLASH
 .data : { *(.data*) } >RAM AT>FLASH
 .bss (NOLOAD) : { *(.bss*) *(COMMON) } >RAM }
