//
//  hashes.h
//
//
//  Created by Anthony Cagliano on 7/11/24.
//

#ifndef hashes_h
#define hashes_h

#include <stdio.h>
#include "../helpers/tls_err.h"

struct tls_Hash {
  size_t cLength;
  uint8_t algorithm_id;
  void (*update)(void *context, const void *data, size_t len);
  void (*digest)(void *context, void *digest);
  size_t digest_len;
  uint8_t state[];
};

// avoiding static allocation for contexts in lwip where possible
struct tls_Hash *tls_hash_context_create(uint8_t hash_id);
tls_err_t tls_hash_context_update(struct tls_Hash *context, const void *data, size_t len);
tls_err_t tls_hash_context_digest(struct tls_Hash *context, void *digest);


struct tls_Hmac {
  size_t cLength;
  uint8_t algorithm_id;
  void (*update)(void *context, const void *data, size_t len);
  void (*digest)(void *context, void *digest);
  size_t digest_len;
  uint8_t ipad[64];       /**< holds the key xored with a magic value to be hashed with the inner digest */
  uint8_t opad[64];       /**< holds the key xored with a magic value to be hashed with the outer digest */
  uint8_t state[];
};

struct tls_Hmac *tls_hmac_context_create(uint8_t hash_id, const void* key, size_t keylen);
tls_err_t tls_hmac_context_update(struct tls_Hmac *context, const void *data, size_t len);
tls_err_t tls_hmac_context_digest(struct tls_Hmac *context, void *digest);


#endif
