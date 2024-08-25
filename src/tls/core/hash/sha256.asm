;--------------------------------------------
; SHA-256 EZ80 implementation
; Author: beckadamtheinventor
;--------------------------------------------

assume adl=1
section .text

include "../share/virtuals.inc"
include "../share/nointerrupts.inc"

public hash_sha256_init
public hash_sha256_update
public hash_sha256_final

_sha256_m_buffer := _sprng_sha_mbuffer

; reverse b longs endianness from iy to hl
_sha256_reverse_endianness:
	ld a, (iy + 0)
	ld c, (iy + 1)
	ld d, (iy + 2)
	ld e, (iy + 3)
	ld (hl), e
	inc hl
	ld (hl), d
	inc hl
	ld (hl), c
	inc hl
	ld (hl), a
	inc hl
	lea iy, iy + 4
	djnz _sha256_reverse_endianness
	ret
 
; helper macro to xor [B,C] with [R1,R2] storing into [R1,R2]
; destroys: af
macro _xorbc? R1,R2
	ld a,b
	xor a,R1
	ld R1,a
	ld a,c
	xor a,R2
	ld R2,a
end macro

; helper macro to xor [(HL),(HL+1)] with [R1,R2] storing into [R1,R2], advancing HL
; destroys: af
macro _xorihl? R1,R2
	ld a,R1
	xor a,(hl)
	ld R1,a
	inc hl
	ld a,R2
	xor a,(hl)
	ld R2,a
	inc hl
end macro

; helper macro to xor [(R3), (R3+1)] with [R1,R2] storing into [R1,R2]
; destroys: af
macro _xoriix? R1,R2, R3
	ld a,R1
	xor a,(R3)
	ld R1,a
	ld a,R2
	xor a,(R3+1)
	ld R2,a
end macro

; helper macro to and [(R3), (R3+1)] with [R1,R2] storing into [R1,R2]
; destroys: af
macro _andiix? R1,R2, R3
	ld a,R1
	and a,(R3)
	ld R1,a
	ld a,R2
	and a,(R3+1)
	ld R2,a
end macro

; helper macro to or [(R3), (R3+1)] with [R1,R2] storing into [R1,R2]
; destroys: af
macro _oriix? R1,R2, R3
	ld a,R1
	or a,(R3)
	ld R1,a
	ld a,R2
	or a,(R3+1)
	ld R2,a
end macro

; helper macro to add [(R3), (R3+1)] with [R1,R2] storing into [R1,R2]
; destroys: af
macro _addiix? R1,R2, R3
	ld a,R1
	add a,(R3)
	ld R1,a
	ld a,R2
	adc a,(R3+1)
	ld R2,a
end macro

; helper macro to adc [(R3), (R3+1)] with [R1,R2] storing into [R1,R2]
; destroys: af
macro _adciix? R1,R2, R3
	ld a,R1
	adc a,(R3)
	ld R1,a
	ld a,R2
	adc a,(R3+1)
	ld R2,a
end macro

; helper macro to add [(R3), (R3+1)] with [R1,R2] storing into [(R3), (R3+1)]
; destroys: af
macro _addiix_store? R1,R2, R3
	ld a,R1
	add a,(R3)
	ld R1,a
	ld a,R2
	adc a,(R3+1)
	ld R2,a
end macro

; helper macro to adc [(R3), (R3+1)] with [R1,R2] storing into [(R3), (R3+1)]
; destroys: af
macro _adciix_store? R1,R2, R3
	ld a,R1
	adc a,(R3)
	ld (R3),a
	ld a,R2
	adc a,(R3+1)
	ld (R3+1),a
end macro

; helper macro to xor [R1,R2] with [R3,R4] storing into [R1,R2]
; destroys: af
macro _xor? R1,R2,R3,R4
	ld a,R1
	xor a,R3
	ld R1,a
	ld a,R2
	xor a,R4
	ld R2,a
end macro

; helper macro to or [R1,R2] with [R3,R4] storing into [R1,R2]
; destroys: af
macro _or? R1,R2,R3,R4
	ld a,R1
	or a,R3
	ld R1,a
	ld a,R2
	or a,R4
	ld R2,a
