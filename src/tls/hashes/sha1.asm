

virtual at 0
	sha1_offset_data     rb 64
	sha1_offset_datalen  rb 1
	sha1_offset_bitlen   rb 8
	sha1_offset_state    rb 4*5
	_sha1ctx_size:
end virtual


; void hash_sha1_init(SHA1_CTX *ctx);
hash_sha1_init:
	pop iy,de
	push de
	ld hl,$FF0000
	ld bc,sha1_offset_state
	ldir
	ld c,4*5
	ld hl,_sha1_state_init
	ldir
	ld a, 1
_indcall:
; Calls IY
	jp (iy)


; void hash_sha1_update(SHA1_CTX *ctx);
hash_sha1_update:
	save_interrupts

	call ti._frameset0
	; (ix + 0) RV
	; (ix + 3) old IX
	; (ix + 6) arg1: ctx
	; (ix + 9) arg2: data
	; (ix + 12) arg3: len

	ld iy, (ix + 6) ; iy = context
	
	ld a, (iy + offset_datalen)
	ld bc, 0
	ld c, a
	
	ld de, (ix+9)
	ld hl, (ix+6)
	add hl, bc
	ex de, hl

	ld bc, (ix + 12)
	call _sha1_update_loop
	cp a,64
	call z, _sha1_update_apply_transform
	
	ld iy, (ix+6)
	ld (iy + offset_datalen), a
	pop ix

	restore_interrupts hash_sha1_update
	ret


_sha1_update_loop:
	inc a
	ldi ;ld (de),(hl) / inc de / inc hl / dec bc
	ret po ;return if bc==0 (ldi decrements bc and updates parity flag)
	cp a,64
	call z, _sha1_update_apply_transform
	jr _sha1_update_loop
_sha1_update_apply_transform:
	push hl, de, bc
	ld bc, (ix + 6)
	push bc
	call _sha1_transform	  ; if we have one block (64-bytes), transform block
	pop iy
	ld bc, 512				  ; add 1 blocksize of bitlen to the bitlen field
	push bc
	pea iy + offset_bitlen
	call u64_addi
	pop bc, bc, bc, de, hl
	xor a,a
	ld de, (ix + 6)
	ret
	
; void hashlib_Sha1Final(SHA1_CTX *ctx, BYTE hash[]);
hash_sha1_final:
	save_interrupts

	ld hl,-_sha1ctx_size
	call ti._frameset
	; ix-_sha1ctx_size to ix-1
	; (ix + 0) Return address
	; (ix + 3) saved IX
	; (ix + 6) arg1: ctx
	; (ix + 9) arg2: outbuf
	
	; scf
	; sbc hl,hl
	; ld (hl),2

	ld iy, (ix + 6)					; iy =  context block
	lea hl, iy
	lea de, ix-_sha1ctx_size
	ld bc, _sha1ctx_size
	ldir

	ld bc, 0
	ld c, (iy + offset_datalen)     ; data length
	lea hl, ix-_sha1ctx_size					; ld hl, context_block_cache_addr
	add hl, bc						; hl + bc (context_block_cache_addr + bytes cached)

	ld a,55
	sub a,c ;c is set to datalen earlier
	ld (hl),$80
	jq c, _sha1_final_over_56
	inc a
_sha1_final_under_56:
	ld b,a
	xor a,a
_sha1_final_pad_loop2:
	inc hl
	ld (hl), a
	djnz _sha1_final_pad_loop2
	jr _sha1_final_done_pad
_sha1_final_over_56:
	ld a, 64
	sub a,c
	ld b,a
	xor a,a
_sha1_final_pad_loop1:
	inc hl
	ld (hl), a
	djnz _sha1_final_pad_loop1
	push iy
	call _sha1_transform
	pop de
	ld hl,$FF0000
	ld bc,56
	ldir
_sha1_final_done_pad:
	lea iy, ix-_sha1ctx_size
	ld c, (iy + offset_datalen)
	ld b,8
	mlt bc ;multiply 8-bit datalen by 8-bit value 8
	push bc
	pea iy + offset_bitlen
	call u64_addi
	pop bc,bc

	lea iy, ix-_sha1ctx_size ;ctx
	lea hl,iy + offset_bitlen
	lea de,iy + offset_data + 63

	ld b,8
_sha1_final_pad_message_len_loop:
	ld a,(hl)
	ld (de),a
	inc hl
	dec de
	djnz _sha1_final_pad_message_len_loop

	push iy ; ctx
	call _sha1_transform
	pop iy

	ld hl, (ix + 9)
	lea iy, iy + offset_state
	ld b, 5
	call _sha256_reverse_endianness

	ld sp,ix
	pop ix

	restore_interrupts hash_sha1_final
	ret

; void _sha1_transform(SHA256_CTX *ctx);
_sha1_transform:
._e := -4
._d := -8
._c := -12
._b := -16
._a := -20
._state_vars := -20
._i := -21
._frame_offset := -21
_sha1_w_buffer := _sha256_m_buffer ; reuse m buffer from sha256 as w
	ld hl,._frame_offset
	call ti._frameset
	ld hl,_sha1_k
	ld (.sha1_k_smc),hl
	ld hl,_sha1_w_buffer
	ld (.step_3_smc),hl
	add hl,bc
	or a,a
	sbc hl,bc
	jq z,_sha256_transform._exit
	ld iy,_sha1_w_buffer
	ld b, 16
	call _sha256_reverse_endianness ; essentially just reversing the endian-ness of the data into w
	ld iy, (ix + 6)
	lea hl, iy + offset_state
	lea de, ix + ._state_vars
	ld bc, 4*5
	ldir
	ld (ix + ._i), 16
