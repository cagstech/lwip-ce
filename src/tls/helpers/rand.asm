;--------------------------------------------
; Secure RNG
; Author: acagliano
;--------------------------------------------
; include to supported algorithms
include "clear_stack.asm"
include "kill_interrupts.asm"
include "../hashes/sha256.asm"

;------------------------------------
; Public Functions
export _tls_random_init_entropy
export _tls_random
export _tls_random_bytes

;-------------------------

;-------------------------------------
; bool tls_random_init(void);
_tls_random_init_entropy:
	ld hl, $D65800
	ld iy, 0
	ld de,$FFFFFF
	ld c, l
.loop1: ; loops 256 times
	call .test_byte
	dec c
	jr nz,.loop1
.loop2: ; loops 256 times
	call .test_byte
	dec c
	jr nz,.loop2
	call .test_byte ; run a total of 513 times

	lea hl, iy+0
	ld (_sprng_read_addr), hl

	add hl, bc
	xor a, a
	sbc hl, bc  ; set the z flag if HL is 0
	ret z
	inc a
	ret

; test byte at hl, set iy=hl if entropy is better
.test_byte:
	push de
	ld de,0
	ld b,7 ; probably enough tests
.test_byte_outer_loop:
; sample byte twice and bitwise-xor
	ld a,(hl) ; sample 1
	xor a,(hl) ; sample 2
; test the entropy for each of the 8 bits
.test_byte_loop:
	jr z,.done_test_byte_loop
	add a,a ; test next bit (starting with the high bit)
	jr nc,.test_byte_loop ; jump if bit unset
	inc de ; increment score
	jr .test_byte_loop
.done_test_byte_loop:
	djnz .test_byte_outer_loop

	ex (sp),hl ; save pointer to byte, restore current entropy score
; check if the new entropy score is higher than the current entropy score
	or a,a
	sbc hl,de ; current - new
	jr nc,.test_byte_is_worse
	pop iy ; return iy = pointer to byte
	lea hl,iy+1 ; advance pointer to byte for next test
	ret
.test_byte_is_worse:
	ex hl,de ; de = current score
	pop hl ; restore pointer to byte
	inc hl ; advance pointer
	ret


;--------------------------------------
; uint32_t tls_random(void);
_tls_random:
	save_interrupts
; set rand to 0
	ld hl, 0
	ld a, l
	ld (_sprng_rand), hl
	ld (_sprng_rand+3), a
	
    ; there is no realistic way to differentiate a error of 0 from a
    ; totally random return of 0. 1/2^32 is stupid small, but non-zero.
    ; ensure that any internal use of this RNG calls init_entropy first
    ; trust users to do that as well.
 
	ld hl,(_sprng_read_addr)
	ld de, _sprng_entropy_pool
	ld b, 119
.byte_read_loop:
	ld a, (hl)
	xor a, (hl)
	xor a, (hl)
	xor a, (hl)
	xor a, (hl)
	xor a, (hl)
	xor a, (hl)
	ld (de), a
	inc de
	djnz .byte_read_loop

	ld hl, _sprng_hash_ctx
	push hl
	call hash_sha256_init
	pop bc
	ld hl, 119
	push hl
	ld hl, _sprng_entropy_pool
	push hl
	push bc
	call hash_sha256_update
	pop bc, hl, hl
	ld hl, _sprng_sha_digest
	push hl
	push bc
	call hash_sha256_final
	pop bc, hl
	
	; xor hash cyclically into _rand
	ld hl,_sprng_sha_digest
	ld de,_sprng_rand
	ld c,4
.outer:
	xor a,a
	ld b,8
.inner:
	xor a,(hl)
	inc hl
	djnz .inner
	ld (de),a
	inc de
	dec c
	jq nz,.outer
	; destroy sprng state
	ld hl, _sprng_entropy_pool
	ld (hl), 0
	ld de, _sprng_entropy_pool + 1
	ld bc, _sprng_rand - _sprng_entropy_pool - 1
	ldir
	ld hl, (_sprng_rand)
	ld a, (_sprng_rand+3)
	ld e, a
.return:
	restore_interrupts _tls_random
	ret


_tls_random_bytes:
	save_interrupts
	ld	hl, -10
	call	ti._frameset
	ld	de, (ix + 9)
	ld	iy, 0
	ld	(ix + -7), de
.lbl1:
	lea	hl, iy + 0
	or	a, a
	sbc	hl, de
	jq	nc, .lbl2
	ld	(ix + -10), iy
	call	_tls_random
	ld	(ix + -4), hl
	ld	(ix + -1), e
	ld	de, (ix + -10)
	ld	iy, (ix + 6)
	add	iy, de
	ld	bc, (ix + -7)
	push	bc
	pop	hl
	ld	de, 4
	or	a, a
	sbc	hl, de
	jq	c, .lbl5
	ld	bc, 4
.lbl5:
	push	bc
	pea	ix + -4
	push	iy
	call	ti._memcpy
	pop	hl
	pop	hl
	pop	hl
	ld	iy, (ix + -10)
	ld	de, 4
	add	iy, de
	ld	de, -4
	ld	hl, (ix + -7)
	add	hl, de
	ld	(ix + -7), hl
	ld	de, (ix + 9)
	jq	.lbl1
.lbl2:
	ld	sp, ix
	pop	ix

	restore_interrupts _tls_random_bytes
	ret


; will have to manually edit context in this instance
_sprng_entropy_pool.size = 119
virtual at $E30800
	_sprng_entropy_pool     rb _sprng_entropy_pool.size
	_sprng_sha_digest       rb 32
	_sprng_sha_mbuffer      rb (64*4)
	_sprng_hash_ctx         rb _sha256ctx_size
	_sprng_rand             rb 4
	_sprng_read_addr        rb 3
end virtual
