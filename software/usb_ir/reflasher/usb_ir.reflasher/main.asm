;**************************************************************************
; * main.asm ***************************************************************
; **************************************************************************
; *
; * 
; * Main program
; *
; * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
; * Original Author: Brian Shucker <brian@iguanaworks.net>
; * Maintainer: Joseph Dunn <jdunn@iguanaworks.net>
; *
; * Distributed under the GPL version 2.
; * See LICENSE for license details.
; */

; define the start of relocatable code to move this as far back as possible.
; Project -> Settings -> Linker -> Relocatable start address:
;          '%x' % (8192 - (CODESIZE / 64 + 1 + 1) * 64)
; NOTE: we will overwrite the send function at 0x1c00!

include "m8c.inc"         ; part specific constants and macros
include "memory.inc"      ; Constants & macros for SMM/LMM and Compiler
include "PSoCAPI.inc"     ; PSoC API definitions for all User Modules
include "loader.inc"

VERSION_ID_HIGH:  EQU 0xFF ; firmware version ID high byte (bootloader version)
MAGIC_WRITE_BYTE: EQU 0x42 ; random magic value (meaning of life, but hex)

; exported functions
export _main
export soft_reset

AREA text
_main:
    or [loader_flags], FLAG_BODY_INIT ; set the body init flag

    mov A, 0        ; put arg 0 for USB_start
    lcall USB_Start ; enable USB device
    or  F, 0x1      ; enable global interrupts

; wait for usb enumeration
config_loop:
	lcall USB_bGetConfiguration
	jz config_loop ; if return val was zero, try again

; now we're connected
soft_reset:
	and [loader_flags], ~FLAG_HALTED    ; clear halt flag that Linux sets
	or  [loader_flags], FLAG_BODY_RESET ; set the body reset flag

	; clear the IN endpoint by sending 0-byte packet
	mov [USB_APIEPNumber], 0x1 ; set to endpoint 1
	mov [USB_APICount], 0      ; set packet size
	mov X, buffer              ; put packet address into X
	lcall USB_XLoadEP          ; send packet

	; enable OUT endpoint
	MOV A, OUT
	lcall USB_EnableEP

main_loop:
    ; halt condition 
	mov A, [loader_flags] ; check for halt condition
	and A, FLAG_HALTED
	jnz soft_reset        ; go to known state after halt

	; check for data from host
	lcall check_read_body ; see if there is a transmission from the host
	jnz main_recv

	; only call the body loop if the body has successfully initialized
	mov A, [loader_flags] ; check for halt condition
	and A, FLAG_BODY_INIT
	jnz main_loop ; repeat main loop if body has not init'd

	jmp main_loop ; jump back to main loop now that body has run

; there is a transmission, so receive and handle it
main_recv:
	; get control packet
	lcall read_control_body

	;at this point, A contains control code
	jz main_loop          ; null control code, just ignore
	cmp A, CTL_GETVERSION ; get version
	jz main_getversion
	cmp A, CTL_WRITEBLOCK ; program a block of flash
	jz main_prog
	cmp A, CTL_RESET      ; reset command
	jz main_reset

	jmp main_loop

main_getversion:
	; send control packet with version id
	mov [control_pkt + CCODE], CTL_GETVERSION
	mov [control_pkt + CDATA], 0
	; high byte is defined in here
	mov [control_pkt + CDATA + 1], VERSION_ID_HIGH
	mov X, CTL_BASE_SIZE + 2
	lcall write_control_body
	jmp main_loop

; compute the actual checksum for a page
sum_page:
    ; read a page of flash
    mov A, 0x01 ; read block code
    call exec_ssc

    ; compute a very simple checksum
    mov X, FLASH_BLOCK_SIZE
    mov [tmp1], 0
    mov [tmp2], 0
  mcsum_loop:
    dec X
    mov A, [X + buffer]
    add [tmp2], A ; add to the low byte
    adc [tmp1], 0 ; sum the overflows in the high byte
    mov A, X
    jnz mcsum_loop
    ret

main_prog:
	mov [tmp3], [control_pkt + CDATA]           ; save the flash block address
	mov [control_pkt + CDATA], FLASH_BLOCK_SIZE ; set up to read right number of bytes
	lcall read_buffer_body                      ; read the block

	; protect the first 48 pages with a special code
	cmp [control_pkt + CDATA + 1], MAGIC_WRITE_BYTE
    jz write_page
    cmp [tmp3], 48
    jnc write_page
    ; throw an error
	mov [control_pkt + CCODE], CTL_INVALID_ARG
	mov X, CTL_BASE_SIZE
	lcall write_control_body
	jmp main_prog_done

	; ack the receive
  write_page:
    ; disable ints to avoid race condition on page 0
    and F, 0xFE

    ; do a block erase
    mov A, 0x03 ;erase block code
    call exec_ssc

    ; now set up the parameter block for the SROM call
    mov A, 0x02 ;write block code
    call exec_ssc

    ; re-enable interrupts
    or F, 0x01

    ; get a checksum done
    call sum_page
    mov [control_pkt + CCODE], CTL_WRITEBLOCK
    mov [control_pkt + CDATA + 0], [tmp1]
    mov [control_pkt + CDATA + 1], [tmp2]
    mov X, CTL_BASE_SIZE + 2
    lcall write_control_body

  main_prog_done:
    jmp main_loop ; read the next packet

main_reset:
	lcall USB_Stop ; do this first, or we loop sending 0 length packets....
	mov [tmp3], 0x0
	mov A, 0 ;reset code
	call exec_ssc

; pre: tmp3 holds the block index
;      A holds the ssc code
exec_ssc:
	; store the ssc code
	mov [tmp1], A

	;now set up the parameter block for the SROM call
	mov [KEY1], 0x3A
	mov X, SP             ; get stack pointer
	mov A, X
	add A, 3              ; just following directions from datasheet
	mov [KEY2], A
	mov [BLOCKID], [tmp3] ; set which flash block to write
	mov [POINTER], buffer ; write data to the buffer
	mov [CLOCK], 0x00     ; guessing at the right clock divider
	mov [DELAY], 0xAC     ; a guess, datasheet says use 0x56 for 12MHz
	mov A, [tmp1]         ; load the ssc code
	ssc                   ; execute it
	ret
