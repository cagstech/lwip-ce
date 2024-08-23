
assume adl=1

section .text
public _indcallhl
;-------------------------
; call hl
_indcallhl:
; Calls HL
; Inputs:
;  HL : Address to call
	jp	(hl)
