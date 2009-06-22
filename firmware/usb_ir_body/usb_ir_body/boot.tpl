;@Id: boot.tpl#876 @
;=============================================================================
;  FILENAME:   boot.asm
;  VERSION:    4.08
;  DATE:       28 June 2007
;
;  DESCRIPTION:
;  Based on the boot.tpl provided in the loader file, but heavily stripped
;  down.
;
; NOTES:
; PSoC Designer's Device Editor uses a template file, BOOT.TPL, located in
; the project's root directory to create BOOT.ASM. Any changes made to 
; BOOT.ASM will be  overwritten every time the project is generated; therefore
; changes should be made to BOOT.TPL not BOOT.ASM. Care must be taken when
; modifying BOOT.TPL so that replacement strings (such as @PROJECT_NAME)
; are not accidentally modified.
;
;=============================================================================

include "loader.inc"

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

;---------------------------------------------
;         Begin RAM area usage
;---------------------------------------------
    AREA data              (RAM, REL, CON)   ; initialized RAM
__data_start:

    AREA virtual_registers (RAM, REL, CON)   ; Temp vars of C compiler
    AREA InterruptRAM      (RAM, REL, CON)   ; Interrupts, on Page 0
    AREA bss               (RAM, REL, CON)   ; general use
  BLK BODY_SKIP_BYTES - 3
__bss_start:
