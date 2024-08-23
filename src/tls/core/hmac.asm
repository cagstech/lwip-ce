;--------------------------------------------
; tls_Hmac frontend EZ80 implementation
; Author: acagliano
;--------------------------------------------
; include to supported algorithms
include "share/virtuals.inc"
include "share/helpers.asm"
include "share/clear_stack.asm"
include "share/kill_interrupts.inc"
include "hash/sha256.asm"


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
