;util.asm
;
;Utility functions
;
;Copyright (C) 2006, Brian Shucker <brian@iguanaworks.net>
;
;Distribute under the GPL version 2.
;See COPYING for license details.
	
include "m8c.inc"       ; part specific constants and macros
include "memory.inc"    ; Constants & macros for SMM/LMM and Compiler
include "PSoCAPI.inc"   ; PSoC API definitions for all User Modules
include "constants.inc"

export pins_set
export pins_get
export pins_burst
export pins_reset
export pins_set_cfg0
export pins_get_cfg0
export pins_set_cfg1
export pins_get_cfg1
;export wait
;export blink

AREA bss
	
loop_count_high:
	BLK 1 ;loop counter
loop_count_low:
	BLK 1 ;loop counter	
	
AREA text

PIN_CFG_MASK: EQU 0x07 ;we can configure only these bits in the pin control registers

;FUNCTION pins_reset
;puts the pins back into the state they have on power-up
pins_reset:
	;clear the data registers
	mov REG[P0DATA], 0
	mov REG[P1DATA], 0
	
	;clear the config registers for pins we use
	;port 0, pins 0-3
	mov REG[P00CR], 0
	mov REG[P01CR], 0
	mov REG[P02CR], 0
	mov REG[P03CR], 0
	;port 1, pins 3-6
	mov REG[P13CR], 0
	mov REG[P14CR], 0
	mov REG[P15CR], 0
	mov REG[P16CR], 0
	
	ret


;FUNCTION pins_set
;arg: low nibble of first byte has value of port 0 pins
;low nibble of second byte has value of port 1 pins

pins_set:
	;first deal with the low bits, on port 0.0-0.3
	mov A, [control_pkt + CDATA] ;get the byte to set
	
	;low 4 bits go to port 0
	and A, 0x0F ;mask off the upper bits
	mov [tmp], A ;store A for the moment
	
	;disable ints to avoid race conditions on the port
	and F, 0xFE ;clear global interrupt bit
	mov A, REG[P0DATA] ;get the current pin state
	and A, 0xF0 ;clear the lower bits
	or A, [tmp] ;set the appropriate lower bits
	;now A has the right value for port 0
	mov REG[P0DATA], A ;write out value
	or  F, 0x1 ;re-enable global interrupts
	
	;now deal with the high bits, on port 1.3-1.6
	mov A, [control_pkt + CDATA+1] ;get the byte to set
	;shift left 3, to line up with bits 3-6
	asl A
	asl A
	asl A
	and A, 0x78 ;mask off everything but bits 3-6
	mov [tmp], A ;store for the moment
	
	;disable ints to avoid race conditions on the port
	and F, 0xFE ;clear global interrupt bit
	mov A, REG[P1DATA] ;get the current pin state
	and A, ~0x78 ;clear the bits we're setting
	or A, [tmp] ;set the appropriate bits
	;now A has the right value for port 1
	mov REG[P1DATA], A ;write out value
	or  F, 0x1 ;re-enable global interrupts
	
	ret

;FUNCTION pins_burst
;sets the pins to a sequence of values
;arg: control packet data byte 0 contains number of transfers
;arg: buffer contains sequence info (pin states to set)
pins_burst:
	mov [buffer_ptr], buffer ;reset to start of buffer
	;first get the number of transfers, put in X
	mov X, [control_pkt + CDATA]

pb_loop:
	;see if we're done
	jz pb_done
	;get the next byte to set
	mvi A, [buffer_ptr];
	mov [tmp], A ;keep a copy
	
	;set it up in the control packet locations--low nibble in first byte, high in second byte
	and A, 0x0F
	mov [control_pkt + CDATA], A
	
	mov A, [tmp]
	and A, 0xF0
	;shift right 4
	asr A
	asr A
	asr A
	asr A
	mov [control_pkt + CDATA + 1], A
	
	;ok, set pins
	lcall pins_set

	;delay a bit
	mov A, BURST_DELAY
pb_delay:
	jz pb_delend
	dec A
	jmp pb_delay
		
pb_delend:
	dec X
	jmp pb_loop;
	
pb_done:
	ret

;FUNCTION pins_get
;puts the pin state into the first two bytes of data buffer
;low nibble of first byte gets value of port 0 pins
;low nibble of second byte gets value of port 1 pins
pins_get:
	;read port pins 1.3-1.6 into low nibble
	mov A, REG[P1DATA] ;read the port
	;shift right 3 to line up with low nibble
	asr A
	asr A 
	asr A
	and A, 0x0F ;mask off everything  but bits we want
	mov [control_pkt + CDATA+1], A ;store in buffer
	
	;now read port pins 0.0-0.3 into low nibble of next byte
	mov A, REG[P0DATA] ;read the port
	and A, 0x0F ;mask off everything but bits 0-3
	or A, 0x80 ;set high bit, so we don't get zeros in output 
	mov [control_pkt + CDATA], A ;store in buffer
	
	ret
	
