include "constants.inc"

export control_pkt
export buffer
export buffer_ptr
export tmp1
export tmp2

AREA lib_bss               (RAM, ABS, CON)
    org FIRST_FLASH_VAR - BUFFER_SIZE - 1 - 1 - 1

; temporary variables might as well be shared to save space
tmp1:
	BLK 1
tmp2:
	BLK 1

buffer_ptr:
	BLK 1 ; current index into buffer

buffer:
	BLK BUFFER_SIZE ; the main data buffer

control_pkt:
	BLK PACKET_SIZE ; control packet buffer
