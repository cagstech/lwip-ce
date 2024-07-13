;--------------------------------------------
; tls_Hash frontend EZ80 implementation
; code by acagliano
;--------------------------------------------
include "sha256.asm"

;------------------------------------
; Public Functions
export _tls_hash_context_create

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
tls_hashes_implemented := 2

virtual at 0
  algorithmId     rb 1
  digestLen       rb 1
  tls_hash_context_header_size:
end virtual

_tls_hash_context_create;
; dynamically allocates a tls_Hash context for use with the Hash api.
; in = algorithm_id
; out = *tls_Hash
  pop bc, hl
  push hl, bc   ; hl has hash algorithm id
  ld a, l       ; algorithm id into c
 
  ; check algorithm id valid
  ld b, tls_hashes_implemented
  cp a, b       ; a - b
  jr c, .algorithmid_ok   ; if ok, goto ok
  
  ; if not valid, return NULL
  or	a,a
	sbc	hl,hl
	ret

.algorithmid_ok:
  ; allocate context
  ld c, a     ; put a elsewhere because its about to be destroyed
  ld hl, tls_hash_total_size
  call _malloc
  or a
  add hl, de
  sbc hl, de
  ret z       ; if malloc returned null, return that ourselves
  
  ; ld a, c     ; we can put c back into a now => we can actually just leave it in c
  lea ix, hl
  ld (ix + 0), c   ; set ctx->algorithm_id
  
  ; set digest length for algorithm
  ld hl, tls_hash_digest_len
  ld b, 0
  ; ld c, a
  add hl, bc  ; hl should now point to correct digest len
  ld (ix + 1), (hl)   ; ctx->digest_len
  
  ; now we call to the hash init algorithm with ctx->state
  pea ix
    pea ix + 2
      ld hl, tls_hash_init_funcs
      ld b, 0
      ;ld c, a
      add hl, bc        ; get the correct hash init function
      call _indcallhl   ; call it
    pop de  ; we no longer need that
  pop hl    ; that we need to return to the caller
  ret
  
  
  
  ; calculate size of tls_Hash base struct + state => cLength
  ; algorithm id => algorithm_id
  
  ; length of hash digest => digest_len
  ; call algorithm init func
  ; return ptr tls_Hash

tls_hash_context_update:
  ; call (*update) for tls_Hash->state

tls_hash_context_digest:
  ; call (*digest) for tls_Hash->state
  


tls_hash_digest_len:
  db 20
  db 32
  
tls_hash_init_funcs:
  dl hash_sha1_init
  dl hash_sha256_init

tls_hash_update_funcs:
  dl hash_sha1_update
  dl hash_sha256_update
  
tls_hash_digest_funcs:
  dl hash_sha1_digest
  dl hash_sha256_digest
  

extern _malloc