end macro

; helper macro to and [R1,R2] with [R3,R4] storing into [R1,R2]
; destroys: af
macro _and? R1,R2,R3,R4
	ld a,R1
	and a,R3
	ld R1,a
	ld a,R2
	and a,R4
	ld R2,a
end macro

; helper macro to add [R1,R2] with [R3,R4] storing into [R1,R2]
; destroys: af
macro _add? R1,R2,R3,R4
	ld a,R1
	add a,R3
	ld R1,a
	ld a,R2
	adc a,R4
	ld R2,a
end macro

; helper macro to adc [R1,R2] with [R3,R4] storing into [R1,R2]
; destroys: af
macro _adc? R1,R2,R3,R4
	ld a,R1
	adc a,R3
	ld R1,a
	ld a,R2
	adc a,R4
	ld R2,a
end macro

; helper macro to add [B,C] with [R1,R2] storing into [R1,R2]
; destroys: af
; note: this will add including the carry flag, so be sure of what the carry flag is before this
; note: if you're chaining this into a number longer than 16 bits, the order must be low->high
macro _addbclow? R1,R2
	ld a,c
	add a,R2
	ld R2,a
	ld a,b
	adc a,R1
	ld R1,a
end macro
macro _addbchigh? R1,R2
	ld a,c
	adc a,R2
	ld R2,a
	ld a,b
	adc a,R1
	ld R1,a
end macro

; helper macro to move [d,e,h,l] <- [l,e,d,h] therefore shifting 8 bits right.
; destroys: af
macro _rotright8?
	ld a,l
	ld l,h
	ld h,e
	ld e,d
	ld d,a
end macro

; helper macro to move [d,e,h,l] <- [e,h,l,d] therefore shifting 8 bits left.
; destroys: af
macro _rotleft8?
	ld a,d
	ld d,e
	ld e,h
	ld h,l
	ld l,a
end macro


; #define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
;input: [d,e,h,l], b
;output: [d,e,h,l]
;destroys: af, b
_ROTLEFT:
	xor a,a
	rl l
	rl h
	rl e
	rl d
	adc a,l
	ld l,a
	djnz .
	ret

; #define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))
;input: [d,e,h,l], b
;output: [d,e,h,l]
;destroys: af, b
_ROTRIGHT:
	xor a,a
	rr d
	rr e
	rr h
	rr l
	rra
	or a,d
	ld d,a
	djnz .
	ret


; initialize sha256 hash state
hash_sha256_init:
	pop iy,de
	push de
	ld hl,$FF0000
	ld bc,sha256_offset_state
	ldir
	ld c,8*4
	ld hl,_sha256_state_init
	ldir
	ld a, 1
	jp (iy)


hash_sha256_update:
	save_interrupts
	call __frameset0
	; (ix + 0) RV
	; (ix + 3) old IX
	; (ix + 6) arg1: ctx
	; (ix + 9) arg2: data
	; (ix + 12) arg3: len
	ld iy, (ix + 6)			; iy = context, reference
		; start writing data to the right location in the data block
	ld a, (iy + sha256_offset_datalen)
	ld bc, 0
	ld c, a
	; scf
	; sbc hl,hl
	; ld (hl),2
	; get pointers to the things
	ld de, (ix + 9)			; de = source data
	ld hl, (ix + 6)			; hl = context, data ptr
	add hl, bc
	ex de, hl ;hl = source data, de = context / data ptr
	ld bc, (ix + 12)		   ; bc = len
	call _sha256_update_loop
	cp a,64
	call z,_sha256_update_apply_transform
	ld iy, (ix + 6)
	ld (iy + sha256_offset_datalen), a		   ;save current datalen
	pop ix
	restore_interrupts hash_sha256_update
	ret
 
 _sha256_update_loop:
	inc a
	ldi ;ld (de),(hl) / inc de / inc hl / dec bc
	ret po ;return if bc==0 (ldi decrements bc and updates parity flag)
	cp a,64
	call z,_sha256_update_apply_transform
	jq _sha256_update_loop
