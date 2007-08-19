include "constants.inc"

export control_pkt
export buffer
export buffer_ptr
export tmp1
export tmp2

; TODO: must pin these down
area bss
buffer:
	BLK BUFFER_SIZE ; the main data buffer
buffer_ptr:
	BLK 1 ; current index into buffer

; temporary variables might as well be shared to save space
tmp1:
	BLK 1
tmp2:
	BLK 1

AREA pkt_bss               (RAM, ABS, CON)
    org FIRST_FLASH_VAR
control_pkt:
	BLK PACKET_SIZE ; control packet buffer
