//
//  hashes.h
//
//
//  Created by Anthony Cagliano on 7/11/24.
//

#ifndef tls_hash_h
#define tls_hash_h

#include <stdint.h>

/// @def Defines digest length of SHA-256
#define TLS_SHA256_DIGEST_LEN   32

/// @struct Defines context structure for SHA-256
struct tls_context_sha256 {
    uint8_t data[64];
    uint8_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
};

/*********************************************
 * @brief Initializes SHA-256 context.
 * @param context Pointer to a @b tls_context_sha256.
 * @returns Always @b true. This should never fail.
 */
bool tls_sha256_init(struct tls_context_sha256 *context);

/*********************************************
 * @brief Updates the SHA-256 context for given data.
 * @param context Pointer to a @b tls_context_sha256.
 * @param data    Pointer to data to hash.
 * @param len      Length of data to hash.
 * @returns \p context is updated.
 */
void tls_sha256_update(struct tls_context_sha256 *context, const void *data, size_t len);

/*********************************************
 * @brief Returns a digest from the current context state.
 * @param context       Pointer to a @b tls_context_sha256.
 * @param digest        Pointer to buffer to write digest to.
 */
void tls_sha256_digest(struct tls_context_sha256 *context, void *digest);





#endif
