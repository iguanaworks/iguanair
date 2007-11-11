;**************************************************************************
; * pins.asm ***************************************************************
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

export pins_reset
export get_pin_config
export set_pin_config
export get_pins
export set_pins
export pin_burst

has_gpios:
    call get_feature_list
    and A, HAS_LEDS | HAS_BOTH | HAS_SOCKETS
    jnz has_not
    mov A, 1
    jmp has_done
  has_not:
    mov [control_pkt + CCODE], CTL_INVALID_ARG
    mov X, CTL_BASE_SIZE
    lcall write_control
    mov A, 0
  has_done:
    ret

pins_reset:
    call has_gpios
    jz reset_done

    ; clear the data registers
    and REG[P0DATA], 0xF0
    and REG[P1DATA], 0x87

    ; clear the config registers for pins we use
    ; port 0, pins 0-3
    mov REG[P00CR], 0
    mov REG[P01CR], 0
    mov REG[P02CR], 0
    mov REG[P03CR], 0

    ; port 1, pins 3-6
    mov REG[P13CR], 0
    mov REG[P14CR], 0
    mov REG[P15CR], 0
    mov REG[P16CR], 0
  reset_done:
    ret

get_pin_config:
    call has_gpios
    jz get_pin_config_done

    ; ack the request
    mov X, CTL_BASE_SIZE
    lcall write_control

    mov A, REG[P00CR]   ; read the pin control register
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov [control_pkt + 0], A ; store pin config byte

    mov A, REG[P01CR]   ; read the pin control register
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov [control_pkt + 1], A ; store pin config byte

    mov A, REG[P02CR]   ; read the pin control register
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov [control_pkt + 2], A ; store pin config byte

    mov A, REG[P03CR]   ; read the pin control register
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov [control_pkt + 3], A ; store pin config byte

    mov A, REG[P13CR]   ; read the pin control register
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov [control_pkt + 4], A ; store pin config byte

    mov A, REG[P14CR]   ; read the pin control register
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov [control_pkt + 5], A ; store pin config byte

    mov A, REG[P15CR]   ; read the pin control register
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov [control_pkt + 6], A ; store pin config byte

    mov A, REG[P16CR]   ; read the pin control register
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov [control_pkt + 7], A ; store pin config byte

    ; return the config data in a second packet
    mov X, PACKET_SIZE
    mov A, control_pkt
    lcall write_packet

  get_pin_config_done:
    ret

set_pin_config:
    call has_gpios
    jz set_pin_config_done

  wait_data_ready:
    lcall check_read ; see if there is a transmission from the host
    jz wait_data_ready

    ; read the new configuration
    mov X, PACKET_SIZE
    mov A, control_pkt
    lcall read_packet

    mov X, PACKET_SIZE
    mov A, control_pkt
    lcall write_packet

    mov A, [control_pkt + 0] ; get pin config byte
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov REG[P00CR], A   ; set the pin control register

    mov A, [control_pkt + 1] ; get pin config byte
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov REG[P01CR], A   ; set the pin control register

    mov A, [control_pkt + 2] ; get pin config byte
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov REG[P02CR], A   ; set the pin control register

    mov A, [control_pkt + 3] ; get pin config byte
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov REG[P03CR], A   ; set the pin control register

    mov A, [control_pkt + 4] ; get pin config byte
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov REG[P13CR], A   ; set the pin control register

    mov A, [control_pkt + 5] ; get pin config byte
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov REG[P14CR], A   ; set the pin control register

    mov A, [control_pkt + 6] ; get pin config byte
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov REG[P15CR], A   ; set the pin control register

    mov A, [control_pkt + 7] ; get pin config byte
    and A, PIN_CFG_MASK ; mask off the bits we will accept
    mov REG[P16CR], A   ; set the pin control register

    ; ack the request
    mov [control_pkt + CCODE], CTL_SETPINCONFIG
    mov X, CTL_BASE_SIZE
    lcall write_control

  set_pin_config_done:
    ret

get_pins:
    call has_gpios
    jz get_pins_done

    ; disable ints to avoid race conditions on the ports
    and F, 0xFE
    mov A, REG[P0DATA]               ; get the current pin state
    mov [control_pkt + CDATA + 0], A ; write out the state
    mov A, REG[P1DATA]               ; get the current pin state
    mov [control_pkt + CDATA + 1], A ; write out the state
    or  F, 0x1 ; re-enable global interrupts

    ; no need to move first byte's 4 bits, the other >> 3
    asr [control_pkt + CDATA + 1]
    asr [control_pkt + CDATA + 1]
    asr [control_pkt + CDATA + 1]

    ; mask away the upper bits of both bytes
    and [control_pkt + CDATA + 0], 0x0F
    and [control_pkt + CDATA + 1], 0x0F

  get_pins_done:
    ret

; FUNCTION set_pins
;   arg: low nibble of first byte has value of port 0 pins
;        low nibble of second byte has value of port 1 pins
set_pins:
    call has_gpios
    jz set_pins_done

    ; mask away the upper bits of both bytes
    and [control_pkt + CDATA + 0], 0x0F
    and [control_pkt + CDATA + 1], 0x0F

    ; no need to move first byte's 4 bits, the other << 3
    asl [control_pkt + CDATA + 1]
    asl [control_pkt + CDATA + 1]
    asl [control_pkt + CDATA + 1]

    ; disable ints to avoid race conditions on the ports
    and F, 0xFE
    mov A, REG[P0DATA]              ; get the current pin state
    and A, 0xF0                     ; mask off the ones we're setting
    or A, [control_pkt + CDATA + 0] ; set the appropriate bits
    mov REG[P0DATA], A              ; write out the new value

    mov A, REG[P1DATA]              ; get the current pin state
    and A, 0x87                     ; mask off the ones we're setting
    or A, [control_pkt + CDATA + 1] ; set the appropriate bits
    mov REG[P1DATA], A              ; write out the new value

    or  F, 0x1 ; re-enable global interrupts
  set_pins_done:
    ret

; FUNCTION pins_burst
;   sets the pins to a sequence of values
;   arg: control packet data byte 0 contains number of transfers
;   arg: buffer contains sequence info (pin states to set)
pin_burst:
    call has_gpios
    jz reset_done

    mov [buffer_ptr], buffer ; reset to start of buffer
    ; first get the number of transfers, put in X
    mov X, [control_pkt + CDATA]

  pin_burst_loop:
    jz pin_burst_done   ; see if we're done
    mvi A, [buffer_ptr] ; get the next byte to set
    mov [tmp1], A       ; keep a copy

    ; set it up in the control packet locations--low nibble in first byte, high in second byte
    and A, 0x0F
    mov [control_pkt + CDATA + 0], A

    mov A, [tmp1]
    and A, 0xF0
    ; shift right 4
    asr A
    asr A
    asr A
    asr A
    mov [control_pkt + CDATA + 1], A

    ; ok, set pins
    lcall set_pins

    ; delay a bit
    mov A, BURST_DELAY
  pin_burst_delay:
    jz pin_burst_delend
    dec A
    jmp pin_burst_delay

  pin_burst_delend:
    dec X
    jmp pin_burst_loop

  pin_burst_done:
    ret
