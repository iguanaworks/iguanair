;pc.asm
;
;PC transceiver functions (the USB port)
;
;Copyright (C) 2006, Brian Shucker <brian@iguanaworks.net>
;
;Distribute under the GPL version 2.
;See COPYING for license details.


include "m8c.inc"       ; part specific constants and macros
include "memory.inc"    ; Constants & macros for SMM/LMM and Compiler
include "PSoCAPI.inc"   ; PSoC API definitions for all User Modules
include "constants.inc"

;by convention, the PC interface uses these terms:
;read = an OUT transfer from the PC to this device
;write = an IN transfer from this device to the PC

;don't use the send/recv terms; those refer to the IR device!

export read_buffer;
export write_signal;
export write_data;
export check_read;
export write_control;
export read_control;
export control_pkt
export buffer

AREA bss

buffer:
	BLK BUFFER_SIZE ;the data buffer
control_pkt:
	BLK PACKET_SIZE ;control packet buffer
to_copy:
	BLK 1 ;temp var for copy operations
	
AREA text
	
;FUNCTION read_buffer
;read a specified amount of data into the buffer
;size is specified in control packet data byte 0
;returns 1 if read ok, 0 if read overflow
read_buffer:
	mov [buffer_ptr], buffer ;reset to start of buffer
	mov A, [control_pkt + CDATA] ;get number of bytes to read
	;check against buffer size
	jz rb_done ;if no data to read, we're done
	mov [tmp2], A ;bytes left in tmp2
rb_frag: //get each fragment (packet)
	lcall read_packet; get packet--puts packet length in A	
	mov X, EP2DATA ;set start of data fifo
	mov [to_copy], A; bytes to copy for this packet
rb_loop: //copy the bytes of this packet
	;check for overflow
	cmp [buffer_ptr], buffer + BUFFER_SIZE
	jz rb_overflow ;more data, but buffer full (tx overflow)
	mov A, REG[X]; move data into A
	mvi [buffer_ptr], A ;move data into packet
	dec [tmp2]; one less byte total
	jz rb_done; done with receive
	dec [to_copy]; one less byte to copy in this packet
	jz rb_frag; done with fragment, get next packet
	inc X; increment data pointer
	jmp rb_loop ;keep copying from this packet
rb_done: ;we're done receiving
	mov A, 1 ;return code ok
	ret
rb_overflow: ;had an overflow
	mov [control_pkt + CDATA], BUFFER_SIZE ;record the number of bytes actually read
	mov A, 0 ;return code overflow
	ret
	
;FUNCTION check_read
;returns 1 if there is data to receive, 0 otherwise
;also makes sure the endpoint stays enabled
check_read:
	;disable ints to avoid race condition
	and F, 0xFE ;clear global interrupt bit
	
	mov A, 0x2; ;check endpoint 2
	lcall USB_bGetEPState ;check state
	CMP A, EVENT_PENDING ;compare--if equal, zero flag set
	jz check_yes ;if pending, return yes
check_no: ;else return no
	;re-enable endpoint, in case it was disabled by PC halt command
	;it's safe to re-enable here because we know there's no data
	;to get flushed out
	MOV		A, 2 ;out is enpoint 2
	lcall  USB_EnableEP 
	;return zero
	mov A, 0
	jmp check_done
check_yes:
	mov A, 1
check_done:
	or  F, 0x1 ;re-enable global interrupts
	ret

;FUNCTION read_packet
;returns packet length

read_packet:	
	;wait for receive
read_wait:
	mov A, [halted];check for halt condition
	jnz soft_reset ;go to known state after halt
	mov A, 0x2; ;check endpoint 2
	lcall USB_bGetEPState ;check state
	CMP A, EVENT_PENDING ;compare--if equal, zero flag set
	jnz read_wait ;not equal, keep waiting
	
	;get packet length
	mov A, 0x2; ;check endpoint 2
	lcall USB_bGetEPCount ; packet length now in A
	mov [tmp], A ;store packet length
	;re-enable endpoint
	MOV		A, 2 ;out is enpoint 2
	lcall  USB_EnableEP 
	mov A, [tmp] ;get packet length back
	ret ;return packet length

;FUNCTION read_control
;receives a control packet; returns the control code
;puts a copy of code and data into control_packet
read_control:
	;we already know the packet is pending (from check_recv), so just read it
	mov [tmp], control_pkt + CCODE ;set pointer to where control code goes in packet
	mov A, 0x2; ;check endpoint 2
	lcall USB_bGetEPCount ; packet length now in A
	mov [to_copy], A ;put length in tmp
	mov X, EP2DATA ;set start of data fifo
	inc [to_copy] ;because we decrement immediately
	dec X ;because we increment before byte reads
