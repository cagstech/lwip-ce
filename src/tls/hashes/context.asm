
tls_hash_context_create;
  ; calculate size of tls_Hash base struct + state => cLength
  ; algorithm id => algorithm_id
  ; pointer to algorithm update func => (*update)
  ; pointer to algorithm digest func => (*digest)
  ; length of hash digest => digest_len
  ; call algorithm init func
  ; return ptr tls_Hash

tls_hash_context_update:
  ; call (*update) for tls_Hash->state

tls_hash_context_digest:
  ; call (*digest) for tls_Hash->state
  
  
tls_hmac_context_create;
  ; calculate size of tls_Hmac base struct + state => cLength
  ; algorithm id => algorithm_id
  ; pointer to algorithm update func => (*update)
  ; pointer to algorithm digest func => (*digest)
  ; length of hash digest => digest_len
  ; perform initial hmac state transform
  ; call algorithm init func
  ; return ptr tls_Hmac

tls_hmac_context_update:
  ; call (*update) for tls_Hmac->state

tls_hash_context_digest:
  ; call (*digest) for tls_Hmac->state
  ; perform final hmac state transform
  
  
