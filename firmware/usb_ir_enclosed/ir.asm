;ir.asm
;
;IR transceiver functions
;
;Copyright (C) 2006, Brian Shucker <brian@iguanaworks.net>
;
;Distribute under the GPL version 2.
;See COPYING for license details.

include "m8c.inc"       ; part specific constants and macros
include "memory.inc"    ; Constants & macros for SMM/LMM and Compiler
include "PSoCAPI.inc"   ; PSoC API definitions for all User Modules
include "constants.inc"

export rx_enable
export rx_disable
export transmit_code
export tcap_int
export twrap_int

AREA bss

tx_pins:
	BLK 1 ;pins to use for current tx
tx_state:
	BLK 1 ;state of tx (on or off)
tx_temp:
	BLK 1 ;tx temp variable

rx_high:
	BLK 1 ;received data high byte
rx_low:
	BLK 1 ;received data low byte
rx_pulse:
	BLK 1 ;high bit is 1 if this is a pulse, 0 for space

AREA text

;FUNCTION rx_enable enables the IR receiver
rx_enable:
	mov [rx_overflow], 0; clear rx overflow flag
	mov [rx_fill], 0 ;clear fill flag
	mov [write_ptr], buffer ;reset write ptr to start of buffer
	mov [buffer_ptr], buffer; reset rx ptr to start of buffer
	mov [buf_size], 0 ;reset size to 0

	;enable the timer capture interrupt
	mov A, REG[INT_MSK1]
	or A, 0b10000000 ;tcap interrupt enable
	mov REG[INT_MSK1], A

	;enable the timer wrap interrupt
	mov A, REG[INT_MSK2]
	or A, 0b00000010 ;twrap interrupt enable
	mov REG[INT_MSK2], A

	ret;

;FUNCTION rx_disable disables the IR receiver
rx_disable:
	;disable the timer interrupt
	mov A, REG[INT_MSK1]
	and A, ~0b10000000 ;tcap interrupt enable
	mov REG[INT_MSK1], A

	;disable the timer wrap interrupt
	mov A, REG[INT_MSK2]
	and A, ~0b00000010 ;twrap interrupt enable
	mov REG[INT_MSK2], A

	mov [rx_fill], 0 ;clear fill flag
	mov [rx_overflow], 0; clear rx overflow flag
	ret;

;FUNCTION buf_load
;puts value into the circular buffer
;arg: a is value to load
;does not modify A
buf_load:
	cmp [buf_size], BUFFER_SIZE ;check for overflow
	jz bl_oflow
	mvi [buffer_ptr], A ;load the data
	inc [buf_size]
	cmp [buf_size], PACKET_SIZE ;see if we have enough data to send to host
	jz bl_fill
bl_check_wrap:
	cmp [buffer_ptr], buffer + BUFFER_SIZE ;check for end of buffer
	jz bl_wrap
	jmp bl_done

bl_oflow:
	;set the rx overflow flag, clear buffers
	mov [rx_overflow], 1
	mov [write_ptr], buffer ;reset write ptr to start of buffer
	mov [buffer_ptr], buffer; reset rx ptr to start of buffer
	mov [buf_size], 0 ;reset size to 0
	jmp bl_done

bl_fill:
	mov [rx_fill], 1 ;set fill flag to true
	jmp bl_check_wrap

bl_wrap:
	mov [buffer_ptr], buffer ;wrap around to start of buffer
	jmp bl_done

bl_done:
	ret


