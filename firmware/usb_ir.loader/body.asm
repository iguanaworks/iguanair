;**************************************************************************
; * body.asm ***************************************************************
; **************************************************************************
; *
; * 
; Use this file as a starting point for applications.  Just populate the
;   body_main function.
; *
; * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
; * Author: Joseph Dunn <jdunn@iguanaworks.net>
; *
; * Distributed under the GPL version 2.
; * See LICENSE for license details.
; */


include "loader.inc"

VERSION_ID_LOW:  equ 0x00 ; firmware version ID low byte (code body version)

; FUNCTION: body_main
;  API used by the loader to call into the body.
;  pre: A contains a control code
;       ctl_packet contains the whole control packet
body_main:
    ret

body_loop_body:
    ret

body_tcap_int_handler:
    reti

body_twrap_int_handler: 
    reti

; we've discovered that we need a timeout on long reads, so co-opt
; timer wrap to knock down a dedicated counter
body_twrap_int_base:
  ; if we're in the middle of a read then increment the timer wrap count
    mov A, [loader_flags]
    and A, FLAG_READING
    jz btib_jmp_to_body
    inc [tmp3]
    reti

  ; otherwise jump to the body twrap handler
  btib_jmp_to_body:
    ljmp body_twrap_int_handler

; implementation of the body jump table located at BODY_JUMPS
; Do not modify this code unless you KNOW what you are doing!
area bodyentry (ROM, ABS, CON)
org body_version
    mov A, VERSION_ID_LOW
    ret

org body_handler
    jmp body_main

org body_loop
    jmp body_loop_body

org body_tcap_int
    jmp body_tcap_int_handler

org body_twrap_int
    jmp body_twrap_int_base
