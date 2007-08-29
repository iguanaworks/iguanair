include "m8c.inc"

export fixer_start

; addresses for SROM param block taken from the data sheet
KEY1:				EQU	0xF8
KEY2:				EQU	0xF9
BLOCKID:			EQU 0xFA
POINTER:			EQU 0xFB
CLOCK:				EQU 0xFC
DELAY:				EQU 0xFE

AREA bss
buffer:
    BLK 64
tmp1:
    BLK 1
tmp2:
    BLK 1
stack:

AREA text;_pinned (ROM, ABS, CON)
;org 0x1fc0
fixer_start:
    ; something was messed up in the stack, move it
	mov A, stack
	swap A, SP

	; lock
	and F, 0xFE

	; set the page once
	mov [tmp2], 0x33

	; read
	mov [tmp1], 0x01
	call exec_ssc

	; modify
	mov [buffer + 7], 0x09

	; erase
	mov [tmp1], 0x03
	call exec_ssc

	; write
	mov [tmp1], 0x02
	call exec_ssc

	; unlock
	or F, 0x01

exec_ssc:
	;now set up the parameter block for the SROM call
	mov [BLOCKID], [tmp2]       ; set which flash block to write
	mov [POINTER], buffer       ; write data to the buffer
	mov [CLOCK], 0x00           ; guessing at the right clock divider
	mov [DELAY], 0xAC           ; this is a guess, since datasheet says use 0x56 for 12MHz
	mov [KEY1], 0x3A;
	mov X, SP                   ; get stack pointer
	mov A, X
	add A, 3                    ; just following directions from datasheet
	mov [KEY2], A
	mov A, [tmp1]               ; load the ssc code
	ssc                         ; execute it
	ret
