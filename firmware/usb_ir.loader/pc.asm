;**************************************************************************
; * pc.asm ***************************************************************
; **************************************************************************
; *
; * 
; * PC transceiver functions (the USB port)
; *
; * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
; * Original Author: Brian Shucker <brian@iguanaworks.net>
; * Maintainer: Joseph Dunn <jdunn@iguanaworks.net>
; *
; * Distributed under the GPL version 2.
; * See LICENSE for license details.
; */;pc.asm
;

include "m8c.inc"       ; part specific constants and macros
include "PSoCAPI.inc"   ; PSoC API definitions for all User Modules
include "loader.inc"

; don't use the send/recv terms; those refer to the IR device!
; by convention, the PC interface uses these terms:
; read = an OUT transfer from the PC to this device
; write = an IN transfer from this device to the PC

AREA BOTTOM(ROM,ABS,CON)
    org check_read
    ljmp check_read_body

    org read_packet
    ljmp read_packet_body

    org read_buffer
    ljmp read_buffer_body

    org read_control
    ljmp read_control_body

    org wait_for_IN_ready
    ljmp wait_for_IN_ready_body

    org write_packet
    ljmp write_packet_body

    org write_control
    ljmp write_control_body

AREA text
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

; FUNCTION read_packet
read_packet_body:
    mov [tmp1], X         ; store buffer size
    mov [tmp2], A         ; store data pointer

    ; get packet length
    mov A, OUT            ; check OUT endpoint
    lcall USB_bGetEPCount ; packet length now in A
    mov [tmp3], A         ; store packet length

    ; re-enable OUT endpoint
    mov A, OUT
    lcall USB_EnableEP

    ; set start of data fifo
    mov X, EP2DATA

    ; adjust tmp1 to point to the end of the buffer
    mov A, [tmp1]
    add A, [tmp2]
    mov [tmp1], A

  read_packet_loop:
    mov A, [tmp2]           ; check for overflow
    cmp A, [tmp1]
    jz read_packet_overflow ; more data, but buffer full (tx overflow)
    mov A, REG[X]           ; move data into A
    inc X                   ; increment to the next byte
    mvi [tmp2], A           ; move data into buffer
    dec [tmp3]              ; one less byte total
    jz read_packet_done     ; done with receive
    jmp read_packet_loop    ; keep copying from this packet

  read_packet_overflow:
    mov A, 0
    ret

  read_packet_done:
    mov A, 1
    ret

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

    mov X, PACKET_SIZE    ; store buffer size
    mov A, control_pkt    ; store data pointer
    call read_packet_body ; read the data
    jz rc_read_bad        ; overflow how?

    ; read through the packet checking the format
    mov X, control_pkt - 1
    ; first skip the 0s
  rc_read_zero:
    inc X
    mov A, [X]
    jz rc_read_zero

    ; at this point, the next byte should be 0xCD
    cmp [X], FROM_PC
    jz rc_read_done

  rc_read_bad:
    mov A, 0
    jmp rc_read_ret

  rc_read_done:
    ; store control code in tmp1
    mov A, [X + 1]

  rc_read_ret:
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

; FUNCTION: write_packet
; write a packet back to the PC
;   pre: X contains pointer to data
;        A contains the packet size
write_packet_body:
    mov [tmp1], X ; store packet size
    mov [tmp2], A ; store data pointer

    call wait_for_IN_ready_body

    ; now send the packet
    mov [USB_APIEPNumber], IN  ; set to IN endpoint
    mov [USB_APICount], [tmp1] ; set packet size
    mov X, [tmp2]              ; retrieve data pointer
    lcall USB_XLoadEP          ; send packet
    ret

; FUNCTION: write_control
; writes a control packet back to the PC (using write_packet)
;   pre: control_pkt[CCODE] is a valid response code
;        A contains the packet size
write_control_body:
    ; load packet header: 2 zero bytes followed by DC
    mov [control_pkt + 0], 0
    mov [control_pkt + 1], 0
    mov [control_pkt + 2], TO_PC

    mov A, control_pkt    ; store a pointer to the data
    jmp write_packet_body ; jmp to the actual worker
