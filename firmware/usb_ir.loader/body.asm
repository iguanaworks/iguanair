; Use this file as a starting point for applications.  Just populate the
;   body_main function.
include "loader.inc"

VERSION_ID_LOW:  equ 0x00 ; firmware version ID low byte (code body)

; FUNCTION: body_main
;  API used by the loader to call into the body.
;  pre: A contains a control code
;       ctl_packet contains the whole control packet
body_main:
    ret

body_tcap_int_handler:
    reti

body_twrap_int_handler: 
    reti
        
; implementation of the body jump table located at BODY_JUMPS
; Do not modify this code unless you KNOW what you are doing!
area bodyentry (ROM, ABS, CON)
org body_handler
    jmp body_main

org body_version
    mov A, VERSION_ID_LOW
    ret

org body_tcap_int
    jmp body_tcap_int_handler

org body_twrap_int
    jmp body_twrap_int_handler
