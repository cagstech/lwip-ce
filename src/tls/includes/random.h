/// @file random.h
/// @author ACagliano & Adam Beckingham
/// @brief Module providing a TRNG for use with TLS and other secure applications
/// @warning Use this module for ALL applications requiring security. The toolchain @b rand functions are
///     suitable for generic use but are **not cryptographically secure**.

#ifndef tls_random_h
#define tls_random_h

#include <stdbool.h>
#include <stdint.h>

/*************************************************************************************
 * @brief Initializes the cryptographic TRNG within this module.
 * @returns [bool] True if generator initialization succeeded or False if failed.
 * @warning DO NOT USE the toolchain @b rand functions with the TLS functions of lwIP-CE. They are not secure
 * for cryptography. Use this module instead.
 */
bool tls_random_init_entropy(void);

/*************************************************************************************
 * @brief Returns a secure 64-bit random integer.
 * @returns [uint64_t]    A secure 64-bit random integer.
 */
uint64_t tls_random(void);

/*************************************************************************************
 * @brief Fills a buffer with securely random bytes.
 * @param[in,out] buffer    Pointer to a buffer to fill with random bytes.
 * @param[in] len    Length of buffer to fill.
 * @returns [void*]    Pointer to random buffer. Should be equal to @p buffer .
 */
void* tls_random_bytes(void* buffer, size_t len);


#endif
