;constants.inc
;
;Constant declarations
;
;Copyright (C) 2006, Brian Shucker <brian@iguanaworks.net>
;
;Distribute under the GPL version 2.
;See COPYING for license details.

;version ID indicates the firmware version loaded on this chip
VERSION_ID_LOW:  EQU 0x03 ;firmware version ID low byte
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
;address of the tx function
TXPROG_ADDR:		EQU 0x1C00


BUFFER_SIZE: EQU 144 ;size of code buffer
;don't make code buffer smaller than 132--there will be dire consequences.
;This guarantees that addresses 7c through 83 (8 bytes) are inside
;the code buffer, no matter where it is in memory.  We can take advantage of that
;fact when writing user-defined code that needs a packet buffer.

PACKET_SIZE:   EQU 8 ;size of packets to send
CTL_BASE_SIZE: EQU 4 ;base size of control packets (w/o data)

;control codes
CTL_VERSION:		EQU 0x01 ;get the firmware version
CTL_TX:				EQU	0x02 ;tx starting, will read code from host
CTL_RX_ON:			EQU	0x03 ;rx enabled
CTL_RX_OFF:			EQU	0x04 ;rx disabled
CTL_GET_PINS:		EQU	0x05 ;get GPIO pin state
CTL_SET_PINS:		EQU	0x06 ;set GPIO pin state
CTL_GET_CFG0_PINS:	EQU 0x07 ;get GPIO pin configuration, port 0
CTL_SET_CFG0_PINS:	EQU 0x08 ;set GPIO pin configuration, port 0
CTL_GET_CFG1_PINS:	EQU 0x09 ;get GPIO pin configuration, port 1
CTL_SET_CFG1_PINS:	EQU 0x0A ;set GPIO pin configuration, port 1
CTL_GET_BUFSIZE:	EQU 0x0B ;get the data buffer size
CTL_PROG:			EQU 0x0C ;program a block of flash
CTL_EXEC:			EQU 0x0D ;call an arbitrary address
CTL_BURST:			EQU 0x0E ;set the GPIO pins in a sequence
CTL_RST:			EQU	0xFF ;reset requested
CTL_RX_OVERFLOW: 	EQU 0x20 ;received code is too long, throwing it out
CTL_TX_OVERFLOW: 	EQU 0x30 ;transmit code is too long, throwing it out

RX_PIN_CR: EQU P05CR ; control reg for rx pin

TX_BANK:   EQU P1DATA ;which set are the transmit pins in?
TX_MASK:   EQU 0x0C   ;all pins that can tx

BURST_DELAY: EQU 30 ;delay loops between GPIO transfers in burst mode

CCODE: EQU  3 ;where control code goes in control_pkt
CDATA: EQU  4 ;where control data goes in control_pkt
