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

;-------------------------------------
; bool tls_random_init(void);
_tls_random_init_entropy:
; ix = selected byte
; de = current deviation
; hl = starting address
; inputs: stack = samples / 4, Max is 256 (256*4 = 1024 samples)
; outputs: hl = address
	ld de, 256		; thorough sampling
.start:
	ld a, e
	ld (_smc_samples), a
 
	push ix
	   ld ix, 0
	   ld hl, $D65800
	   ld bc,513
.test_range_loop:
	   push bc
		  call _test_byte
	   pop bc
	   cpi
	   jq pe,.test_range_loop
 
	   lea hl, ix+0
	   ld (_sprng_read_addr), hl
 
	   xor a, a
	   sbc hl, bc  ; subtract 0 to set the z flag if HL is 0
	pop ix
	ret z
	inc a
	ret
 
_test_byte:
; inputs: hl = byte
; inputs: de = minimum deviance
; inputs: ix = pointer to the byte with minimum deviance
; outputs: de is the new minimum deviance (if updated)
; outputs: ix updated to hl if this byte contains the bit with lowest deviance
; outputs: b = 0
; outputs: a = 0x86
; destroys: f
; modifies: a, b, de, ix
	ld a,0x46 ; second half of the `bit 0,(hl)` command
.test_byte_bitloop:
	push hl
	   push de
		  call _test_bit  ; HL = deviance (|desired - actual|)
	   pop de
 
	   add a,8       ; never overflows, so resets carry
	   sbc hl, de    ; check if HL is smaller than DE
 
	   jq nc, .skip_next_bit          ; HL >= DE
	   add hl,de
	   ex de,hl
	   pop ix
	   push ix
.skip_next_bit:
	pop hl
	cp 0x86
	jq nz, .test_byte_bitloop
	ret
  
_test_bit:
; inputs: a = second byte of CB**
; inputs: hl = byte
; outputs: hl = hit count
; destroys: af, bc, de, hl
 
_smc_samples:=$+1
	ld b,0
	ld (.smc1),a
	ld (.smc2),a
	ld (.smc3),a
	ld (.smc4),a
	ld de,0
.loop:
	bit 0,(hl)
.smc1:=$-1
	jq z,.next1
	inc de
.next1:
	bit 0,(hl)
.smc2:=$-1
	jq nz,.next2    ; notice the inverted logic !
	dec de          ; and the dec instead of inc !
.next2:
	bit 0,(hl)
.smc3:=$-1
	jq z, .next3
	inc de
.next3:
	bit 0,(hl)
.smc4:=$-1
	jq nz,.next4    ; notice the inverted logic !
	dec de          ; and the dec instead of inc !
.next4:
	djnz .loop
 
	; return |DE|
	or a,a
	sbc hl,hl
	sbc hl,de
	ret nc
	ex de,hl
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
_sprng_read_addr:        rb 3
_sprng_entropy_pool.size = 119
virtual at $E30800
	_sprng_entropy_pool     rb _sprng_entropy_pool.size
	_sprng_sha_digest       rb 32
	_sprng_sha_mbuffer      rb (64*4)
	_sprng_hash_ctx         rb _sha256ctx_size
	_sprng_rand             rb 4
end virtual
