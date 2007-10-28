;**************************************************************************
; * body.asm ***************************************************************
; **************************************************************************
; *
; * TODO: DESCRIBE AND DOCUMENT THIS FILE
; *
; * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
; * Author: Joseph Dunn <jdunn@iguanaworks.net>
; *
; * Distributed under the GPL version 2.
; * See LICENSE for license details.
; */

include "m8c.inc"    ; part specific constants and macros
include "loader.inc"
include "body.inc"

VERSION_ID_LOW:  equ 0x01 ; firmware version ID low byte (code body)


; FUNCTION: body_main
;  API used by the loader to call into the body.
;  pre: A contains a control code
;       ctl_packet contains the whole control packet
body_main:
    ; save the control code
    mov [tmp1], A

    ; check if we need to initialize or reset
    mov A, [loader_flags]
    and A, FLAG_BODY_INIT
    jz bm_was_initialized

    ;  INITIALIZE
    ; enable output for tx on all four channels
    mov REG[P14CR], 0x01 ; channel 0
    mov REG[P15CR], 0x01 ; channel 1
    mov REG[P16CR], 0x01 ; channel 2
    mov REG[P17CR], 0x01 ; channel 3
    ; but make sure the pins are set high because the LEDs are active LOW
    mov REG[TX_BANK], TX_MASK

    ; configure capture
    mov REG[RX_PIN_CR], 0b00000010 ; configure port pin: pullup enabled
    mov REG[TMRCLKCR],  0b11001111 ; timer to 3MHz
    mov REG[TMRCR],     0b00001000 ; 16 bit mode
    mov REG[TCAPINTE],  0b00000011 ; configure rise and fall interrupts
    ; but don't actually enable the interrupts yet

    ; clear the initialize flag
    mov A, [loader_flags]
    and A, ~FLAG_BODY_INIT
    mov [loader_flags], A

  bm_was_initialized:
    mov A, [loader_flags]
    and A, FLAG_BODY_RESET
    jz bm_was_reset

    ; SOFT RESET
    mov [rx_on], 0   ; rx starts in the off state
    lcall rx_reset   ; make sure receiver state matches
;    lcall pins_reset     ; clear GPIO pin state

    ; clear the soft reset flag
    mov A, [loader_flags]
    and A, ~FLAG_BODY_RESET
    mov [loader_flags], A



  bm_was_reset:
    ; reload the control code
    mov A, [tmp1]

    cmp A, CTL_GETFEATURES
    jz get_features_body

    cmp A, CTL_GETBUFSIZE
    jz get_buf_size_body

    ; receive functions
    cmp A, CTL_RECVON
    jz recv_on_body
    cmp A, CTL_RECVOFF
    jz recv_off_body

    ; send functions
    cmp A, CTL_SEND
    jz send_body
    cmp A, CTL_SETCHANNELS
    jz set_channels_body
    cmp A, CTL_GETCHANNELS
    jz get_channels_body

    ; pin functions
    cmp A, CTL_GETPINCONFIG
    jz get_pin_config_body
    cmp A, CTL_SETPINCONFIG
    jz set_pin_config_body
    cmp A, CTL_GETPINS
    jz get_pins_body
    cmp A, CTL_SETPINS
    jz set_pins_body
    cmp A, CTL_BULKPINS
    jz bulk_pins_body

    ; misc functions
    cmp A, CTL_GETID
    jz getid_body
    cmp A, CTL_EXECUTE
    jz execute_body

    ; that's everything we handle
    jmp bm_ret

  bm_ack_then_ret:
    ; send ack
    mov A, CTL_BASE_SIZE
    lcall write_control
  bm_ret:
    ret                ; return to main recv

body_loop_body:
    lcall write_signal ; write back received data
    ret                ; return to main loop

get_features_body:
    mov [control_pkt + CCODE], CTL_GETFEATURES
    call get_feature_list
    mov [control_pkt + CDATA], A
    mov A, CTL_BASE_SIZE + 1
    lcall write_control
    jmp bm_ret

get_buf_size_body:
    mov [control_pkt + CCODE], CTL_GETBUFSIZE
    mov [control_pkt + CDATA], BUFFER_SIZE
    mov A, CTL_BASE_SIZE + 1
    lcall write_control
    jmp bm_ret

recv_on_body:
    mov [rx_on], 0x1 ; note that rx should be on
    lcall rx_reset   ; make rx state actually match rx_on
    jmp bm_ack_then_ret

recv_off_body:
    mov [rx_on], 0x0 ; note that rx should be off
    lcall rx_disable ; make rx state actually match rx_on
    jmp bm_ack_then_ret

send_body:
    lcall rx_disable      ; disable timer interrupt, clear rx state
    lcall read_buffer     ; receive the code--returns 0 if read overflow
    jz send_body_overflow ; error on overflow
    lcall transmit_code   ; transmit
    lcall rx_reset        ; turn rx on if necessary
    jmp bm_ack_then_ret

  send_body_overflow:
    lcall transmit_code ; transmit anyway
    lcall rx_reset      ; turn rx on if necessary

    ; send overflow instead of ack
    mov [control_pkt + CCODE], CTL_OVERSEND

    ; send ack
    mov A, CTL_BASE_SIZE + 1
    lcall write_control
    jmp bm_ret

set_channels_body:
    jmp bm_ret

get_channels_body:
    jmp bm_ret

get_pin_config_body:
    jmp bm_ret

set_pin_config_body:
    jmp bm_ret

get_pins_body:
    mov [control_pkt + CCODE], CTL_GETPINS
    mov A, CTL_BASE_SIZE + 2
    lcall write_control
    jmp bm_ret

set_pins_body:
    ; disable ints to avoid race conditions on the port
    and F, 0xFE ; clear global interrupt bit

    ; load the P0 value into A
    mov A, [control_pkt + CDATA + 0]
    ; write out value
    mov REG[P0DATA], A

    ; load the P1 value into A
    mov A, [control_pkt + CDATA + 1]
    ; mask off the USB lines so the user cannot break that
    and A, 0xFC
    ; write out value
    mov REG[P1DATA], A

    ; re-enable global interrupts
    or  F, 0x1

    jmp bm_ret

bulk_pins_body:
    jmp bm_ret

getid_body:
    jmp bm_ret

execute_body:
    jmp bm_ret



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
    jmp tcap_int

org body_twrap_int
    jmp twrap_int

org BODY_JUMPS + 60
  get_feature_list:
    mov A, 0
    ret
