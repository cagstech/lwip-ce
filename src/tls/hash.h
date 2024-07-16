//
//  hashes.h
//
//
//  Created by Anthony Cagliano on 7/11/24.
//

#ifndef tls_hash_h
#define tls_hash_h

#include <stdint.h>

enum {
	TLS_HASH_SHA1,
	TLS_HASH_SHA256
	// TLS_HASH_SHA512 ??
};

struct tls_hash {
	uint8_t algorithm_id;
	uint8_t digest_len;
	uint8_t state[];
};

// avoiding static allocation for contexts in lwip where possible
struct tls_hash *tls_hash_context_create(uint8_t algorithm_id);
bool tls_hash_update(struct tls_hash *context, const void *data, size_t len);
bool tls_hash_digest(struct tls_hash *context, void *digest);


// moving this to an hmac module




#endif