_sha256_update_apply_transform:
	push hl, de, bc
	ld bc, (ix + 6)
	push bc
	call _sha256_transform	  ; if we have one block (64-bytes), transform block
	pop iy
	ld bc, 512				  ; add 1 blocksize of bitlen to the bitlen field
	push bc
	pea iy + sha256_offset_bitlen
	call u64_addi
	pop bc, bc, bc, de, hl
	xor a,a
	ld de, (ix + 6)
	ret
 

 hash_sha256_final:
	save_interrupts
	ld hl,-_sha256ctx_size
	call __frameset
	; ix-_sha256ctx_size to ix-1
	; (ix + 0) Return address
	; (ix + 3) saved IX
	; (ix + 6) arg1: ctx
	; (ix + 9) arg2: outbuf
	ld iy, (ix + 6)					; iy =  context block
	lea hl, iy
	lea de, ix-_sha256ctx_size
	ld bc, _sha256ctx_size
	ldir
	ld bc, 0
	ld c, (iy + sha256_offset_datalen)     ; data length
	lea hl, ix-_sha256ctx_size					; ld hl, context_block_cache_addr
	add hl, bc						; hl + bc (context_block_cache_addr + bytes cached)
	ld a,55
	sub a,c ;c is set to datalen earlier
	ld (hl),$80
	jq c, _sha256_final_over_56
	inc a
_sha256_final_under_56:
	ld b,a
	xor a,a
_sha256_final_pad_loop2:
	inc hl
	ld (hl), a
	djnz _sha256_final_pad_loop2
	jq _sha256_final_done_pad
_sha256_final_over_56:
	ld a, 64
	sub a,c
	ld b,a
	xor a,a
_sha256_final_pad_loop1:
	inc hl
	ld (hl), a
	djnz _sha256_final_pad_loop1
	push iy
	call _sha256_transform
	pop de
	ld hl,$FF0000
	ld bc,56
	ldir
_sha256_final_done_pad:
	lea iy, ix-_sha256ctx_size
	ld c, (iy + sha256_offset_datalen)
	ld b,8
	mlt bc ;multiply 8-bit datalen by 8-bit value 8
	push bc
	pea iy + sha256_offset_bitlen
	call u64_addi
	pop bc,bc
	lea iy, ix-_sha256ctx_size ;ctx
	lea hl,iy + sha256_offset_bitlen
	lea de,iy + sha256_offset_data + 63
	ld b,8
_sha256_final_pad_message_len_loop:
	ld a,(hl)
	ld (de),a
	inc hl
	dec de
	djnz _sha256_final_pad_message_len_loop
	push iy ;ctx
	call _sha256_transform
	pop iy
	ld hl, (ix + 9)
	lea iy, iy + sha256_offset_state
	ld b, 8
	call _sha256_reverse_endianness
	ld sp,ix
	pop ix
	restore_interrupts hash_sha256_final
	ret
 
 ; #define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
;input [d,e,h,l]
;output [d,e,h,l]
;destroys af, bc
_SIG0:
	ld b,3
	call _ROTRIGHT  ;rotate long accumulator 3 bits right
	push hl,de	  ;save value for later
	ld b,4
	call _ROTRIGHT  ;rotate long accumulator another 4 bits right for a total of 7 bits right
	push hl,de	  ;save value for later
	_rotright8      ;rotate long accumulator 8 bits right
	ld b,3
	call _ROTRIGHT  ;rotate long accumulator another 3 bits right for a total of 18 bits right
	pop bc
	_xorbc d,e  ;xor third ROTRIGHT result with second ROTRIGHT result (upper 16 bits)
	pop bc
	_xorbc h,l  ;xor third ROTRIGHT result with second ROTRIGHT result (lower 16 bits)
	pop bc
	ld a,b
	and a,$1F   ;cut off the upper 3 bits from the result of the first ROTRIGHT call
	xor a,d	 ;xor first ROTRIGHT result with result of prior xor (upper upper 8 bits)
	ld d,a
	ld a,c
	xor a,e	 ;xor first ROTRIGHT result with result of prior xor (lower upper 8 bits)
	ld e,a
	pop bc
	_xorbc h,l  ;xor first ROTRIGHT result with result of prior xor (lower 16 bits)
	ret

