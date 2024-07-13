;--------------------------------------------
; tls_Hmac frontend EZ80 implementation
; Author: acagliano
;--------------------------------------------
; include to supported algorithms
include "sha256.asm"

;------------------------------------
; Public Functions
export _tls_hmac_context_create
export _tls_hmac_update
export _tls_hmac_digest

;-------------------------
; call hl
_indcallhl:
; Calls HL
; Inputs:
;  HL : Address to call
	jp	(hl)

; ------------------------------------------------------
; Define total size of hash context (including state)
tls_hmac_context_state_size := _sha256ctx_size  ; change this if the largest context changes
tls_hmac_total_size := tls_hash_context_state_size + tls_hash_context_header_size
tls_hashes_implemented := 2

;--------------------------------
; Define hash header format
virtual at 0
	algorithmId     rb 1
	digestLen       rb 1
	iPad			rb 64
	oPad			rb 64
	tls_hmac_context_header_size:
end virtual
