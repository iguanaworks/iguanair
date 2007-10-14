;**************************************************************************
; * body.inc ***************************************************************
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


; constants only used by the body code
; supported control codes
CTL_RECVON:       EQU 0x10
CTL_RECVOFF:      EQU 0x11
CTL_SEND:         EQU 0x12
CTL_GETCHANNELS:  EQU 0x13
CTL_SETCHANNELS:  EQU 0x14
CTL_GETPINCONFIG: EQU 0x15
CTL_SETPINCONFIG: EQU 0x16
CTL_GETPINS:      EQU 0x17
CTL_SETPINS:      EQU 0x18
CTL_BULKPINS:     EQU 0x19
CTL_EXECUTE:      EQU 0x1A
CTL_GETID:        EQU 0x1B
CTL_GETBUFSIZE:   EQU 0x1C

; errors
CTL_OVERRECV:    EQU 0x1D
CTL_OVERSEND:    EQU 0x1E

; other constants
RX_PIN_CR:   EQU P05CR  ; control reg for rx pin
TX_BANK:     EQU P1DATA ; which set are the transmit pins in?
TX_MASK:     EQU 0xF0   ; all pins that can tx
BURST_DELAY: EQU 30     ; delay loops between GPIO transfers in burst mode
