; **************************************************************************
; * ir.asm *****************************************************************
; **************************************************************************
; *
; * IR transceiver functions
; *
; * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
; * Original Author: Brian Shucker <brian@iguanaworks.net>
; * Maintainer: Joseph Dunn <jdunn@iguanaworks.net>
; *
; * Distributed under the GPL version 2.
; * See LICENSE for license details.
; */


include "m8c.inc"    ; part specific constants and macros
include "memory.inc" ; Constants & macros for SMM/LMM and Compiler
include "loader.inc"
include "body.inc"

; exported functions
export rx_disable
export rx_reset
export transmit_code
export tcap_int
export twrap_int
export write_signal
;export send_loop
export ir_repeater

; exported variables
export rx_flags
;export tx_pins
;export tx_state

AREA bss
; transmission variables
tx_pins:
    BLK 1 ; pins to use for current tx
tx_state:
    BLK 1 ; state of tx (on or off)
tx_bank:
    BLK 1 ; new or old TX_BANK?

; reception variables
rx_flags:
    BLK 2
buf_size:
    BLK 1 ; number of bytes in buffer (or waste one for cicular buffer)
read_ptr:
    BLK 1 ; where to read data from in the circular buffer

; temporary variables used in interrupt handlers
rx_high:
    BLK 1 ; received data high byte
rx_low:
    BLK 1 ; received data low byte
rx_pulse:
    BLK 1 ; high bit is 1 if this is a pulse, 0 for space

AREA text
; FUNCTION: get_byte
;   puts the next byte off the rx buffer into A
get_byte:
    mvi A, [read_ptr]                    ; read byte, increment ptr
    cmp [read_ptr], buffer + BUFFER_SIZE ; check for end of buffer
    jnz gb_dec
    mov [read_ptr], buffer ; wrap around to start of buffer

  gb_dec:
    ; we want to decrement buffer atomically
    and F, 0xFE    ; clear global interrupt bit
    dec [buf_size]
    or  F, 0x1     ; re-enable global interrupts
    ret

; FUNCTION: put_byte
; puts value into the circular buffer
;   pre: A is value to load
; does not modify A
put_byte:
    cmp [buf_size], BUFFER_SIZE ; check for overflow
    jz pb_oflow                 ; jmp handle overflow
    mvi [buffer_ptr], A         ; load the data
    inc [buf_size]              ; increment the data count

    cmp [buffer_ptr], buffer + BUFFER_SIZE ; check for end of buffer
    jnz pb_done                            ; done if we didn't hit the end
    mov [buffer_ptr], buffer               ; wrap around to start of buffer
    jmp pb_done

  pb_oflow:
    or [rx_flags], RX_OVERFLOW_FLAG ; set the overflow flag

  pb_done:
    ret

; FUNCTION: write_signal
;   writes one packet's worth of signal data from the rx buffer to host
write_signal:
    ; see if there is enough data ready
    cmp [buf_size], PACKET_SIZE - 1
    jc ws_done

    ; we use the control packet buffer to send the data
    mov X, PACKET_SIZE - 1  ; bytes to copy (last is fill)
    mov [tmp1], control_pkt ; packet pointer
  ws_ld_loop:
    call get_byte          ; get next byte
    mvi [tmp1], A
    dec X
    jnz ws_ld_loop

    ; put buffer fill level in last byte
    mov A, [buf_size]
    mvi [tmp1], A

    ; send the data packet
    mov X, PACKET_SIZE ; packet size
    mov A, control_pkt ; packet pointer
    lcall write_packet ; send the data
  ws_done:
    ret


tx_pins_off:
    call get_feature_list
    and A, HAS_LEDS | HAS_BOTH | HAS_SOCKETS | SLOT_DEV
    jnz tx_disable_new

  tx_disable_old:
    ; old --> active high --> and ~
    and REG[OLD_TX_BANK], ~OLD_TX_MASK
    jmp tx_disable_done

  tx_disable_new:
    ; new --> active low --> or
    or REG[TX_BANK], TX_MASK

  tx_disable_done:
    ret


; FUNCTION rx_disable disables the IR receiver
rx_disable:
    ; disable the timer capture (tcap) interrupt
    mov A, REG[INT_MSK1]
    and A, ~0b10000000
    mov REG[INT_MSK1], A

    ; disable the timer wrap (twrap) interrupt
    mov A, REG[INT_MSK2]
    and A, ~0b00000010
    mov REG[INT_MSK2], A

    ; make sure the transmit LEDs are OFF
    call tx_pins_off
    ret


