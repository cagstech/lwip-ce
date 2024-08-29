
#include <stdint.h>
#include <string.h>
#include "../includes/bytes.h"
#include "../includes/random.h"
#include "../includes/hash.h"
#include "../includes/rsa.h"

#define    ENCODE_START     0
#define ENCODE_SALT     (1 + ENCODE_START)
bool tls_rsa_encode_oaep(const uint8_t* inbuf, size_t in_len, uint8_t* outbuf,
                           size_t modulus_len, const char *auth, uint8_t hash_alg){
    
    // initial sanity checks
    if((modulus_len > RSA_MODULUS_MAX_SUPPORTED) ||
       (modulus_len < RSA_MODULUS_MIN_SUPPORTED) ||
       (inbuf == NULL) ||
       (outbuf == NULL) ||
       (in_len == 0)
    ) return false;
    
    struct tls_hash_context hash;
    if(!tls_hash_context_init(&hash, hash_alg)) return false;
    size_t min_padding_len = (hash.digestlen<<1) + 2;
    size_t ps_len = modulus_len - in_len - min_padding_len;
    size_t db_len = modulus_len - hash.digestlen - 1;
    size_t encode_lhash = ENCODE_SALT + hash.digestlen;
    size_t encode_ps = encode_lhash + hash.digestlen;
    uint8_t mgf1_digest[RSA_MODULUS_MAX_SUPPORTED];
    
    if((in_len + min_padding_len) > modulus_len) return false;
    
    // set first byte to 00
    outbuf[ENCODE_START] = 0x00;
    // seed next 32 bytes
    tls_random_bytes(&outbuf[ENCODE_SALT], hash.digestlen);
    
    // hash the authentication string
    if(auth != NULL) hash.update(&hash._private, auth, strlen(auth));
    hash.digest(&hash._private, &outbuf[encode_lhash]);    // nothing to actually hash
    
    memset(&outbuf[encode_ps], 0, ps_len);        // write padding zeros
    outbuf[encode_ps + ps_len] = 0x01;            // write 0x01
    memcpy(&outbuf[encode_ps + ps_len + 1], inbuf, in_len);        // write plaintext to end of output
    
    // hash the salt with MGF1, return hash length of db
    tls_mgf1(&outbuf[ENCODE_SALT], hash.digestlen, mgf1_digest, db_len, hash_alg);
    
    // XOR hash with db
    for(size_t i=0; i < db_len; i++)
        outbuf[encode_lhash + i] ^= mgf1_digest[i];
    
    // hash db with MGF1, return hash length of RSA_SALT_SIZE
    tls_mgf1(&outbuf[encode_lhash], db_len, mgf1_digest, hash.digestlen, hash_alg);
    
    // XOR hash with salt
    for(size_t i=0; i<hash.digestlen; i++)
        outbuf[ENCODE_SALT + i] ^= mgf1_digest[i];
    
    // Return the static size of 256
    return true;
}


