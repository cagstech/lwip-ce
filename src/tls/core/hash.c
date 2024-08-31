#include <stdint.h>
#include "../includes/hash.h"

bool tls_hash_context_init(struct tls_hash_context *ctx, uint8_t algorithm){
    if(ctx==NULL) return false;
    switch(algorithm){
        case TLS_HASH_SHA256:
            ctx->digestlen = TLS_SHA256_DIGEST_LEN;
            ctx->init = tls_sha256_init;
            ctx->update = tls_sha256_update;
            ctx->digest = tls_sha256_digest;
            break; /*
        case TLS_SHA256_HW:
            ctx->digestlen = TLS_SHA256_DIGEST_LEN;
            ctx->init = tls_sha256hw_init;
            ctx->update = tls_sha256hw_update;
            ctx->digest = tls_sha256hw_digest;
            break;      */
        default:
            return false;
    }
    return ctx->init(&ctx->_private);
}

void tls_hash_update(struct tls_hash_context *ctx, const uint8_t *data, size_t len) {
    ctx->update(&ctx->_private, data, len);
}

void tls_hash_digest(struct tls_hash_context *ctx, uint8_t *digest) {
    ctx->digest(&ctx->_private, digest);
}
