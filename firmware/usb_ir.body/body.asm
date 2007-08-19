include "m8c.inc"       ; part specific constants and macros
include "constants.inc"
include "body.inc"

VERSION_ID_LOW:  equ 0x01 ; firmware version ID low byte (code body)

body_main:
	; misc functions
	cmp A, CTL_GETID
	jz getid_body
	cmp A, CTL_EXECUTE
	jz execute_body
	cmp A, CTL_GETBUFSIZE
	jz get_buf_size_body

	; receive functions
	cmp A, CTL_RECVON
	jz recv_on_body
	cmp A, CTL_RECVOFF
	jz recv_off_body
	cmp A, CTL_RECV

	; send functions
	cmp A, CTL_SEND
	jz send_body
	cmp A, CTL_SETCHANNELS
	jz set_channels_body
	cmp A, CTL_GETCHANNELS
	jz get_channels_body

	; pin functions
	cmp A, CTL_GETPINCONFIG
	jz get_pin_config_body
	cmp A, CTL_SETPINCONFIG
	jz set_pin_config_body
	cmp A, CTL_GETPINS
	jz get_pins_body
	cmp A, CTL_SETPINS
	jz set_pins_body
	cmp A, CTL_BULKPINS
	jz bulk_pins_body

	; that's everything we handle
  body_main_ret:
	ret

get_version_low_body:
	mov A, VERSION_ID_LOW
	ret





getid_body:
	jmp body_main_ret

execute_body:
	jmp body_main_ret

get_buf_size_body:
	jmp body_main_ret

recv_on_body:
	jmp body_main_ret

recv_off_body:
	jmp body_main_ret

send_body:
	jmp body_main_ret

set_channels_body:
	jmp body_main_ret

get_channels_body:
	jmp body_main_ret

get_pin_config_body:
	jmp body_main_ret

set_pin_config_body:
	jmp body_main_ret

get_pins_body:
	mov [control_pkt + CCODE], CTL_GETPINS
	mov A, CTL_BASE_SIZE + 2
	lcall write_control
	jmp body_main_ret

set_pins_body:
	jmp body_main_ret

bulk_pins_body:
	jmp body_main_ret





; pin down the entry points 
area bodyentry (ROM, ABS, CON)
org body_handler
	ljmp body_main

org get_version_low
;	ljmp get_version_low_body
