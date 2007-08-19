; all packet types supported by this version of the code body:
; misc functions
CTL_GETID:			EQU 0x10
CTL_EXECUTE:		EQU 0x11
CTL_GETBUFSIZE:		EQU 0x12
; receive functions
CTL_RECVON:			EQU 0x13
CTL_RECVOFF:		EQU 0x14
CTL_RECV:			EQU 0x15
; send functions
CTL_SEND:			EQU 0x16
CTL_SETCHANNELS:	EQU 0x17
CTL_GETCHANNELS:	EQU 0x18
; pin functions
CTL_GETPINCONFIG:	EQU 0x19
CTL_SETPINCONFIG:	EQU 0x1A
CTL_GETPINS:		EQU 0x1B
CTL_SETPINS:		EQU 0x1C
CTL_BULKPINS:		EQU 0x1D
; errors
CTL_OVERRECV:		EQU 0x1E
CTL_OVERSEND:		EQU 0x1F

; constants used by the body code
TX_BANK:			EQU P1DATA
TX_MASK:			EQU 0xF0
