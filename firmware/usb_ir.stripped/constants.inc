;constants.inc
;
;Constant declarations
;
;Copyright (C) 2006, Brian Shucker <brian@iguanaworks.net>
;
;Distribute under the GPL version 2.
;See COPYING for license details.

;version ID indicates the firmware version loaded on this chip
VERSION_ID_LOW:  EQU 0x04 ;firmware version ID low byte
VERSION_ID_HIGH: EQU 0x00 ;firmware version ID high byte

;flash programming stuff
FLASH_BLOCK_SIZE: 	EQU 64 ;size of flash block
;addresses for SROM param block
KEY1:				EQU	0xF8
KEY2:				EQU	0xF9
BLOCKID:			EQU 0xFA
POINTER:			EQU 0xFB
CLOCK:				EQU 0xFC
DELAY:				EQU 0xFE
;exec address
EXEC_ADDR:			EQU 0x1FC0

BUFFER_SIZE: EQU 144 ;size of code buffer
;don't make code buffer smaller than 132--there will be dire consequences.
;This guarantees that addresses 7c through 83 (8 bytes) are inside
;the code buffer, no matter where it is in memory.  We can take advantage of that
;fact when writing user-defined code that needs a packet buffer.

PACKET_SIZE:   EQU 8 ;size of packets to send
CTL_BASE_SIZE: EQU 4 ;base size of control packets (w/o data)

;control codes
CTL_VERSION:		EQU 0x01 ;get the firmware version
CTL_PROG:			EQU 0x0C ;program a block of flash
CTL_EXEC:			EQU 0x0D ;call an arbitrary address
CTL_RST:			EQU	0xFF ;reset requested

CCODE: EQU  3 ;where control code goes in control_pkt
CDATA: EQU  4 ;where control data goes in control_pkt
