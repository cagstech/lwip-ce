
#include <stdint.h>
#include <string.h>
#include "../includes/rsa.h"
#include "../includes/random.h"

#define    ENCODE_START     0
#define ENCODE_SALT     (1 + ENCODE_START)
bool tls_rsa_encode_oaep(const uint8_t* in, size_t in_len, uint8_t* out,
                           size_t modulus_len, const uint8_t *auth, uint8_t alg){
    
    // initial sanity checks
    if((modulus_len > RSA_MODULUS_MAX_SUPPORTED) ||
       (modulus_len < RSA_MODULUS_MIN_SUPPORTED) ||
       (in == NULL) ||
       (out == NULL) ||
       (in_len == 0)
    ) return false;
    
    struct tls_context_sha256 ctx;
    if(!tls_sha256_init(&ctx)) return 0;
    size_t min_padding_len = (TLS_SHA256_DIGEST_LEN<<1) + 2;
    size_t ps_len = modulus_len - len - min_padding_len;
    size_t db_len = modulus_len - TLS_SHA256_DIGEST_LEN - 1;
    size_t encode_lhash = ENCODE_SALT + TLS_SHA256_DIGEST_LEN;
    size_t encode_ps = encode_lhash + TLS_SHA256_DIGEST_LEN;
    uint8_t mgf1_digest[RSA_MODULUS_MAX];
    
    if((in_len + min_padding_len) > modulus_len) return false;
    
    // set first byte to 00
    out[ENCODE_START] = 0x00;
    // seed next 32 bytes
    tls_random_bytes(&out[ENCODE_SALT], TLS_SHA256_DIGEST_LEN);
    
    // hash the authentication string
    if(auth != NULL) tls_sha256_update(&ctx, auth, strlen(auth));
    tls_sha256_digest(&ctx, &out[encode_lhash]);    // nothing to actually hash
    
    memset(&out[encode_ps], 0, ps_len);        // write padding zeros
    out[encode_ps + ps_len] = 0x01;            // write 0x01
    memcpy(&out[encode_ps + ps_len + 1], in, in_len);        // write plaintext to end of output
    
    // hash the salt with MGF1, return hash length of db
    tls_mgf1(&out[ENCODE_SALT], TLS_SHA256_DIGEST_LEN, mgf1_digest, db_len);
    
    // XOR hash with db
    for(size_t i=0; i < db_len; i++)
        out[encode_lhash + i] ^= mgf1_digest[i];
    
    // hash db with MGF1, return hash length of RSA_SALT_SIZE
    tls_mgf1(&out[encode_lhash], db_len, mgf1_digest, TLS_SHA256_DIGEST_LEN);
    
    // XOR hash with salt
    for(size_t i=0; i<hash_len; i++)
        out[ENCODE_SALT + i] ^= mgf1_digest[i];
    
    // Return the static size of 256
    return true;
}


size_t tls_rsa_decode_oaep(const uint8_t *in, size_t len, uint8_t* out, const uint8_t *auth){
    
    if((len > RSA_MODULUS_MAX_SUPPORTED) ||
       (len < RSA_MODULUS_MIN_SUPPORTED) ||
       (in == NULL) ||
       (out == NULL)
       ) return 0;
    
    struct tls_context_sha256 ctx;
    if(!tls_sha256_init(&ctx)) return 0;
    
    size_t db_len = len - TLS_SHA256_DIGEST_LEN - 1;
    uint8_t sha256_digest[TLS_SHA256_DIGEST_LEN];
    size_t encode_lhash = ENCODE_SALT + TLS_SHA256_DIGEST_LEN;
    size_t encode_ps = encode_lhash + TLS_SHA256_DIGEST_LEN;
    uint8_t mgf1_digest[RSA_MODULUS_MAX];
    size_t i;
    uint8_t tmp[RSA_MODULUS_MAX];
    
    memcpy(tmp, in, len);
    
    // Copy last 16 bytes of input buf to salt to get encoded salt
    // memcpy(salt, &in[len-RSA_SALT_SIZE-1], RSA_SALT_SIZE);
    
    // SHA-256 hash db
    tls_mgf1(&tmp[encode_lhash], db_len, mgf1_digest, TLS_SHA256_DIGEST_LEN);
    
    // XOR hash with encoded salt to return salt
    for(i = 0; i < hash_len; i++)
        tmp[ENCODE_SALT + i] ^= mgf1_digest[i];
    
    // MGF1 hash the salt
    tls_mgf1(&tmp[ENCODE_SALT], hash_len, mgf1_digest, db_len);
    
    // XOR MGF1 of salt with encoded message to get decoded message
    for(i = 0; i < db_len; i++)
        tmp[encode_lhash + i] ^= mgf1_digest[i];
    
    // verify authentication
    if(auth != NULL) tls_sha256_update(&ctx, auth, strlen(auth));
    tls_sha256_digest(&ctx, &out[encode_lhash]);
    
    if(!tls_bytes_compare(sha256_digest, out, TLS_SHA256_DIGEST_LEN)) return 0;
    
    for(i = encode_ps; i < len; i++)
        if(tmp[i] == 0x01) break;
    if(i==len) return false;
    i++;
    memcpy(out, &tmp[i], len-i);
    
    
    return len-i;
}

void powmod_exp_u24(uint8_t size, uint8_t *restrict base, uint24_t exp, const uint8_t *restrict mod);
#define RSA_PUBLIC_EXP  65537
bool tls_rsa_encrypt(const void* msg, size_t msglen, uint8_t *out,
                     const uint8_t* pubkey, size_t keylen){
    size_t spos = 0;
    if((msg==NULL) ||
       (pubkey==NULL) ||
       (out==NULL) ||
       (msglen==0) ||
       (keylen<RSA_MODULUS_MAX_SUPPORTED) ||
       (keylen>RSA_MODULUS_MIN_SUPPORTED) ||
       (!(pubkey[keylen-1]&1))) return false;
    
    while(pubkey[spos]==0) {out[spos++] = 0;}
    if(!tls_rsa_encode_oaep(msg, msglen, &out[spos], keylen-spos, NULL)) return false;
    powmod_exp_u24((uint8_t)keylen, ct, RSA_PUBLIC_EXP, pubkey);
    return true;
}
