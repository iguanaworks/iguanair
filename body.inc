; **************************************************************************
; * body.inc ***************************************************************
; **************************************************************************
; *
; * Template for the body.inc header file that defines the functions
; * supported by the firmware body code.
; *
; * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
; * Author: Joseph Dunn <jdunn@iguanaworks.net>
; *
; * Distributed under the GPL version 2.
; * See LICENSE for license details.
; */

; standard "body" functions
CTL_GETFEATURES:  EQU 0x10
CTL_GETBUFSIZE:   EQU 0x11
CTL_RECVON:       EQU 0x12
CTL_RAWRECVON:    EQU 0x13
CTL_RECVOFF:      EQU 0x14
CTL_SEND:         EQU 0x15
CTL_GETCHANNELS:  EQU 0x16
CTL_SETCHANNELS:  EQU 0x17
CTL_GETCARRIER:   EQU 0x18
CTL_SETCARRIER:   EQU 0x19
CTL_GETPINCONFIG: EQU 0x1A
CTL_SETPINCONFIG: EQU 0x1B
CTL_GETPINS:      EQU 0x1C
CTL_SETPINS:      EQU 0x1D
CTL_PINBURST:     EQU 0x1E
CTL_EXECUTE:      EQU 0x1F
CTL_GETID:        EQU 0x20
CTL_SETID:        EQU 0x21

; packets initiated by the device
CTL_RECV:         EQU 0x30
CTL_OVERRECV:     EQU 0x31
CTL_OVERSEND:     EQU 0x32

; device feature flags, 0 means old style device
HAS_LEDS:         EQU 0x01
HAS_BOTH:         EQU 0x02
HAS_SOCKETS:      EQU 0x04
HAS_LCD:          EQU 0x08

; other constants
RX_PIN_CR:    EQU P05CR  ; control reg for rx pin

TX_BANK:      EQU P1DATA ; which set are the transmit pins in?
OLD_TX_BANK:  EQU P0DATA ; pins used to be in this set
TX_MASK:      EQU 0xF0   ; all pins that can tx
OLD_TX_MASK:  EQU 0x40   ; mask for unenclosed devices

PIN_CFG_MASK: EQU 0x07   ; we only configure these pin control bits
BURST_DELAY:  EQU 30     ; delay loops between GPIO transfers in burst mode
