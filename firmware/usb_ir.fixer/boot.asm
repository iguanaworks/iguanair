;-----------------------------------------------------------------------------
; RAM segments for C CONST, static & global items
;-----------------------------------------------------------------------------
    AREA lit
__lit_start:

    AREA idata
__idata_start:

    AREA func_lit
__func_lit_start:

    AREA psoc_config(ROM,REL,CON)
__psoc_config_start:

    AREA UserModules(ROM,REL,CON)
__usermodules_start:

    AREA gpio_isr(ROM,REL,CON)
__gpio_isr_start:

;---------------------------------------------
;         CODE segment for general use
;---------------------------------------------
    AREA text(ROM,REL,CON)
__text_start:
ljmp fixer_start

;---------------------------------------------
;         Begin RAM area usage
;---------------------------------------------
    AREA data              (RAM, REL, CON)   ; initialized RAM
__data_start:

    AREA virtual_registers (RAM, REL, CON)   ; Temp vars of C compiler
    AREA InterruptRAM      (RAM, REL, CON)   ; Interrupts, on Page 0
    AREA bss               (RAM, REL, CON)   ; general use
__bss_start:
