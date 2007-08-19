include "loader.inc"

; default implementations do nothing
area bodyentry (ROM, ABS, CON)
org body_handler
	ret

org get_version_low
	mov A, 0x00
	ret