.loop1:
	ld a, (ix + ._i)
	cp a, 80
	jq c,.done_loop1
	or a,a
	sbc hl,hl
	ld l,a
	ld de, _sha1_w_buffer - 3*4
	add hl,de ; &w[i-3]
	push hl
	ld de,-5*4
	add hl,de ; &w[i-8]
	push hl
	ld de,-6*4
	add hl,de ; &w[i-14]
	push hl
	ld de,-2*4
	add hl,de ; &w[i-16]

; w[i-16]
	ld bc,(hl)
	inc hl
	inc hl
	ld de,(hl)

; ^ w[i-14]
	pop hl
	_xorihl b,c ; low 16 bits
	_xorihl d,e ; high 16 bits

; ^ w[i-8]
	pop hl
	_xorihl b,c
	_xorihl d,e

; ^ w[i-3]
	pop hl
	_xorihl b,c
	_xorihl d,e

	ld h,b
	ld l,c
	ld b,1
	call _ROTLEFT
	jq .loop1

.done_loop1:
	ld (ix + ._i), 0
.loop2:
	ld a, (ix + ._i)
	cp a, 20
	jq nc,.i_gteq_20
; low word of ((~b) & d) | (b & c)
	ld de, (ix + ._d + 0)
	ld a, (ix + ._b + 0)
	cpl
	and a,e
	ld e,a
	ld a, (ix + ._b + 1)
	cpl
	and a,d
	ld d,a
	ld bc, (ix + ._b + 0)
	_andiix b,c, ix+._b+0
	ld a,c
	or a,e
	ld l,a
	ld a,b
	or a,d
	ld h,a
; high word of ((~b) & d) | (b & c)
	ld de, (ix + ._d + 2)
	ld a, (ix + ._b + 2)
	cpl
	and a,e
	ld e,a
	ld a, (ix + ._b + 3)
	cpl
	and a,d
	ld d,a
	ld bc, (ix + ._b + 2)
	_andiix b,c, ix+._b+2
	ld a,c
	or a,e
	ld e,a
	ld a,b
	or a,d
	ld d,a
	jq .step_3
.i_gteq_20:
	cp a,40
	jr nc,.i_gteq_40
.i_gteq_60:
	ld hl, (ix + ._b + 0)
	ld de, (ix + ._b + 2)
	_xoriix h,l, ix+._c+0
	_xoriix d,e, ix+._c+2
	_xoriix h,l, ix+._d+0
	_xoriix d,e, ix+._d+2
	jr .step_3
.i_gteq_40:
	cp a,60
	jr nc,.i_gteq_60
; low word of (b&c) | (b&d) | (c&d)
	ld hl, (ix + ._b + 0)
	_andiix h,l, ix+._c+0
	ld de, (ix + ._b + 0)
	_andiix d,e, ix+._d+0
	_or h,l,d,e
	ld de, (ix + ._b + 0)
	_andiix d,e, ix+._d+0
	_or h,l,d,e
; high word of (b&c) | (b&d) | (c&d)
	ld de, (ix + ._b + 0)
	_andiix d,e, ix+._c+0
	ld bc, (ix + ._b + 0)
	_andiix b,c, ix+._d+0
	_or d,e,b,c
	ld bc, (ix + ._b + 0)
	_andiix b,c, ix+._d+0
	_or d,e,b,c

.step_3:
	ld iy,_sha1_w_buffer
.step_3_smc := $-3
	_addiix h,l, ix+._e+0
	_adciix d,e, ix+._e+2
	_addiix h,l, iy+0
	_adciix d,e, iy+2
	ld iy,_sha1_k
.sha1_k_smc := $-3
	_addiix h,l, iy+0
	_adciix d,e, iy+2
	push de,hl
	ld hl,(ix + ._a + 0)
	ld de,(ix + ._a + 2)
	ld b,5
	call _ROTLEFT ; rotl32(a, 5)
	pop bc
	_add h,l,b,c
	pop bc
	_adc d,e,b,c
	; d,e,h,l = rotl32(a, 5) + f(t, b,c,d) + e + w[s] + k[t/20];
	push de,hl
	; _e = _d; _d = _c; _c = _b; _b = _a;
	lea hl, ix + ._d + 3
	lea de, ix + ._e + 3
	ld bc,4*4
	lddr
	ld hl,(ix + ._c + 0)
	ld de,(ix + ._c + 2)
	ld b,32-30
	call _ROTRIGHT ; rotl32(b, 30)
	ld (ix + ._c + 0),l
	ld (ix + ._c + 1),h
	ld (ix + ._c + 2),e
	ld (ix + ._c + 3),d
	pop hl,de
	ld (ix + ._a + 0),l
	ld (ix + ._a + 1),h
	ld (ix + ._a + 2),e
	ld (ix + ._a + 3),d
	ld a, (ix + ._i)
	inc a
	ld (ix + ._i), a
	cp a,80
	jr z,.done
	cp a,20
	jr z,.next_k
	cp a,40
	jr z,.next_k
	cp a,60
	jr nz,.not_next_k
.next_k:
	ld hl,(.sha1_k_smc)
	inc hl
	ld (.sha1_k_smc),hl
.not_next_k:
	ld hl,(.step_3_smc)
	inc hl
	and a,$F
	jr nz,.set_step_3_smc
	ld hl,_sha1_w_buffer
.set_step_3_smc:
	ld (.step_3_smc),hl
	jq .loop2
.done:
	ld iy, (ix + 6)
	
repeat 5
	ld hl,(ix + ._a + ((% - 1) * 4 + 0))
	ld de,(ix + ._a + ((% - 1) * 4 + 2))
	_addiix_store h,l, (iy + offset_state + ((% - 1) * 4 + 0))
	_adciix_store d,e, (iy + offset_state + ((% - 1) * 4 + 2))
end repeat

	ld sp,ix
	pop ix
	ret