; FUNCTION: rx_reset enables the IR receiver
rx_reset:
    ; reset a pile of variables related to reception
    and [rx_flags], ~RX_OVERFLOW_FLAG ; clear overflow flag
    mov [read_ptr], buffer   ; reset write ptr to start of buffer
    mov [buffer_ptr], buffer ; reset rx ptr to start of buffer
    mov [buf_size], 0        ; reset size to 0

    call rx_disable          ; disable no matter what

    mov A, [rx_flags]        ; check if rx should be enabled
    and A, RX_ON_FLAG
	jz rx_reset_done

    ; enable the timer capture interrupt
    mov A, REG[INT_MSK1]
    or A, 0b10000000         ; tcap interrupt enable
    mov REG[INT_MSK1], A

    ; enable the timer wrap interrupt
    mov A, [rx_flags]        ; check if we're in repeater mode
    and A, RX_REPEATER_FLAG
	jnz rx_reset_done
    mov A, REG[INT_MSK2]
    or A, 0b00000010         ; twrap interrupt enable
    mov REG[INT_MSK2], A

  rx_reset_done:
    ret


; used-to-be FUNCTION load_value (now it's a jump in - jump out)
; loads a received value into the data buffer
;  pre: rx_high and rx_low have the raw timer data
;       rx_pulse has the pulse bit set correctly
; returns: 1 if ok, 0 if overflow
load_value:
    ; shift right 6 bits
    mov A, [rx_low]
    asr A
    asr A
    asr A
    asr A
    asr A
    asr A
    ; clear the upper 6 bits (because of sign extension mask off upper bits)
    and A, 0x03
    mov [rx_low], A ; stores bits 0-1 of final result

    ; for bits 2-6, we want to use bits 0-4 of the upper byte, so shift left 2
    mov A, [rx_high]
    asl A
    asl A
    or [rx_low], A ; stores bits 2-6 of final result
    jz ld_skip_dec ; if zero, don't decrement
    ; NOTE: this gives us some minor inaccuracy
    ; decrement raw value by one, so our range is 1-128 instead of 0-127
    dec [rx_low]
  ld_skip_dec:
    ; last bit of final result is the pulse bit
    mov A, [rx_low]
    and A, 0x7F ;clear pulse bit in case it's already set
    or  A, [rx_pulse]
    mov [rx_low], A

    ; now we need to deal with long pulses: send repeated FF packets to
    ; take care of times longer than 128

    ;we need to loop for the value of bits 5-7 of the high byte
    mov A, [rx_high]
    ; shift 5 bits over
    asr A
    asr A
    asr A
    asr A
    asr A
    ; clear the upper 5 bits because of sign extension
    and A, 0x07

    ; set up the loop
    mov [rx_high], A  ; loop counter in rx_high
    jz ld_big_done    ; if zero, we don't need to do any big loads

    ; put the value to load in A
    mov A, [rx_pulse] ; set pulse bit correctly
    or A, 0x7F        ; load max value

    ; loop and load the right number of max value packets
  ld_big_loop:
    call put_byte
    dec [rx_high]
    jnz ld_big_loop

  ld_big_done:
    ; send the remainder
    mov A, [rx_low] ; get the remainder byte
    call put_byte   ; load A into buffer
    jmp tcapi_load_done



; INTERRUPT: timer/capture interrupt handler
tcap_int:
    push A

    ; read the timer in temporary variables
    mov A, REG[FRTMRL]   ; load low-order byte
    mov [rx_low], A      ; store
    mov A, REG[FRTMRH]   ; load high-order byte
    mov [rx_high], A     ; store

    ; check if we have a rising or falling edge
    mov A, REG[TCAPINTS] ; read capture interrupt status
    and A, 0x1           ; true if this is a rising edge
    jnz tcapi_rise

  ; if here, it's a falling edge
  tcapi_fall:
    mov [rx_pulse], 0x80 ; set pulse bit to indicate space
	; if we're in repeat mode then set the tx_state
	mov A, [rx_flags]
	and A, RX_REPEATER_FLAG
	jz tcapi_done
	mov [tx_state], [tx_pins] ; TODO: this seems backwards and I'm not sure why
    jmp tcapi_load_done

  ; found a rising edge
  tcapi_rise:
    mov [rx_pulse], 0x00 ; clear pulse bit to indicate pulse (seems backwards, I know)
	; if we're in repeat mode then set the tx_state
	mov A, [rx_flags]
	and A, RX_REPEATER_FLAG
	jz tcapi_done
	mov [tx_state], 0 ; TODO: this seems backwards and I'm not sure why
    jmp tcapi_load_done

  tcapi_done:
    jmp load_value          ; store into data buffer
  tcapi_load_done:
    mov REG[FRTMRL], 0      ; reset timer low byte
    mov REG[FRTMRH], 0      ; reset timer high byte
    mov REG[TCAPINTS], 0x03 ; clear int status

    pop A
    reti


; INTERRUPT: timer wrap interrupt handler
twrap_int:
    push A

    ; load an 0x80 to indicate full-length space
    mov A, 0x80
    call put_byte

    pop A
    reti


read_send_settings:
    ; does this device support channels?
    mov [tx_bank], OLD_TX_BANK
    mov [tx_pins], OLD_TX_MASK
    call get_feature_list
    and A, HAS_LEDS | HAS_BOTH | HAS_SOCKETS | SLOT_DEV
    jz read_send_done

    ; read a byte describing channel selection, and make sure it only
    ; specifies valid channels
    mov [tx_bank], TX_BANK
    and [control_pkt + CDATA + 1], TX_MASK
    mov [tx_pins], [control_pkt + CDATA + 1]
    ; if the byte was 0 then transmit on all channels
    mov A, [tx_pins]
    jnz tx_default_carrier
    mov [tx_pins], TX_MASK

  tx_default_carrier:
    ; set a default 38kHz frequency when sent 0,0
    mov A, [control_pkt + CDATA + 2]
    jnz read_send_done
    mov A, [control_pkt + CDATA + 3]
    jnz read_send_done
    mov [control_pkt + CDATA + 2], 0x06
    mov [control_pkt + CDATA + 3], 0x31

  read_send_done:
    ret


ir_repeater:
    call read_send_settings

	; set rx_flags, then make rx state actually match rx_flags
    or [rx_flags], RX_ON_FLAG | RX_REPEATER_FLAG
    call rx_reset
;    mov [tx_off_pins], REG[X]

  repeat_loop:
  	M8C_ClearWDTAndSleep
  
	; break out of the loop if we see data from the host
    lcall check_read
    jnz repeat_done

    ; transmit a long pulse/space depending on the interrupts
    mov A, 0x7F
    call send_loop
    jmp repeat_loop

  repeat_done:
    and [rx_flags], ~RX_ON_FLAG & ~RX_REPEATER_FLAG
    call rx_reset
    ret


; FUNCTION: transmit_code
; transmit the code over IR
; code format: first bit is 1 for on, 0 for off
; next 7 bits are length in 26.3uS (38KHz) increments--that's 316 clocks up, 316 down at 24MHz
transmit_code:
    call read_send_settings

    mov [buffer_ptr], buffer          ; reset to start of buffer
    mov [tx_state], 0                 ; clear tx state
    mov [tmp1], [control_pkt + CDATA] ; get number of bytes to transmit
    mov A, [tmp1]                     ; set zero flag if tmp1 is zero
    jz tx_end_pulse                   ; if zero byte, we're done

  tx_loop:
    mvi A, [buffer_ptr] ; move buffer data into A, increment pointer
    call send_loop

  tx_end_pulse:
    dec [tmp1]    ; decrement remaining byte count
    jnz tx_loop   ; if more, go to next pulse
    ret           ; done


send_loop:
    mov [tmp3], A       ; store byte
    and A, 0x7F         ; mask off the pulse length bits
    asl A               ; shift left to multiply by two due to carrier division
    mov [tmp2], A       ; store pulse length in tmp2
	mov X, [tx_bank]

    ; do not modify tx_state in repeater mode
    mov A, [rx_flags]
    and A, RX_REPEATER_FLAG
    jnz sl_pulse

    mov A, [tmp3]     ; get original byte back
    and A, 0x80       ; mask off pulse on/off bit
    jz sl_on          ; if on, jump to tx_on, else fall through
    mov [tx_state], 0 ; clear tx
    jmp sl_pulse      ; start sending pulse

  sl_on:
    mov [tx_state], [tx_pins] ; mask on tx bits
    jmp sl_pulse              ; start sending pulse--this jump seems redundant,
                        ; but is there to make timing the same on both branches

  sl_pulse: ; ready to send a pulse.  Need to AND in a XX kHz carrier
    mov A, [tmp2]       ; put pulse length into A. zero flag valid   [5 cycles]
    jz sl_end_pulse     ; this pulse is done                         [5 cycles]
    mov A, REG[X]       ; get current register state                 [7 cycles]
    xor A, [tx_state]   ; if on, toggle, else doing nothing          [6 cycles]
    mov REG[X], A       ; write change to register                   [6 cycles]
    dec [tmp2]          ; decrement remaining pulse length           [7 cycles]

;this is a set of 7-clock delays.  You jump into it at different points in
;order to get different length delays.
    ; load the bytes to skip for 4 delays
    mov A, [control_pkt + CDATA + 2] ; load the 7s delay             [5 cycles]
    ; A + argument + (PC + 1) = A + 1 + PC + 1 = A + PC + 2 = cmp
    jacc delay_7s                    ; jump to the precise offset    [7 cycles]

    ; 7 cmps for a possible delay of 7 * 7 = 49 cycles
  delay_7s:
    cmp A, [0]
    cmp A, [0]
    cmp A, [0]
    cmp A, [0]

;this is a set of 4-clock delays.  You jump into it at different points in
;order to get different length delays.
    ; load the bytes to skip for 4 delays
    mov A, [control_pkt + CDATA + 3] ; load the 4s delay             [5 cycles]
    ; A + argument + (PC + 1) = A + 1 + PC + 1 = A + PC + 2 = nop
    jacc delay_4s                    ; jump to the precise offset    [7 cycles]

    ; 100 nops for a possible delay of 4 * 100 = 400 cycles
  delay_4s:
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

    jmp sl_pulse ; continue the pulse                                [5 cycles]

  ; end of the transmit function
  sl_end_pulse:
    ; make sure tx pins are off
    call tx_pins_off
    ret           ; done