; #define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))
;input: [d,e,h,l]
;output: [d,e,h,l]
;destroys: af, bc
_SIG1:
	_rotright8      ;rotate long accumulator 8 bits right
	ld b,2
	call _ROTRIGHT  ;rotate long accumulator 2 bits right for a total of 10 bits right
	push hl,de	    ;save value for later
	_rotright8      ;rotate long accumulator 8 bits right for a total of 18 bits right
	ld b,1
	call _ROTLEFT  ;rotate long accumulator a bit left for a total of 17 bits right
	push hl,de	  ;save value for later
	ld b,2
	call _ROTRIGHT  ;rotate long accumulator another 2 bits right
	pop bc
	_xorbc d,e  ;xor third ROTRIGHT result with second ROTRIGHT result (upper 16 bits)
	pop bc
	_xorbc h,l  ;xor third ROTRIGHT result with second ROTRIGHT result (lower 16 bits)

	;we're cutting off upper 10 bits of first ROTRIGHT result meaning we're xoring by zero, so we can just keep the value of d.
	pop bc
	ld a,c
	and a,$3F   ;cut off the upper 2 bits from the lower upper byte of the first ROTRIGHT result.
	xor a,e	 ;xor first ROTRIGHT result with result of prior xor (lower upper upper 8 bits)
	ld e,a
	pop bc
	_xorbc h,l  ;xor first ROTRIGHT result with result of prior xor (lower 16 bits)
	ret


; void _sha256_transform(SHA256_CTX *ctx);
_sha256_transform:
._h := -4
._g := -8
._f := -12
._e := -16
._d := -20
._c := -24
._b := -28
._a := -32
._state_vars := -32
._tmp1 := -36
._tmp2 := -40
._i := -41
._frame_offset := -41
	ld hl,._frame_offset
	call __frameset
	ld hl,_sha256_m_buffer
	add hl,bc
	or a,a
	sbc hl,bc
	jq z,._exit
	ld iy,(ix + 6)
	ld b,16
	call _sha256_reverse_endianness ;first loop is essentially just reversing the endian-ness of the data into m (both represented as 32-bit integers)

	ld iy,_sha256_m_buffer
	lea iy, iy + 16*4
	ld b, 64-16
._loop2:
	push bc
; m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
	ld hl,(iy + -2*4 + 0)
	ld de,(iy + -2*4 + 2)
	call _SIG1
	push de,hl
	ld hl,(iy + -15*4 + 0)
	ld de,(iy + -15*4 + 2)
	call _SIG0

; SIG0(m[i - 15]) + m[i - 16]
	ld bc, (iy + -16*4 + 0)
	_addbclow h,l
	ld bc, (iy + -16*4 + 2)
	_addbchigh d,e

; + SIG1(m[i - 2])
	pop bc
	_addbclow h,l
	pop bc
	_addbchigh d,e

; + m[i - 7]
	ld bc, (iy + -7*4)
	_addbclow h,l
	ld bc, (iy + -7*4 + 2)
	_addbchigh d,e

; --> m[i]
	ld (iy + 3), d
	ld (iy + 2), e
	ld (iy + 1), h
	ld (iy + 0), l

	lea iy, iy + 4
	pop bc
	djnz ._loop2


	ld iy, (ix + 6)
	lea hl, iy + sha256_offset_state
	lea de, ix + ._state_vars
	ld bc, 8*4
	ldir				; copy the ctx state to scratch stack memory (uint32_t a,b,c,d,e,f,g,h)

	ld (ix + ._i), c
._loop3:
; tmp1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
; CH(e,f,g)
; #define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
	lea iy,ix
	ld b,4
._loop3inner1:
	ld a, (iy + ._e)
	xor a,$FF
	and a, (iy + ._g)
	ld c,a
	ld a, (iy + ._e)
	and a, (iy + ._f)
	xor a,c
	ld (iy + ._tmp1),a
	inc iy
	djnz ._loop3inner1

	; scf
	; sbc hl,hl
	; ld (hl),2

