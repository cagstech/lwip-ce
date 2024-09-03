
#ifndef tls_aes_h
#define tls_aes_h

#include <stdbool.h>
#include <stdint.h>

/** @struct private internal structure for GCM. */
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
    uint8_t lock;   /**< sets allowed operations on the context. */
};

/** @struct AES state context. */
struct tls_aes_context {
    uint24_t keysize;
    uint32_t round_keys[60];
    uint8_t iv[16];
    uint8_t op_assoc;       /**< sets to either encrypt or decrypt based on first operation done on context. */
    struct _gcm_private _private;
};


#define TLS_AES_BLOCK_SIZE   16
#define TLS_AES_IV_SIZE     TLS_AES_BLOCK_SIZE
#define TLS_AES_AUTH_TAG_SIZE   TLS_AES_BLOCK_SIZE

/********************************************************************
 * @brief Initializes an AES context.
 * @param ctx       Pointer to an AES context.
 * @param key       Pointer to an AES key.
 * @param key_len        Length of AES key.
 * @param iv        Pointer to initialization vector.
 * @param iv_len     Length of initialization vector.
 * @returns @b true if success, @b false if error.
 */
bool tls_aes_init(struct tls_aes_context *ctx,
                  const uint8_t *key, size_t key_len,
                  const uint8_t *iv, size_t iv_len);

/********************************************************************
 * @brief Updates a context for associated data.
 * @param ctx       Pointer to an AES context.
 * @param aad       Pointer to associated data.
 * @param aad_len        Length of associated data.
 * @returns @b true if success, @b false if error.
 */
bool tls_aes_update_aad(struct tls_aes_context *ctx,
                        const uint8_t *aad, size_t aad_len);

/********************************************************************
 * @brief Encrypts a block of data.
 * @param ctx           Pointer to an AES context.
 * @param inbuf     Pointer to data to encrypt.
 * @param in_len        Length of data to encrypt.
 * @param outbuf    Pointer to buffer to write encrypted data.
 * @returns @b true if success, @b false if error.
 */
bool tls_aes_encrypt(struct tls_aes_context *ctx,
                     const uint8_t *inbuf, size_t in_len,
                     uint8_t *outbuf);

/********************************************************************
 * @brief Returns a digest for the current context state.
 * @param ctx       Pointer to an AES context.
 * @param digest        Pointer to a buffer to write the tag to.
 * @returns @b true if success, @b false if error.
 */
bool tls_aes_digest(struct tls_aes_context *ctx, uint8_t *digest);

/********************************************************************
 * @brief Decrypts a block of data.
 * @param ctx           Pointer to an AES context.
 * @param inbuf     Pointer to data to decrypt.
 * @param in_len        Length of data to decrypt.
 * @param outbuf    Pointer to buffer to write decrypted data.
 * @returns @b true if success, @b false if error.
 */
bool tls_aes_decrypt(struct tls_aes_context* ctx,
                     const uint8_t *inbuf, size_t in_length,
                     uint8_t *outbuf);

/********************************************************************
 * @brief Verifies the given AAD and ciphertext against the given tag.
 * @param ctx           Pointer to an AES context.
 * @param aad           Pointer to associated data to verifiy.
 * @param aad_len   Length of associated data.
 * @param ciphertext        Pointer to ciphertext data to verify.
 * @param ciphertext_len    Length of ciphertext data.
 * @param tag           Pointer to a tag to verify.
 * @returns @b true if tag valid, @b false otherwise.
 * @note For security, this function does not decrypt the ciphertext. Call \p tls_aes_decrypt
 * if this function returns @b true.
 */
bool tls_aes_verify(struct tls_aes_context *ctx,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *ciphertext, size_t ciphertext_len,
                    const uint8_t *tag);

#endif
