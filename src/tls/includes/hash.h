//
//  hashes.h
//
//
//  Created by Anthony Cagliano on 7/11/24.
//

#ifndef tls_hash_h
#define tls_hash_h

#include <stdbool.h>
#include <stdint.h>

/// @def Defines digest length of SHA-256
#define TLS_SHA256_DIGEST_LEN   32

enum _hash_algorithms {
    TLS_SHA256
};

/// @struct Defines context structure for SHA-256
struct tls_sha256_context {
    uint8_t data[64];
    uint8_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
};
bool tls_sha256_init(struct tls_sha256_context *ctx);
void tls_sha256_update(struct tls_sha256_context *ctx, const uint8_t *data, size_t len);
void tls_sha256_digest(struct tls_sha256_context *ctx, uint8_t *digest);

struct tls_hash_context {
    uint8_t digestlen;
    bool (*init)(void *ctx);
    void (*update)(void *ctx, const uint8_t *data, size_t len);
    void (*digest)(void *ctx, uint8_t *digest);
    union {
        struct tls_sha256_context sha256;
    } _private;
};
bool tls_hash_context_init(struct tls_hash_context *ctx, uint8_t algorithm);
void tls_hash_update(struct tls_hash_context *ctx, const uint8_t *data, size_t len);
void tls_hash_digest(struct tls_hash_context *ctx, uint8_t *digest);





#endif
