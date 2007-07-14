;constants.inc
;
;Constant declarations
;
;Copyright (C) 2007, Joseph Dunn <jdunn@iguanaworks.net>
;
;Distribute under the GPL version 2.
;See COPYING for license details.

; control codes supported by the boot loader
CTL_VERSION:		EQU 0x01 ; get the firmware version
CTL_PROG:			EQU 0x02 ; program a block of flash
CTL_CHKSUM:			EQU 0x03 ; checksum a block of flash
CTL_RST:			EQU	0xFF ; reset requested

; special reflasher version number
VERSION_ID_LOW:  EQU 0x00 ; firmware version ID low byte
VERSION_ID_HIGH: EQU 0x01 ; firmware version ID high byte

; USB read/write constants
BUFFER_SIZE:   EQU 64 ; size of the data buffer
PACKET_SIZE:   EQU 8  ; size of packets to send
CTL_BASE_SIZE: EQU 4  ; base size of control packets (w/o data)

; control packet layout
CCODE: EQU 3 ; where control code goes in control_pkt
CDATA: EQU 4 ; where control data goes in control_pkt

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

; constants to remove some "magic" numbers
IN:			EQU 1
OUT:		EQU 2
FROM_PC:	EQU 0xCD
TO_PC:		EQU 0xDC
CODE_START:	EQU 0x14E0
