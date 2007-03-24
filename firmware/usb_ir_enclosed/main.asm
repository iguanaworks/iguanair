;main.asm
;
;Main program
;
;Copyright (C) 2006, Brian Shucker <brian@iguanaworks.net>
;
;Distribute under the GPL version 2.
;See COPYING for license details.

include "m8c.inc"       ; part specific constants and macros
include "memory.inc"    ; Constants & macros for SMM/LMM and Compiler
include "PSoCAPI.inc"   ; PSoC API definitions for all User Modules
include "constants.inc"

export _main
export soft_reset
export tmp
export tmp2
export write_ptr
export rx_overflow
export rx_fill
export halted
export buffer_ptr
export buf_size

AREA bss

;TODO: combine these flag bits into one byte
buffer_ptr:
	BLK 1 ;current index into buffer
write_ptr:
	BLK 1 ;current index where we are writing from buffer to USB
rx_overflow:
	BLK 1 ;true if rx buffer has overflowed
rx_on:
	BLK 1 ;true if receiver is turned on
buf_size:
	BLK 1 ;amount of data currently in buffer
rx_fill:
	BLK 1 ;true if receiver on and a packet's worth of data ready to send
halted:
	BLK 1 ;true if endpoint was halted
tmp:
	BLK 1 ;general purpose temporary var
	;NOTE: tmp is not preserved across function calls!
tmp2:
	BLK 1 ;general purpose temporary var
	;NOTE: tmp2 is not preserved across function calls!
flash_addr:
	BLK 1

AREA text

_main:
	mov REG[TX_PIN_CR], 0x01 ;enable output on tx pin

	;configure capture
	mov REG[RX_PIN_CR], 0b00000010 ;configure port pin: pullup enabled
	mov REG[TMRCLKCR], 0b11001111 ;timer to 3MHz
	mov REG[TMRCR], 0b00001000 ;16 bit mode
	mov REG[TCAPINTE], 0b00000011 ;configure rise and fall interrupts
	;don't actually enable the interrupt yet

	mov A, 0 ;put arg 0 for USB_start
	lcall USB_Start ;enable USB device
	or  F, 0x1 ;enable global interrupts

;wait for usb enumeration
config_loop:
	lcall USB_bGetConfiguration
	jz config_loop ;if return val was zero, try again

;now we're connected
soft_reset:
	mov [halted], 0; :clear halt flag
	mov [rx_overflow], 0; clear rx overflow flag
	mov [rx_on], 0 ;rx starts in the off state
	mov [rx_fill], 0 ;clear fill flag
	lcall rx_disable ;make sure receiver is off
	lcall pins_reset ;clear GPIO pin state

	;clear the IN endpoint by sending 0-byte packet
	mov [USB_APIEPNumber], 0x1 ;set to endpoint 1
	mov [USB_APICount], 0 ;set packet size
	mov X, buffer ;put packet address into X
	lcall USB_XLoadEP ;send packet

	;enable OUT endpoint
	MOV		A, 2 ;out is enpoint 2
	lcall  USB_EnableEP

main_loop:
	mov A, [halted];check for halt condition
	jnz soft_reset ;go to known state after halt
	;check for data from host
	lcall check_read ;see if there is a transmission from the host
	jnz main_recv ;there is a transmission, so receive and handle it
	mov A, [rx_overflow] ;check rx overflow flag
	jnz main_send_oflow ;there is a rx overflow, send it to the host
	mov A, [rx_fill] ;check rx fill flag
	jnz main_write_signal ;there is rx data, send it to the host
	jmp main_loop ;repeat main loop

