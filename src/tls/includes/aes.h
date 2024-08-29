
#ifndef tls_aes_h
#define tls_aes_h

#include <stdbool.h>
#include <stdint.h>

struct _gcm_private {
    uint8_t ghash_key[16];
    uint8_t auth_tag[16];
    uint8_t last_block[16];
    uint8_t last_block_len;
    uint8_t aad_cache[16];
    uint8_t aad_cache_len;
    uint8_t auth_j0[16];
    size_t aad_len;
    size_t ct_len;
    uint8_t lock;
};


struct tls_aes_context {
    uint24_t keysize;
    uint32_t round_keys[60];
    uint8_t iv[16];
    uint8_t op_assoc;
    struct _gcm_private _private;
};


#define TLS_AES_BLOCK_SIZE   16
#define TLS_AES_IV_SIZE     TLS_AES_BLOCK_SIZE
#define TLS_AES_AUTH_TAG_SIZE   TLS_AES_BLOCK_SIZE

bool tls_aes_init(struct tls_aes_context *ctx,
                  const uint8_t *key, size_t key_len,
                  const uint8_t *iv, size_t iv_len);

bool tls_aes_update_aad(struct tls_aes_context *ctx,
                        const uint8_t *aad, size_t aad_len);

bool tls_aes_encrypt(struct tls_aes_context *ctx,
                     const uint8_t *inbuf, size_t in_len,
                     uint8_t *outbuf);

bool tls_aes_digest(struct tls_aes_context *ctx, uint8_t *digest);

bool tls_aes_decrypt(struct tls_aes_context* ctx, 
                     const uint8_t *inbuf, size_t in_len,
                     uint8_t *outbuf);

bool tls_aes_verify(struct tls_aes_context *ctx,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *ciphertext, size_t ciphertext_len,
                    const uint8_t *tag);

#endif