; EP1(e)
; #define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
	ld hl,(ix + ._e + 0)
	ld de,(ix + ._e + 2)
	ld b,6    ;rotate e 6 bits right
	call _ROTRIGHT
	push de,hl
	ld b,5    ;rotate accumulator another 5 bits for a total of 11
	call _ROTRIGHT
	push de,hl
	_rotright8 ;rotate accumulator another 8 bits for a total of 19
	ld b,6
	call _ROTRIGHT ;rotate accumulator another 6 bits for a total of 25
	pop bc         ;ROTRIGHT(x,11) ^ ROTRIGHT(x,25)
	_xorbc h,l
	pop bc
	_xorbc d,e
	pop bc         ; ^ ROTRIGHT(x,6)
	_xorbc h,l
	pop bc
	_xorbc d,e

; EP1(e) + h
	ld bc, (ix + ._h)
	_addbclow h,l
	ld bc, (ix + ._h + 2)
	_addbchigh d,e

; h + EP1(e) + CH(e,f,g)
	ld bc, (ix + ._tmp1)
	_addbclow h,l
	ld bc, (ix + ._tmp1 + 2)
	_addbchigh d,e

; B0ED BDD0
	push de,hl
	ld hl,_sha256_m_buffer
	ld b,4
	ld c,(ix + ._i)
	mlt bc
	add hl,bc
	ld de,(hl)
	inc hl
	inc hl
	ld hl,(hl)
	push hl,de
	ld hl,_sha256_k
	add hl,bc
	ld de,(hl)
	inc hl
	inc hl
	ld hl,(hl)

; m[i] + k[i]
	pop bc
	_addbclow d,e
	pop bc
	_addbchigh h,l

; m[i] + k[i] + h + EP1(e) + CH(e,f,g)
	pop bc
	_addbclow d,e
	pop bc
	_addbchigh h,l

; --> tmp1
	ld (ix + ._tmp1 + 3),h
	ld (ix + ._tmp1 + 2),l
	ld (ix + ._tmp1 + 1),d
	ld (ix + ._tmp1 + 0),e

; tmp2 = EP0(a) + MAJ(a,b,c);
; MAJ(a,b,c)
; #define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
	lea iy,ix
	ld b,4
._loop3inner2:
	ld a, (iy + ._a)
	and a, (iy + ._b)
	ld c,a
	ld a, (iy + ._a)
	and a, (iy + ._c)
	xor a,c
	ld c,a
	ld a, (iy + ._b)
	and a, (iy + ._c)
	xor a,c
	ld (iy + ._tmp2), a
	inc iy
	djnz ._loop3inner2

; EP0(a)
; #define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
	ld hl,(ix + ._a + 0)
	ld de,(ix + ._a + 2)
	ld b,2
	call _ROTRIGHT     ; x >> 2
	push de,hl
	_rotright8         ; x >> 10
	ld b,3
	call _ROTRIGHT     ; x >> 13
	push de,hl
	_rotright8         ; x >> 21
	inc b ;_ROTRIGHT sets b to zero
	call _ROTRIGHT     ; x >> 22
	pop bc             ; (x >> 22) ^ (x >> 13)
	_xorbc h,l
	pop bc
	_xorbc d,e
	pop bc             ; (x >> 2) ^ (x >> 22) ^ (x >> 13)
	_xorbc h,l
	pop bc
	_xorbc d,e

	ld bc, (ix + ._tmp2)  ; EP0(a) + MAJ(a,b,c)
	_addbclow h,l
	ld bc, (ix + ._tmp2 + 2)
	_addbchigh d,e
	ld (ix + ._tmp2 + 3), d
	ld (ix + ._tmp2 + 2), e
	ld (ix + ._tmp2 + 1), h
	ld (ix + ._tmp2 + 0), l

; h = g;
	ld hl, (ix + ._g + 0)
	ld a,  (ix + ._g + 3)
	ld (ix + ._h + 0), hl
	ld (ix + ._h + 3), a