main_recv:
	;get control packet
	lcall read_control

	;at this point, A contains control code
	jz main_loop ;null control code, just ignore
	cmp A, CTL_VERSION ;get version
	jz main_getversion
	cmp A, CTL_GET_BUFSIZE ;get the buffer size
	jz main_getbufsize
	cmp A, CTL_RX_ON ;enable receive
	jz main_rx_on
	cmp A, CTL_RX_OFF ;disable receive
	jz main_rx_off
	cmp A, CTL_TX ;transmit a code
	jz main_transmit ;receive code, ack and transmit
	cmp A, CTL_SET_PINS ;set the GPIO pins
	jz main_set_pins
	cmp A, CTL_BURST ;set the GPIO pins in a sequence
	jz main_burst
	cmp A, CTL_GET_PINS ;get the GPIO pin state
	jz main_get_pins
	cmp A, CTL_GET_CFG0_PINS ;get GPIO configuration
	jz main_get_cfg0_pins
	cmp A, CTL_SET_CFG0_PINS ;set GPIO configuration
	jz main_set_cfg0_pins
	cmp A, CTL_GET_CFG1_PINS ;get GPIO configuration
	jz main_get_cfg1_pins
	cmp A, CTL_SET_CFG1_PINS ;set GPIO configuration
	jz main_set_cfg1_pins
	cmp A, CTL_PROG ;program a block of flash
	jz main_prog
	cmp A, CTL_EXEC ;call an arbitrary address
	jz main_exec
	cmp A, CTL_RST ;reset command
	jz main_reset
	jmp main_loop; default behavior--unknown code

main_getversion:
	lcall rx_disable ;disable timer interrupt, clear rx state
	//send control packet with version id
	mov [control_pkt + CCODE], CTL_VERSION
	mov [control_pkt + CDATA], VERSION_ID_LOW
	mov [control_pkt + CDATA+1], VERSION_ID_HIGH
	mov A, CTL_BASE_SIZE + 2
	lcall write_control
	jmp main_return

main_getbufsize:
	lcall rx_disable ;disable timer interrupt, clear rx state
	//send control packet with buffer size
	mov [control_pkt + CCODE], CTL_GET_BUFSIZE
	mov [control_pkt + CDATA], BUFFER_SIZE
	mov A, CTL_BASE_SIZE + 1
	lcall write_control
	jmp main_return

main_rx_on:
	//send ack
	mov [control_pkt + CCODE], CTL_RX_ON
	mov A, CTL_BASE_SIZE
	lcall write_control;

	mov [rx_on], 0x1 ;note that rx is on
	lcall rx_enable ;enable rx
	jmp main_loop

main_rx_off:
	lcall rx_disable ;disable rx
	mov [rx_on], 0x0 ;note that rx is off
	//send ack
	mov [control_pkt + CCODE], CTL_RX_OFF
	mov A, CTL_BASE_SIZE
	lcall write_control;
	jmp main_loop

main_transmit:
	lcall rx_disable;disable timer interrupt, clear rx state
	lcall read_buffer ;receive the code--returns 0 if read overflow
	jz main_tover
	lcall transmit_code ;transmit
	;send ack
	mov [control_pkt + CCODE], CTL_TX
	mov A, CTL_BASE_SIZE
	lcall write_control ;send control packet
	jmp main_return

main_tover:
	lcall transmit_code ;transmit anyway
	;send overflow instead of ack
	mov [control_pkt + CCODE], CTL_TX_OVERFLOW
	mov A, CTL_BASE_SIZE
	lcall write_control ;send control packet
	jmp main_return

main_send_oflow:
	;send control packet
	mov [control_pkt + CCODE], CTL_RX_OVERFLOW
	mov A, CTL_BASE_SIZE
	lcall write_control;
	mov [rx_overflow], 0 ;clear overflow flag
	jmp main_loop ;repeat main loop

main_write_signal:
	lcall write_signal
	jmp main_loop ;repeat main loop

main_get_pins:
	lcall rx_disable ;disable timer interrupt, clear rx state
	;get the pins
	lcall pins_get ;get the pins and put data in control packet
	mov [control_pkt + CCODE], CTL_GET_PINS
	mov A, CTL_BASE_SIZE+2
	lcall write_control;
	jmp main_return

main_set_pins:
	lcall rx_disable ;disable timer interrupt, clear rx state
	;set the pins
	lcall pins_set ;set the pins
	;send ack packet
	mov [control_pkt + CCODE], CTL_SET_PINS
	mov A, CTL_BASE_SIZE
	lcall write_control;
	jmp main_return

main_burst:
	lcall rx_disable ;disable timer interrupt, clear rx state
	;set the pins
	lcall read_buffer ;get the block to read
	lcall pins_burst ;set pins in a burst
	;send ack packet
	mov [control_pkt + CCODE], CTL_BURST
	mov A, CTL_BASE_SIZE
	lcall write_control;
	jmp main_return

