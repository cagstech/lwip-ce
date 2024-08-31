#ifndef tls_hmac_h
#define tls_hmac_h

#include "hash.h"


struct tls_hmac_context {
    uint8_t digestlen;
    void (*update)(struct tls_hmac_context *ctx, const uint8_t *data, size_t len);
    void (*digest)(struct tls_hmac_context *ctx, uint8_t *digest);
    void (*_update)(void *ctx, const uint8_t *data, size_t len);
    bool (*_init)(void *ctx);
    void (*_final)(void *ctx, uint8_t *digest);
    uint8_t ipad[64];       /**< holds the key xored with a magic value to be hashed with the inner digest */
    uint8_t opad[64];       /**< holds the key xored with a magic value to be hashed with the outer digest */
    union {
        struct tls_sha256_context sha256;
    } _private;
};

bool tls_hmac_context_init(struct tls_hmac_context *ctx, uint8_t algorithm,
                           const uint8_t* key, size_t keylen);

void tls_hmac_update(struct tls_hmac_context *ctx, const uint8_t *data, size_t len);
void tls_hmac_digest(struct tls_hmac_context *ctx, uint8_t *digest);

#endif
