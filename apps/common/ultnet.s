; cc65 binding for the GEOS-U64 Ultimate jump table ($FE50)
;
; unsigned __fastcall__ ultcall(unsigned char a, unsigned addr);
;   calls addr with A = a; returns A (value) in the low byte and
;   X (error code, 0 = ok) in the high byte. Parameters in r0-r3 are
;   set by the caller through the R0..R3 macros in ultnet.h.

	.export _ultcall
	.import popa

_ultcall:
	sta call+1
	stx call+2
	jsr popa
call:	jsr $ffff
	rts
