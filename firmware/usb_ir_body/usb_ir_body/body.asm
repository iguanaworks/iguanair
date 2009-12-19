;**************************************************************************
; * body.asm ***************************************************************
; **************************************************************************
; *
; * This implements the basic firmware body code.
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

VERSION_ID_LOW:  equ 0x07 ; firmware version ID low byte (code body)

export get_feature_list

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
    ; enable output for tx on all channels
    call get_feature_list
    and A, HAS_LEDS | HAS_BOTH | HAS_SOCKETS | SLOT_DEV
    jnz bm_output_channels
    and REG[OLD_TX_BANK], ~OLD_TX_MASK
	mov REG[P06CR], 0x01 ; only had 1 channel back then
    jmp bm_output_enabled
   bm_output_channels:
    or REG[TX_BANK], TX_MASK
    mov REG[P12CR], 0x01 ; channel NA / 0
    mov REG[P13CR], 0x01 ; channel NA / 1
    mov REG[P14CR], 0x01 ; channel 0  / 2
    mov REG[P15CR], 0x01 ; channel 1  / 3
    mov REG[P16CR], 0x01 ; channel 2  / 4
    mov REG[P17CR], 0x01 ; channel 3  / 5
   bm_output_enabled:
    ; pins will be set correctly by the rx_reset below

    ; configure capture
    mov REG[RX_PIN_CR], 0b00000010 ; configure port pin: pullup enabled
    mov REG[TMRCLKCR],  0b11001111 ; timer to 3MHz
    mov REG[TMRCR],     0b00001000 ; 16 bit mode
    mov REG[TCAPINTE],  0b00000011 ; configure rise and fall interrupts
    ; but don't actually enable the interrupts yet

    ; clear the initialize flag
    and [loader_flags], ~FLAG_BODY_INIT

  bm_was_initialized:
    mov A, [loader_flags]
    and A, FLAG_BODY_RESET
    jz bm_was_reset

    ; SOFT RESET
    and [rx_flags], ~RX_ON_FLAG ; rx starts in the off state
    lcall pins_reset            ; clear GPIO pin state

    ; clear the soft reset flag
    and [loader_flags], ~FLAG_BODY_RESET

  bm_was_reset:
    mov A, [loader_flags]
    and A, FLAG_BODY_BUFCLR
    jz bm_buffer_okay
    ; make sure receiver state matches
    lcall rx_reset

    ; clear the bufclr flag
    and [loader_flags], ~FLAG_BODY_BUFCLR

  bm_buffer_okay:
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
    cmp A, CTL_RESEND
    jz resend_body

    ; pin functions
    cmp A, CTL_GETPINCONFIG
    jz get_pin_config_body
    cmp A, CTL_SETPINCONFIG
    jz set_pin_config_body
    cmp A, CTL_GETPINS
    jz get_pins_body
    cmp A, CTL_SETPINS
    jz set_pins_body
    cmp A, CTL_PINBURST
    jz pin_burst_body

    ; misc functions
    cmp A, CTL_EXECUTE
    jz execute_body
    cmp A, CTL_REPEATER
    jz repeater_body
	; the "no usb" action is to fall into repeater mode
    cmp A, CTL_NOUSB
    jz start_repeating

    ; that's everything we handle
    jmp bm_ret

  bm_ack_then_ret:
    ; send ack
    mov X, CTL_BASE_SIZE
    lcall write_control
  bm_ret:
    ret                ; return to main recv

body_loop_body:
    ; check for receive overflow
    mov A, [rx_flags]
    and A, RX_OVERFLOW_FLAG
    jz no_overflow
    ; send back an overflow packet
    mov [control_pkt + CCODE], CTL_OVERRECV
    mov X, CTL_BASE_SIZE
    lcall write_control
    lcall rx_reset

  no_overflow:
    lcall write_signal ; write back received data
    ret                ; return to main loop

get_features_body:
    mov [control_pkt + CCODE], CTL_GETFEATURES
    call get_feature_list
    mov [control_pkt + CDATA + 0], A
    ; the numbers (cycle counts) are taken from the ir.asm code
    mov [control_pkt + CDATA + 1],5 + 5 + 7 + 6 + 6 + 7 + (5 + 7) + (5 + 7) + 5
    mov X, CTL_BASE_SIZE + 2
    lcall write_control
    jmp bm_ret

get_buf_size_body:
    mov [control_pkt + CCODE], CTL_GETBUFSIZE
    mov [control_pkt + CDATA], BUFFER_SIZE
    mov X, CTL_BASE_SIZE + 1
    lcall write_control
    jmp bm_ret

recv_on_body:
    or [rx_flags], RX_ON_FLAG ; note that rx should be on
    lcall rx_reset            ; make rx state actually match
    jmp bm_ack_then_ret

recv_off_body:
    and [rx_flags], ~RX_ON_FLAG ; note that rx should be off
    lcall rx_reset              ; make rx state actually match
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

    ; send the overflow packet
    mov X, CTL_BASE_SIZE + 1
    lcall write_control
    jmp bm_ret

; send the data that is already in the buffer 
resend_body:
    lcall rx_disable      ; disable timer interrupt, clear rx state
    lcall transmit_code   ; transmit
    lcall rx_reset        ; turn rx on if necessary
    jmp bm_ack_then_ret

get_pin_config_body:
    lcall get_pin_config
    jmp bm_ret

set_pin_config_body:
    lcall set_pin_config
    jmp bm_ret

; put the pins in the control packet and send it back
get_pins_body:
    lcall get_pins
    mov X, CTL_BASE_SIZE + 2
    lcall write_control
    jmp bm_ret

; use control packet pin settings then ack
set_pins_body:
    lcall set_pins
    jmp bm_ack_then_ret

; send a burst of activity to the GPIO pins
pin_burst_body:
    ; disable rx since the burst timing would interfer
    lcall rx_disable

    lcall read_buffer ; get the block to read
    lcall pin_burst   ; set pins in a burst

    jmp bm_ack_then_ret

; default to executing the final page since nothing else is easy
execute_body:
    lcall 0x1FC0
    jmp bm_ret

repeater_body:
    ; send the ack
    mov X, CTL_BASE_SIZE
    lcall write_control

	; call the repeater
  start_repeating:
	lcall ir_repeater
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
