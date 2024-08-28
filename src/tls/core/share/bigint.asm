
assume adl=1

section .text
public u64_addi
; void u64_addi(uint64_t *a, uint64_t *b);
u64_addi:
	pop bc,hl,de
	push de,hl,bc
	xor a,a
	ld bc,(hl)
	ex hl,de
	adc hl,bc
	ex hl,de
	ld (hl),de
	inc hl
	inc hl
	inc hl
	ld b,5
	ld c,a
.loop:
	ld a,(hl)
	adc a,c
	ld (hl),a
	inc hl
	djnz .loop
	ret


section .text
public _gf128_mul
_gf128_mul:
; Galois-Field GF(2^128) multiplication routine
; little endian fields expected
	ld hl, -16
	call __frameset
	lea de, ix - 16		; stack mem?
	ld hl, (ix + 6)		; op1 (save a copy)
	ld bc, 16
	ldir				; ix - 32 = tmp = op1
 
	; zero out output
	ld de, (ix + 12)		; output
	xor a
	ld (de), a
	inc de
	ld hl, (ix + 12)
	ld bc, 15
	ldir
 
	ld hl, (ix + 9)		; op2 = for bit in bits
	ld b, 0
	ld c, 16
.loop_op2:
	ld a, (hl)
	push hl
		ld b, 8
.loop_bits_in_byte:
		rla
		push af
			sbc a,a
			push bc
				ld c,a
 
				; add out (res) + tmp
				ld hl, (ix + 12)		; hl = (dest)
				lea de, ix - 16		; de = tmp (src)
				ld b, 16
.loop_add:
				ld a, (de)
				and a, c
				xor a, (hl)
				ld (hl), a
				inc hl
				inc de
				djnz .loop_add
 
			; now double tmp
				lea hl, ix - 16		; tmp in hl	little endian
			 ld b, 16
			 or a                ; reset carry
.loop_mul2:
			 rr (hl)
			 inc hl		; little endian
			 djnz .loop_mul2
 
			 ; now xor with polynomial x^128 + x^7 + x^2 + x + 1
			 ; if bit 128 set, xor least-significant byte with 10000111b
 
			 sbc a, a
			 and a, 11100001b
			 xor a, (ix - 16)		; little endian
			 ld (ix - 16), a
 
.no_xor_poly:
			pop bc
		pop af
		djnz .loop_bits_in_byte
	pop hl
	inc hl		; little endian
	dec c
	jr nz, .loop_op2
	ld sp, ix
	pop ix
	ret

extern __frameset
