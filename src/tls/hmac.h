//
//  hmac.h
//  
//
//  Created by Anthony Cagliano on 7/13/24.
//

#ifndef tls_hmac_h
#define tls_hmac_h

enum {
	TLS_HMAC_SHA1,
	TLS_HMAC_SHA256
	// TLS_HASH_SHA512 ??
};

struct tls_Hmac {
	uint8_t algorithm_id;
	uint8_t digest_len;
	uint8_t ipad[64];       /**< holds the key xored with a magic value to be hashed with the inner digest */
	uint8_t opad[64];       /**< holds the key xored with a magic value to be hashed with the outer digest */
	uint8_t state[];
};

// avoiding static allocation for contexts in lwip where possible
struct tls_Hash *tls_hmac_context_create(uint8_t hash_id);
bool tls_hmac_update(struct tls_Hash *context, const void *data, size_t len);
bool tls_hmac_digest(struct tls_Hash *context, void *digest);

#endif /* hmac_h */
