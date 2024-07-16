;--------------------------------------------
; tls_Hash frontend EZ80 implementation
; Author: acagliano
;--------------------------------------------
; include to supported algorithms
include "sha256.asm"

;------------------------------------
; Public Functions
export _tls_hash_context_create
export _tls_hash_update
export _tls_hash_digest

;-------------------------
; call hl
_indcallhl:
; Calls HL
; Inputs:
;  HL : Address to call
	jp	(hl)

; ------------------------------------------------------
; Define total size of hash context (including state)
tls_hash_context_state_size := _sha256ctx_size  ; change this if the largest context changes
tls_hash_total_size := tls_hash_context_state_size + tls_hash_context_header_size
tls_hashes_implemented := 1

;--------------------------------
; Define hash header format
virtual at 0
	algorithmId     rb 1
	digestLen       rb 1
	tls_hash_context_header_size:
end virtual

;----------------------------------------------------------------------
; struct tls_Hash *tls_hash_context_create(uint8_t algorithm_id);
_tls_hash_context_create;
; dynamically allocates a tls_Hash context for use with the Hash api.
; in = algorithm_id
; out = *tls_Hash
; destroys: hl, bc, ix, a, flags
	pop bc, hl
	push hl, bc   ; hl has hash algorithm id
	ld a, l       ; algorithm id into c

	; check algorithm id valid
	ld b, tls_hashes_implemented
	cp a, b       ; a - b               ; check if a < b
	jr c, .algorithmid_ok               ; if carry set, a < b, then goto ok
	or	a,a
	sbc	hl,hl
	ret           ; return NULL;
.algorithmid_ok:
	ld b, 0     ; we can do this, push bc before _malloc, pop it after
	ld c, a     ; save a in c (allows up to remove a few ld c, a instructions later)
	; allocate context
	ld hl, tls_hash_total_size
	push bc
	call _malloc    ; malloc the size of the hash header + state struct
	pop bc
	add hl, bc
	or a
	sbc hl, bc  ; if hl == 0
	ret z       ; return NULL;
	push hl
	pop ix    ; hl into ix
	ld (ix + 0), c   ; set ctx->algorithm_id = algorithm_id
	ld l, c
	ld h, 3
	mlt hl
	push hl
	pop bc          ; bc should have correct offset
	; set digest length for algorithm, bc saved earlier
	ld hl, tls_hash_digest_len
	add hl, bc  ; hl should now point to correct digest len
	ld (ix + 1), (hl)   ; ctx->digest_len
	; now we call to the hash init algorithm with ctx->state
	pea ix
		pea ix + tls_hash_context_header_size
			ld hl, tls_hash_init_funcs
			add hl, bc        ; get the correct hash init function
			call _indcallhl   ; call it
		pop de  ; we no longer need that
	pop hl    ; that we need to return to the caller
	ret

;---------------------------------------------------------------------------------
; bool tls_hash_update(struct tls_Hash *context, void *data, size_t len);
_tls_hash_update:
; updates a hash context for input data
; in: context, data, len
; out: bool
; destroys: hl, bc, ix, iy, a, flags
	call  ti._frameset0
	; (ix+0) return vector
	; (ix+3) old ix
	; (ix+6) context
	; (ix+9) data
	; (ix+12) len
	; validate algorithm_id again. users like to do all sorts of mess.
	ld iy, (ix + 6)
	ld a, (iy + 0)
	ld b, tls_hashes_implemented
	cp a, b       ; a - b               ; check if a < b
	jr c, .algorithmid_ok               ; if carry set, a < b, then goto ok
	xor a
	jr .exit        ; return false;
.algorithmid_ok:
	ld l, a			; algorithm id from a into l
	ld h, 3			; * 3
	mlt hl			; hl = l * h
	push hl
	pop bc          ; bc should have correct offset
	; push ctx->state, data, len for inner call
	ld hl, (ix + 12)
	push hl
		ld hl, (ix + 9)
		push hl
			pea iy + tls_hash_context_header_size   ; iy from earlier
				ld hl, (iy + 3)
				; get update func for hash
				ld hl, tls_hash_update_funcs
				add hl, bc					; get the correct hash update function
				call _indcallhl
				ld a, 1
	pop hl,hl,hl
.exit:
	ld sp, ix
	pop ix
	ret

;---------------------------------------------------------------------------------
; bool tls_hash_digest(struct tls_Hash *context, void *digest);
_tls_hash_digest:
; returns a digest from hash state
; in: context, digest buffer
; out: bool
; destroys: hl, bc, ix, iy, a, flags
	ld hl, -tls_hash_context_state_size
	call ti._frameset0
	; (ix+0) return vector
	; (ix+3) old ix
	; (ix+6) context
	; (ix+9) outbuf
	; validate algorithm_id again. users like to do all sorts of mess.
	ld iy, (ix + 6)
	ld a, (iy)
	ld b, tls_hashes_implemented
	cp a, b       ; a - b               ; check if a < b
	jr c, .algorithmid_ok               ; if carry set, a < b, then goto ok
	xor a
	jr .exit        ; return false;
.algorithmid_ok:
	ld l, a			; algorithm id from a into l
	ld h, 3			; * 3
	mlt hl			; hl = l * h
	push hl
	pop bc          ; bc should have correct offset
	ld hl, (ix + 9)
	push hl
		pea iy + tls_hash_context_header_size
			ld hl, tls_hash_digest_funcs
			add hl, bc					; get the correct hash digest function
			call _indcallhl
	pop hl,hl
	ld a, 1
.exit:
	ld sp, ix
	pop ix
	ret


;-------------------------------------------------
; Lookups for hash digest, functions

; LUT for hash digest lengths
tls_hash_digest_len:
;	db 20
	db 32

; LUT for hash init
tls_hash_init_funcs:
	dl hash_sha256_init
	; dl hash_sha384_init

; LUT for hash update
tls_hash_update_funcs:
	dl hash_sha256_update
	; dl hash_sha384_update

; LUT for hash digest
tls_hash_digest_funcs:
	dl hash_sha256_digest
	; dl hash_sha384_digest


extern _malloc
