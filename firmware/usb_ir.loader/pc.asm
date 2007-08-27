;pc.asm
;
;PC transceiver functions (the USB port)
;
;Copyright (C) 2006, Brian Shucker <brian@iguanaworks.net>
;Copyright (C) 2007, Joseph Dunn <jdunn@iguanaworks.net>
;
;Distribute under the GPL version 2.
;See COPYING for license details.

include "m8c.inc"       ; part specific constants and macros
include "PSoCAPI.inc"   ; PSoC API definitions for all User Modules
include "loader.inc"

; don't use the send/recv terms; those refer to the IR device!
; by convention, the PC interface uses these terms:
; read = an OUT transfer from the PC to this device
; write = an IN transfer from this device to the PC

; exported functions
export read_buffer
export check_read
export write_control
export read_control
export wait_for_IN_ready

AREA BOTTOM(ROM,ABS,CON)
    org read_buffer
    ljmp read_buffer_body

    org check_read
    ljmp check_read_body

    org write_control
    ljmp write_control_body

    org read_control
    ljmp read_control_body

    org wait_for_IN_ready
    ljmp wait_for_IN_ready_body
    
    org write_data
    ljmp write_data_body

AREA text
; FUNCTION read_buffer
; read a specified amount of data into the buffer
; size is specified in control packet data byte 0
; returns 1 if read ok, 0 if read overflow
read_buffer_body:
    mov [buffer_ptr], buffer     ; reset to start of buffer
    mov A, [control_pkt + CDATA] ; get number of bytes to read
    jz rb_done                   ; if no data to read, we're done
    mov [tmp1], A                ; keep number of bytes left in tmp1

  ; get each fragment (packet)
  rb_frag:
    mov A, [loader_flags] ; check for halt condition
    and A, FLAG_HALTED
    jnz soft_reset        ; go to known state after halt
    mov A, OUT            ; check OUT endpoint
    lcall USB_bGetEPState ; check state
    CMP A, EVENT_PENDING  ; compare--if equal, zero flag set
    jnz rb_frag           ; not equal, keep waiting

    ; get packet length
    mov A, OUT            ; check OUT endpoint
    lcall USB_bGetEPCount ; packet length now in A
    mov [tmp2], A         ; store packet length

    ; re-enable OUT endpoint
    MOV    A, OUT
    lcall  USB_EnableEP

    ; set start of data fifo
    mov X, EP2DATA

  ; copy the bytes of this packet
  rb_loop:
    cmp [buffer_ptr], buffer + BUFFER_SIZE ; check for overflow
    jz rb_overflow      ; more data, but buffer full (tx overflow)
    mov A, REG[X]       ; move data into A
    mvi [buffer_ptr], A ; move data into buffer
    dec [tmp1]          ; one less byte total
    jz rb_done          ; done with receive
    dec [tmp2]          ; one less byte to copy in this packet
    jz rb_frag          ; done with fragment, get next packet
    inc X               ; increment data pointer
    jmp rb_loop         ; keep copying from this packet

  ; we're done receiving
  rb_done:
    mov A, 1 ; return code ok
    ret

  ; had an overflow
  rb_overflow:
    mov [control_pkt + CDATA], BUFFER_SIZE ; record the number of bytes read
    mov A, 0                               ; return code overflow
    ret

; FUNCTION check_read
; returns 1 if there is data to receive, 0 otherwise
; also makes sure the endpoint stays enabled
check_read_body:
    ; disable ints to avoid race condition
    and F, 0xFE ; clear global interrupt bit

    mov A, OUT            ; check OUT endpoint
    lcall USB_bGetEPState ; check state
    cmp A, EVENT_PENDING  ; compare--if equal, zero flag set
    jz cr_yes             ; if pending, return yes
  cr_no: ; else return no
    ; re-enable endpoint, in case it was disabled by PC halt command
    ; it's safe to re-enable here because we know there's no data
    ; to get flushed out
    mov A, OUT
    lcall USB_EnableEP
    mov A, 0 ; return not ready
    jmp cr_done
  cr_yes:
    mov A, 1 ; return ready
  cr_done:
    or  F, 0x1 ; re-enable global interrupts
    ret

;FUNCTION read_control
;  receives a control packet, returning the control code
;  puts a copy of code and data into control_packet
read_control_body:
    ; zero the entire control_pkt
    mov X, 0
  rc_loop:
    mov [X + control_pkt], 0
    inc X
    mov A, X
    cmp A, PACKET_SIZE
    jnz rc_loop

    ; we already know the packet is pending (from check_recv), so just read it
    mov [tmp1], control_pkt + CCODE ; set pointer to where control code goes in packet
    mov A, OUT ; check OUT endpoint
    lcall USB_bGetEPCount ; packet length now in A
    mov [tmp2], A ; put length in tmp
    mov X, EP2DATA ; set start of data fifo
    inc [tmp2] ; because we decrement immediately
    dec X ; because we increment before byte reads

  rc_read_zero:
    dec [tmp2]
    jz rc_read_bad  ; invalid packet
    inc X
    mov A, REG[X]   ; read byte from fifo
    jz rc_read_zero ; if zero, throw away and keep reading

    dec [tmp2] ; we're off by one

    ;at this point, the next byte should be 0xCD
    cmp A, FROM_PC
    jnz rc_read_bad

    ;next byte is control code
    inc X
    mov A, REG[X]
    mvi [tmp1], A ; store in first byte of packet
    dec [tmp2]    ; we read a byte
    jz rc_read_done

    ; read rest of data into control_pkt
  rc_read_data:
    inc X
    mov A, REG[X] ; read from packet
    mvi [tmp1], A ; store
    dec [tmp2]
    jnz rc_read_data

  rc_read_done:
    ; put control code in A
    mov A, [control_pkt + CCODE]
    jmp rc_read_ret

  rc_read_bad:
    mov A, 0
    jmp rc_read_ret

  rc_read_ret:
    ; store return val
    mov [tmp1], A

    ; re-enable endpoint
    MOV A, OUT
    lcall USB_EnableEP

    ; get return val back
    mov A, [tmp1]
    ret

; FUNCTION: 
wait_for_IN_ready_body:
    mov A, [loader_flags]  ; check for halt condition
    and A, FLAG_HALTED
    jnz soft_reset         ; go to known state after halt
    mov A, IN
    lcall USB_bGetEPState  ; check IN endpoint state
    cmp A, IN_BUFFER_EMPTY ; compare -- if equal, zero flag set
    jnz wait_for_IN_ready_body  ; not equal, keep waiting
    ret

; FUNCTION: write_control
; writes a control packet back to the PC (using write_data)
;   pre: control_pkt[CCODE] is a valid response code
;        A contains the packet size
write_control_body:
    ; load packet header: 2 zero bytes followed by DC
    mov [control_pkt + 0], 0
    mov [control_pkt + 1], 0
    mov [control_pkt + 2], TO_PC

    mov X, control_pkt  ; store a pointer to the data
    jmp write_data_body ; jmp to the actual worker

; FUNCTION: write_data
; write a packet back to the PC
;   pre: X contains pointer to data
;        A contains the packet size
write_data_body:
    mov [tmp1], A ; store packet size
    mov [tmp2], X ; store data pointer

    call wait_for_IN_ready_body

    ; now send the packet
    mov [USB_APIEPNumber], IN  ; set to IN endpoint
    mov [USB_APICount], [tmp1] ; set packet size
    mov X, [tmp2]              ; retrieve data pointer
    lcall USB_XLoadEP          ; send packet
    ret