rc_read_zero:
	dec [to_copy]
	jz rc_read_bad ;invalid packet
	inc X
	mov A, REG[X] ;read byte from fifo
	jz rc_read_zero ;if zero, throw away and keep reading
	
	dec [to_copy] ;we're off by one
	
	;at this point, the next byte should be 0xCD
	cmp A, 0xCD ;TODO: #define this
	jnz rc_read_bad
	
	;next byte is control code
	inc X
	mov A, REG[X]
	mvi [tmp], A ;store in first byte of packet
	dec [to_copy] ;we read a byte
	jz rc_read_done
	
	;read rest of data into control_pkt
rc_read_data:
	inc X
	mov A, REG[X] ;read from packet
	mvi [tmp], A ;store
	dec [to_copy]
	jnz rc_read_data
	
rc_read_done:
	;put control code in A
	mov A, [control_pkt + CCODE]
	jmp rc_read_ret
	
rc_read_bad:
	mov A, 0
	jmp rc_read_ret
	
rc_read_ret: ;common retunr point
	mov [tmp], A ;store return val
	;re-enable endpoint
	MOV		A, 2 ;out is enpoint 2
	lcall  USB_EnableEP 
	mov A, [tmp] ;get return val back
	ret

;FUNCTION write_control
;writes a control packet
;argument: A is packet size
;pre: control packet already contans a valid control packet
write_control:
	mov [tmp], A ;store packet size
	;load packet header: zero byte followed by DC
	mov [control_pkt], 0
	mov [control_pkt+1], 0
	mov [control_pkt+2], 0xDC
	
write_cwait:
	mov A, [halted];check for halt condition
	jnz soft_reset ;go to known state after halt
	mov A, 0x1; ;check endpoint 1
	lcall USB_bGetEPState ;check state
	CMP A, IN_BUFFER_EMPTY ;compare--if equal, zero flag set
	jnz write_cwait ;not equal, keep waiting
	;now send the packet
	mov [USB_APIEPNumber], 0x1 ;set to endpoint 1
	mov [USB_APICount], [tmp] ;set packet size
	mov X, control_pkt ;put packet address into X
	lcall USB_XLoadEP ;send packet
	ret

;FUNCTION get_byte
;puts the next byte off the rx buffer into A
get_byte:
	mvi A, [write_ptr] ;read byte, increment ptr
	cmp [write_ptr], buffer + BUFFER_SIZE ;check for end of buffer
	jz get_wrap
get_dec:
	;we want to decrement buffer atomically
	and F, 0xFE ;clear global interrupt bit
	dec [buf_size] ;decrement buffer size
	;if we no longer have a full packet worth of data, clear rx_fill flag
	cmp [buf_size], PACKET_SIZE - 1
	jz get_clear
get_decdone:
	or  F, 0x1 ;re-enable global interrupts
	jmp get_done
get_clear:
	mov [rx_fill], 0
	jmp get_decdone

get_wrap:
	mov [write_ptr], buffer ;wrap around to start of buffer
	jmp get_dec	

get_done:
	ret

;FUNCTION write_signal
;writes one packet's worth of signal data from the rx buffer to host	
write_signal:
	;we'll use the control packet buffer to send the data
	mov X, PACKET_SIZE-1 ;bytes to copy ;REMOVE -1 
	mov [tmp], control_pkt ;packet pointer
ws_ld_loop:
	lcall get_byte ;get next byte
	mvi [tmp], A
	dec X
	jnz ws_ld_loop
	
	;put buffer fill level in last byte
	mov A, [buf_size]
	mvi [tmp], A
	
	;send the data packet
	mov X, control_pkt ;packet pointer
	lcall write_data ;send the data
	ret
	
;FUNCTION write full data packet (size = PACKET_SIZE)
;pre: X contains pointer to data
write_data:
	mov [tmp2], X ;store data pointer
wd_loop:
	mov A, [halted];check for halt condition
	jnz soft_reset ;go to known state after halt
	mov A, 0x1; ;check endpoint 1
	lcall USB_bGetEPState ;check state
	CMP A, IN_BUFFER_EMPTY ;compare--if equal, zero flag set
	jnz wd_loop ;not equal, keep waiting
	
	;now send the packet
	mov [USB_APIEPNumber], 0x1 ;set to endpoint 1
	mov [USB_APICount], PACKET_SIZE ;set packet size
	mov X, [tmp2] ;retrieve data pointer
	lcall USB_XLoadEP ;send packet
	ret
		