size_t tls_rsa_decode_oaep(const uint8_t *inbuf, size_t in_len, uint8_t* outbuf, const char *auth, uint8_t hash_alg){
    
    if((in_len > RSA_MODULUS_MAX_SUPPORTED) ||
       (in_len < RSA_MODULUS_MIN_SUPPORTED) ||
       (inbuf == NULL) ||
       (outbuf == NULL)) return 0;
    
    struct tls_hash_context hash;
    if(!tls_hash_context_init(&hash, hash_alg)) return false;
    
    size_t db_len = in_len - hash.digestlen - 1;
    uint8_t sha256_digest[TLS_SHA256_DIGEST_LEN];
    size_t encode_lhash = ENCODE_SALT + hash.digestlen;
    size_t encode_ps = encode_lhash + hash.digestlen;
    uint8_t mgf1_digest[RSA_MODULUS_MAX_SUPPORTED];
    size_t i;
    uint8_t tmp[RSA_MODULUS_MAX_SUPPORTED];
    
    memcpy(tmp, inbuf, in_len);
    
    // Copy last 16 bytes of input buf to salt to get encoded salt
    // memcpy(salt, &in[len-RSA_SALT_SIZE-1], RSA_SALT_SIZE);
    
    // SHA-256 hash db
    tls_mgf1(&tmp[encode_lhash], db_len, mgf1_digest, hash.digestlen, hash_alg);
    
    // XOR hash with encoded salt to return salt
    for(i = 0; i < TLS_SHA256_DIGEST_LEN; i++)
        tmp[ENCODE_SALT + i] ^= mgf1_digest[i];
    
    // MGF1 hash the salt
    tls_mgf1(&tmp[ENCODE_SALT], hash.digestlen, mgf1_digest, db_len, hash_alg);
    
    // XOR MGF1 of salt with encoded message to get decoded message
    for(i = 0; i < db_len; i++)
        tmp[encode_lhash + i] ^= mgf1_digest[i];
    
    // verify authentication
    if(auth != NULL) hash.update(&hash._private, auth, strlen(auth));
    hash.digest(&hash._private, &outbuf[encode_lhash]);
    
    if(!tls_bytes_compare(sha256_digest, outbuf, TLS_SHA256_DIGEST_LEN)) return 0;
    
    for(i = encode_ps; i < in_len; i++)
        if(tmp[i] == 0x01) break;
    if(i==in_len) return false;
    i++;
    memcpy(outbuf, &tmp[i], in_len-i);
    
    
    return in_len-i;
}

void powmod_exp_u24(uint8_t size, uint8_t *restrict base, uint24_t exp, const uint8_t *restrict mod);
#define RSA_PUBLIC_EXP  65537
bool tls_rsa_encrypt(const uint8_t* inbuf, size_t in_len, uint8_t *outbuf,
                     const uint8_t* pubkey, size_t keylen, uint8_t hash_alg){
    size_t spos = 0;
    if((inbuf==NULL) ||
       (pubkey==NULL) ||
       (outbuf==NULL) ||
       (in_len==0) ||
       (keylen<RSA_MODULUS_MAX_SUPPORTED) ||
       (keylen>RSA_MODULUS_MIN_SUPPORTED) ||
       (!(pubkey[keylen-1]&1))) return false;
    
    while(pubkey[spos]==0) {outbuf[spos++] = 0;}
    if(!tls_rsa_encode_oaep(inbuf, in_len, &outbuf[spos], keylen-spos, NULL, hash_alg)) return false;
    powmod_exp_u24((uint8_t)keylen, outbuf, RSA_PUBLIC_EXP, pubkey);
    return true;
}


bool tls_mgf1(const uint8_t* data, size_t datalen, uint8_t* outbuf, size_t outlen, uint8_t hash_alg){
    uint32_t ctr = 0;
    uint8_t hash_digest[TLS_SHA256_DIGEST_LEN], ctr_data[4];
    struct tls_hash_context hash_data, hash_ctr;
    if(!tls_hash_context_init(&hash_data, hash_alg)) return false;
    size_t hashlen = hash_data.digestlen;
    hash_data.update(&hash_data._private, data, datalen);
    for(size_t printlen=0; printlen<outlen; printlen+=hashlen, ctr++){
        size_t copylen = (outlen-printlen > hashlen) ? hashlen : outlen-printlen;
        //memcpy(ctr_data, &ctr, 4);
        ctr_data[0] = (uint8_t) ((ctr >> 24) & 0xff);
        ctr_data[1] = (uint8_t) ((ctr >> 16) & 0xff);
        ctr_data[2] = (uint8_t) ((ctr >> 8) & 0xff);
        ctr_data[3] = (uint8_t) ((ctr >> 0) & 0xff);
        memcpy(&hash_ctr, &hash_data, sizeof hash_ctr);
        hash_ctr.update(&hash_ctr._private, ctr_data, 4);
        hash_ctr.digest(&hash_ctr._private, hash_digest);
        memcpy(&outbuf[printlen], hash_digest, copylen);
    }
    return true;
}