;FUNCTION pins_set_cfg
;sets the GPIO pin configuration, port 0
;arg: control data contains config byte for each pin
pins_set_cfg0:
	mov A, [control_pkt + CDATA] ;get pin config byte
	and A, PIN_CFG_MASK ;mask off the bits we will accept
	mov REG[P00CR], A ;set the pin control register
	
	mov A, [control_pkt + CDATA+1] ;get pin config byte
	and A, PIN_CFG_MASK ;mask off the bits we will accept
	mov REG[P01CR], A ;set the pin control register
	
	mov A, [control_pkt + CDATA+2] ;get pin config byte
	and A, PIN_CFG_MASK ;mask off the bits we will accept
	mov REG[P02CR], A ;set the pin control register
	
	mov A, [control_pkt + CDATA+3] ;get pin config byte
	and A, PIN_CFG_MASK ;mask off the bits we will accept
	mov REG[P03CR], A ;set the pin control register
	
	ret
	
;FUNCTION pins_set_cfg
;sets the GPIO pin configuration, port 1
;arg: control data contains config byte for each pin
pins_set_cfg1:
	mov A, [control_pkt + CDATA] ;get pin config byte
	and A, PIN_CFG_MASK ;mask off the bits we will accept
	mov REG[P13CR], A ;set the pin control register
	
	mov A, [control_pkt + CDATA+1] ;get pin config byte
	and A, PIN_CFG_MASK ;mask off the bits we will accept
	mov REG[P14CR], A ;set the pin control register
	
	mov A, [control_pkt + CDATA+2] ;get pin config byte
	and A, PIN_CFG_MASK ;mask off the bits we will accept
	mov REG[P15CR], A ;set the pin control register
	
	mov A, [control_pkt + CDATA+3] ;get pin config byte
	and A, PIN_CFG_MASK ;mask off the bits we will accept
	mov REG[P16CR], A ;set the pin control register
	
	ret
	
;FUNCTIONS pins_get_cfg0
;loads the GPIO pin configuration, port 0 into the control data area
pins_get_cfg0:
	;we set the msb to 1 on all bytes, because that's an unused
	;bit and we don't want to send zeros
	
	;load each port config
	mov A, REG[P00CR] ;port 0.0
	mov [control_pkt + CDATA], A
	
	mov A, REG[P01CR] ;port 0.1
	mov [control_pkt + CDATA+1], A
	
	mov A, REG[P02CR] ;port 0.2
	mov [control_pkt + CDATA+2], A
	
	mov A, REG[P03CR] ;port 0.3
	mov [control_pkt + CDATA+3], A
	
	ret
	
;FUNCTIONS pins_get_cfg1
;loads the GPIO pin configuration, port 1 into the control data area
pins_get_cfg1:
	;we set the msb to 1 on all bytes, because that's an unused
	;bit and we don't want to send zeros
	
	;load each port config
	mov A, REG[P13CR] ;port 1.3
	mov [control_pkt + CDATA], A
	
	mov A, REG[P14CR] ;port 1.4
	mov [control_pkt + CDATA+1], A
	
	mov A, REG[P15CR] ;port 1.5
	mov [control_pkt + CDATA+2], A
	
	mov A, REG[P16CR] ;port 1.6
	mov [control_pkt + CDATA+3], A
	
	ret

	
;FUNCTION wait
;arg: A contains wait length

wait:
	mov [loop_count_high], A ;
busy_outer_loop:
	mov [loop_count_low], 255
busy_middle_loop:
	mov A, 0xFF;
busy_inner_loop: ;count down
	dec A ;
	jnz busy_inner_loop ;
;end inner loop
	dec [loop_count_low] ;
	mov A, [loop_count_low];
	jnz busy_middle_loop;
;end middle loop
	dec [loop_count_high];
	mov A, [loop_count_high]; 
	jnz busy_outer_loop;
	ret ;done

;FUNCTION blink

;blink:
;	mov REG[P00CR], 0x1;
;blink_loop:
;	mov REG[P0DATA], 0x1;
;	mov A, 20
;	lcall wait
;	mov REG[P0DATA], 0x0;
;	mov A, 20
;	lcall wait
;	jmp blink_loop
	