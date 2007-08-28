export fixer_start

; addresses for SROM param block taken from the data sheet
KEY1:				EQU	0xF8
KEY2:				EQU	0xF9
BLOCKID:			EQU 0xFA
POINTER:			EQU 0xFB
CLOCK:				EQU 0xFC
DELAY:				EQU 0xFE

AREA bss
    BLK 100
buffer:
    BLK 64
tmp1:
    BLK 1
tmp3:
    BLK 1

AREA text
fixer_start:
    and F, ~0x01

	mov [tmp3], 51

	; do a block read
	mov A, 0x01 ;read block 
	call exec_ssc

	; change that one byte
	mov [buffer + 7], 0x09

	; do a block erase
	mov A, 0x03 ;erase block code
	call exec_ssc

	;now set up the parameter block for the SROM call
	mov A, 0x02 ;write block code
	call exec_ssc

    or F, 0x01
    ljmp 0 ; hard reset the device

; pre: tmp3 holds the block index
;      A holds the ssc code
exec_ssc:
	; store the ssc code
	mov [tmp1], A

	;now set up the parameter block for the SROM call
	mov [KEY1], 0x3A;
	mov X, SP                   ; get stack pointer
	mov A, X
	add A, 3                    ; just following directions from datasheet
	mov [KEY2], A
	mov [BLOCKID], [tmp3] ; set which flash block to write
	mov [POINTER], buffer       ; write data to the buffer
	mov [CLOCK], 0x00           ; guessing at the right clock divider
	mov [DELAY], 0xAC           ; this is a guess, since datasheet says use 0x56 for 12MHz
	mov A, [tmp1]               ; load the ssc code
	ssc                         ; execute it
	ret