; g = f;
	ld hl, (ix + ._f + 0)
	ld a,  (ix + ._f + 3)
	ld (ix + ._g + 0), hl
	ld (ix + ._g + 3), a

; f = e;
	ld hl, (ix + ._e + 0)
	ld a,  (ix + ._e + 3)
	ld (ix + ._f + 0), hl
	ld (ix + ._f + 3), a

; e = d + tmp1;
	ld hl, (ix + ._d + 0)
	ld a,  (ix + ._d + 3)
	ld de, (ix + ._tmp1 + 0)
	or a,a
	adc hl,de
	adc a, (ix + ._tmp1 + 3)
	ld (ix + ._e + 0), hl
	ld (ix + ._e + 3), a

; d = c;
	ld hl, (ix + ._c + 0)
	ld a,  (ix + ._c + 3)
	ld (ix + ._d + 0), hl
	ld (ix + ._d + 3), a

; c = b;
	ld hl, (ix + ._b + 0)
	ld a,  (ix + ._b + 3)
	ld (ix + ._c + 0), hl
	ld (ix + ._c + 3), a

; b = a;
	ld hl, (ix + ._a + 0)
	ld a,  (ix + ._a + 3)
	ld (ix + ._b + 0), hl
	ld (ix + ._b + 3), a

; a = tmp1 + tmp2;
	ld hl, (ix + ._tmp1 + 0)
	ld a,  (ix + ._tmp1 + 3)
	ld de, (ix + ._tmp2 + 0)
	or a,a
	adc hl,de
	adc a, (ix + ._tmp2 + 3)
	ld (ix + ._a + 0), hl
	ld (ix + ._a + 3), a
	ld a,(ix + ._i)
	inc a
	ld (ix + ._i),a
	cp a,64
	jq c,._loop3

	push ix
	ld iy, (ix + 6)
	lea iy, iy + sha256_offset_state
	lea ix, ix + ._state_vars
	ld b,8
._loop4:
	ld hl, (iy + 0)
	ld de, (ix + 0)
	ld a, (iy + 3)
	or a,a
	adc hl,de
	adc a,(ix + 3)
	ld (iy + 0), hl
	ld (iy + 3), a
	lea ix, ix + 4
	lea iy, iy + 4
	djnz ._loop4

	pop ix
._exit:
	ld sp,ix
	pop ix
	ret

_sha256_state_init:
	dl 648807
	db 106
	dl 6794885
	db -69
	dl 7271282
	db 60
	dl 5240122
	db -91
	dl 938623
	db 81
	dl 354444
	db -101
	dl -8136277
	db 31
	dl -2044647
	db 91
 
 _sha256_k:
	dd	1116352408
	dd	1899447441
	dd	3049323471
	dd	3921009573
	dd	961987163
	dd	1508970993
	dd	2453635748
	dd	2870763221
	dd	3624381080
	dd	310598401
	dd	607225278
	dd	1426881987
	dd	1925078388
	dd	2162078206
	dd	2614888103
	dd	3248222580
	dd	3835390401
	dd	4022224774
	dd	264347078
	dd	604807628
	dd	770255983
	dd	1249150122
	dd	1555081692
	dd	1996064986
	dd	2554220882
	dd	2821834349
	dd	2952996808
	dd	3210313671
	dd	3336571891
	dd	3584528711
	dd	113926993
	dd	338241895
	dd	666307205
	dd	773529912
	dd	1294757372
	dd	1396182291
	dd	1695183700
	dd	1986661051
	dd	2177026350
	dd	2456956037
	dd	2730485921
	dd	2820302411
	dd	3259730800
	dd	3345764771
	dd	3516065817
	dd	3600352804
	dd	4094571909
	dd	275423344
	dd	430227734
	dd	506948616
	dd	659060556
	dd	883997877
	dd	958139571
	dd	1322822218
	dd	1537002063
	dd	1747873779
	dd	1955562222
	dd	2024104815
	dd	2227730452
	dd	2361852424
	dd	2428436474
	dd	2756734187
	dd	3204031479
	dd	3329325298



extern u64_addi
extern __frameset0
extern __frameset