;FUNCTION load_value
;loads a received value into the data buffer
;arg: rx_high and rx_low have the raw timer data
;	rx_pulse has the pulse bit set correctly
;returns: 1 if ok, 0 if overflow
load_value:
	push X
	;shift right 6 bits
	mov A, [rx_low]
	asr A
	asr A
	asr A
	asr A
	asr A
	asr A
	;clear the upper 6 bits (because of sign extend, have to be sure they're 0)
	and A, 0x03
	mov [rx_low], A ;stores bits 0-1 of final result

	;for bits 2-6, we want to use bits 0-4 of the upper byte, so shift left 2
	mov A, [rx_high]
	asl A
	asl A
	or [rx_low], A ;stores bits 2-6 of final result
	jz ld_skip_dec ;if zero, don't decrement
	;TODO: this gives us some minor inaccuracy
	;decrement raw value by one, so our range is 1-128 instead of 0-127
	dec [rx_low]
ld_skip_dec:
	;last bit of final result is the pulse bit
	mov A, [rx_low]
	and A, 0x7F ;clear pulse bit in case it's already set
	or  A, [rx_pulse]
	mov [rx_low], A

	;now we need to deal with long pulses: send repeated FF packets to
	;take care of times longer than 128

	;we need to loop for the value of bits 5-7 of the high byte
	mov A, [rx_high]
	;shift 5 bits over
	asr A
	asr A
	asr A
	asr A
	asr A
	;clear the upper 5 bits (because of sign extend, have to be sure they're 0)
	and A, 0x7

	;set up the loop
	mov X, A ;loop counter in X
	jz ld_big_done ;if zero, we don't need to do any big loads

	;put the value to load in A
	mov A,[rx_pulse] ;set pulse bit correctly
	or A, 0x7F; load max value

	;loop and load the right number of max value packets
ld_big_loop:
	lcall buf_load ;load A into buffer
	dec X
	jnz ld_big_loop

ld_big_done:
	;send the remainder
	mov A, [rx_low] ;get the remainder byte
	lcall buf_load ;load A into buffer
	pop X
	ret



;INTERRUPT
;timer/capture interrupt handler
tcap_int:
	PUSH A

	mov A, REG[TCAPINTS] ;read capture interrupt status
	and A, 0x1 ;true if  this is a rising edge
	jnz tcap_rise

	;if here, it's a falling edge
tcap_rx_fall:
	;here we have the start of a pulse, aka end of a space
	;read the timer and store in packet buffer
	mov A, REG[FRTMRL] ;read low-order byte
	mov [rx_low], A ;store
	mov A, REG[FRTMRH] ;read high-order byte
	mov [rx_high], A ;store
	mov [rx_pulse], 0x80 ; set pulse bit to indicate space
	lcall load_value ;load into data buffer


	jmp tcap_done

tcap_rise:
	;if here, we have the end of a pulse
	;read the timer and store in packet buffer
	mov A, REG[FRTMRL] ;read low-order byte
	mov [rx_low], A ;store
	mov A, REG[FRTMRH] ;read high-order byte
	mov [rx_high], A ;store
	mov [rx_pulse], 0 ; clear pulse bit to indicate pulse
	lcall load_value ;load into data buffer

	jmp tcap_done

tcap_done:
	mov REG[FRTMRL], 0;reset timer low byte
	mov REG[FRTMRH], 0;reset timer high byte
	mov REG[TCAPINTS], 0x0F ;clear int status
	POP A
	reti ;done

;INTERRUPT
;timer wrap interrupt handler

twrap_int:
	push A

	;load an 0x80 to indicate full-length space
	mov A, 0x80
	lcall buf_load

	pop A
	reti ;done

;put the transmit code at a known location in memory, so we can overwrite it without
;too much difficulty via the PROG command (to change the carrier frequency, for example)
AREA txprog (ROM,ABS)
org TXPROG_ADDR

;FUNCTION transmit_code
;transmit the code over IR
;code format: first bit is 1 for on, 0 for off
;next 7 bits are length in 26.3uS (38KHz) increments--that's 316 clocks up, 316 down at 24MHz
transmit_code:
	; read a byte describing channel selection
	mov [tx_pins], [control_pkt + CDATA + 1]
	; if the byte was 0 then transmit on all channels
	mov A, [tx_pins]
	jnz tx_start
	mov [tx_pins], TX_MASK
tx_start:
	mov [buffer_ptr], buffer ;reset to start of buffer
	mov [tx_state], 0 ;clear tx state
	mov [tmp2], [control_pkt + CDATA] ;get number of bytes to transmit
	mov A, [tmp2] ; set zero flag if tmp2 is zero
tx_loop:
	jz tx_done ;if zero byte, we're done
	mvi A, [buffer_ptr] ;move buffer data into A, increment pointer
	mov [tx_temp], A ;store byte
	and A, 0x7F ;mask off the pulse length bits
	asl A ;shift left to multiply by two due to carrier division
	mov X, A ;store pulse length in X

	mov A, [tx_temp]; get original byte back
	and A, 0x80; mask off pulse on/off bit
	jz tx_on ;if on, jump to tx_on, else fall through
	mov [tx_state], 0; clear tx
	jmp tx_pulse ;start sending pulse

tx_on:
	mov [tx_state], [tx_pins] ;mask on tx bits; TODO: changed timing!!!
	jmp tx_pulse ;start sending pulse--this jump seems redundant,
	             ;but is there to make timing the same on both branches

tx_pulse: ;ready to send a pulse.  Need to AND in 38KHz carrier
	mov A, X ;put pulse length into A, to make zero flag valid			[4 cycles]
	jz tx_end_pulse; this pulse is done                   				[5 cycles]
	mov A, REG[TX_BANK] ;get current register state						[6 cycles]
	xor A, [tx_state]; if on, we're toggling.  If off, doing nothing	[6 cycles]
	mov REG[TX_BANK], A; write change to register						[5 cycles]
	dec X ;decrement remaining pulse length								[4 cycles]

	;now we need to delay to get a total of 316 clocks
	lcall delay4_38k;													[21 cycles (including ret), + delay]
	lcall delay7_38k;													[21 cycles (including ret), + delay]

	jmp tx_pulse ;continue the pulse									[5 cycles]

tx_end_pulse:
	and REG[TX_BANK], TX_MASK ;make sure tx pins are off
	dec [tmp2] ;decrement remaining byte count
	jmp tx_loop ;go to the next pulse

tx_done:
	and REG[TX_BANK], TX_MASK ;make sure tx pins are off
	ret ;done

;this is a set of 7-clock delays.  You jump into it at different points in
;order to get different length delays.
delay7:
	cmp A, [0]
	cmp A, [0]
	cmp A, [0]
	cmp A, [0]
delay7_56k:
	cmp A, [0]
	cmp A, [0]
delay7_38k:
delay7_40k:
	cmp A, [0]
	ret

;this is a set of 4-clock delays.  You jump into it at different points in
;order to get different length delays.
delay4:
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
delay4_38k:
	nop
	nop
	nop
	nop
delay4_40k:
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
delay4_56k:
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop

	ret ;go back from whence you came