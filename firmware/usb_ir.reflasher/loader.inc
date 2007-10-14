;**************************************************************************
; * loader.inc ***************************************************************
; **************************************************************************
; *
; * 
; * PC transceiver functions (the USB port)
; *
; * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
; * Author: Joseph Dunn <jdunn@iguanaworks.net>
; *
; * Distributed under the GPL version 2.
; * See LICENSE for license details.
; */


; flag bits provided by the loader
FLAG_HALTED:     EQU 0x01

; control codes supported by the boot loader
CTL_VERSION:		EQU 0x01 ; get the firmware version
CTL_PROG:			EQU 0x02 ; program a block of flash
CTL_CHKSUM:			EQU 0x03 ; checksum a block of flash
CTL_INVALID_ARG:	EQU 0x04 ; bad argument
CTL_RST:			EQU	0xFF ; reset requested

; USB read/write constants
BUFFER_SIZE:   EQU 150 ; size of the data buffer
PACKET_SIZE:   EQU 8   ; size of packets to send
CTL_BASE_SIZE: EQU 4   ; base size of control packets (w/o data)
CCODE:         EQU 3   ; where control code goes in control_pkt
CDATA:         EQU 4   ; where data starts in control_pkt

; flash programming constants
FLASH_BLOCK_SIZE: 	EQU 64 ;size of flash block
; addresses for SROM param block taken from the data sheet
KEY1:				EQU	0xF8
KEY2:				EQU	0xF9
BLOCKID:			EQU 0xFA
POINTER:			EQU 0xFB
CLOCK:				EQU 0xFC
DELAY:				EQU 0xFE
FIRST_FLASH_VAR:	EQU KEY1

; CRAPPY:
;   FIRST_FLASH_VAR to not overlap with control_pkt and SROM stuff
;   BUFFER_SIZE for the buffer itself
;   -3 for 3 1 byte variables in loader.asm
;   -2 for 2 1 byte variables in main.asm
PINNED_VAR_START: EQU FIRST_FLASH_VAR - BUFFER_SIZE - 3 - 2

; start the bss for the body code after the bytes used by the usb
;  library + space for the stack.  The stack size is based on what
;  was used successfully on pre-bootloader versions.
BODY_SKIP_BYTES:    EQU 67 + 18

; over all the previous 2 numbers define the space allowed for "body"
;  variables: 0xF8 - 150 - 3 - 2 - 67 - 18 == 8 ew, TINY!
;  NOTE: body code compilation cannot exceed 93 bytes used!

; constants to remove some "magic" numbers
IN:			EQU 1
OUT:		EQU 2
FROM_PC:	EQU 0xCD
TO_PC:		EQU 0xDC