main_get_cfg0_pins:
	lcall rx_disable ;disable timer interrupt, clear rx state
	;get the pins config
	lcall pins_get_cfg0 ;get the pin config and put data in control packet
	mov [control_pkt + CCODE], CTL_GET_CFG0_PINS
	mov A, CTL_BASE_SIZE+4
	lcall write_control;
	jmp main_return

main_get_cfg1_pins:
	lcall rx_disable ;disable timer interrupt, clear rx state
	;get the pins config
	lcall pins_get_cfg1 ;get the pin config and put data in control packet
	mov [control_pkt + CCODE], CTL_GET_CFG1_PINS
	mov A, CTL_BASE_SIZE+4
	lcall write_control;
	jmp main_return

main_set_cfg0_pins:
	lcall rx_disable ;disable timer interrupt, clear rx state
	;set the pins
	lcall pins_set_cfg0 ;set the pin config
	;send ack packet
	mov [control_pkt + CCODE], CTL_SET_CFG0_PINS
	mov A, CTL_BASE_SIZE
	lcall write_control;
	jmp main_return

main_set_cfg1_pins:
	lcall rx_disable ;disable timer interrupt, clear rx state
	;set the pins
	lcall pins_set_cfg1 ;set the pin config
	;send ack packet
	mov [control_pkt + CCODE], CTL_SET_CFG1_PINS
	mov A, CTL_BASE_SIZE
	lcall write_control;
	jmp main_return

	halt ;to prevent anyone from falling into main_prog
main_prog:
	lcall rx_disable ;disable timer interrupt, clear rx state
	mov [flash_addr], [control_pkt + CDATA] ;read the flash block address
	mov [control_pkt + CDATA], FLASH_BLOCK_SIZE ;set up to read right number of bytes into buffer
	lcall read_buffer; //get the block
	;ack the receive
	mov [control_pkt + CCODE], CTL_PROG
	mov A, CTL_BASE_SIZE
	lcall write_control;

	;wait for ack to go through
main_cwait:
	mov A, [halted];check for halt condition
	jnz soft_reset ;go to known state after halt
	mov A, 0x1; ;check endpoint 1
	lcall USB_bGetEPState ;check state
	CMP A, IN_BUFFER_EMPTY ;compare--if equal, zero flag set
	jnz main_cwait ;not equal, keep waiting

	;now do a block erase
	mov [KEY1], 0x3A;
	mov X, SP ;get stack pointer
	mov A, X
	add A, 3 ;just following directions from datasheet
	mov [KEY2], A
	mov [BLOCKID], [flash_addr] ;set which flash block to write
	mov [CLOCK], 0x00 ;guessing at the right clock divider
	mov [DELAY], 0xAC ;this is a guess, since datasheet says use 0x56 for 12MHz
	mov A, 0x03 ;erase block code
	ssc ;do it

	;now set up the parameter block for the SROM call
	mov [KEY1], 0x3A;
	mov X, SP ;get stack pointer
	mov A, X
	add A, 3 ;just following directions from datasheet
	mov [KEY2], A
	mov [BLOCKID], [flash_addr] ;set which flash block to write
	mov [POINTER], buffer ;read data from buffer
	mov [CLOCK], 0x00 ;guessing at the right clock divider
	mov [DELAY], 0xAC ;this is a guess, since datasheet says use 0x56 for 12MHz
	mov A, 0x02 ;write block code
	ssc ;do it

	jmp main_reset ;reset

main_exec:
	lcall rx_disable ;disable timer interrupt, clear rx state
	lcall EXEC_ADDR ;call the user-defined function
	jmp main_return;

;common code to re-enable rx if necessary and go to main loop
main_return:
	//re-enable rx if it's supposed to be on
	mov A, [rx_on]
	jz main_loop ;rx off, just go to main loop
	lcall rx_enable ;rx on, so re-enable
	jmp main_loop ;repeat main loop

main_reset:
	lcall USB_Stop ;have to do this first, or badness occurs
	mov [KEY1], 0x3A;
	mov X, SP ;get stack pointer
	mov A, X
	add A, 3 ;just following directions from datasheet
	mov [KEY2], A
	mov A, 0 ;reset code
	ssc

prog_end:
.terminate:
    jmp .terminate

;put ret at EXEC_ADDR, so it doesn't halt if the user doens't put anything there
AREA userprog (ROM,ABS)
org EXEC_ADDR
	ret