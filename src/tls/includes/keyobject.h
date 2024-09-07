/// @file keyobject.h
/// @author ACagliano
/// @brief Module providing import and export of keyfiles.

#ifndef tls_keyobject_h
#define tls_keyobject_h

#include "asn1.h"

enum tls_key_type_flags {
    TLS_KEY_PRIVATE = 0,
    TLS_KEY_PUBLIC = 1,
    TLS_KEY_RSA = (0<<1),
    TLS_KEY_ECC = (1<<1),
};

enum tls_object_ids {
    // PKCS/SECG object ids
    TLS_OID_RSA_ENCRYPTION,             /**< RSAEncryption => 1.2.840.113549.1.1.1 */
    TLS_OID_EC_PUBLICKEY,               /**< id-ecPublicKey => 1.2.840.10045.2.1 */
    TLS_OID_EC_PRIVATEKEY,              /**< id-ecPrivateKey => 1.3.132.1.8 */
    TLS_OID_EC_SECP256R1,               /**< secp256r1 (prime256v1) => 1.2.840.10045.3.1.7 */
    
    // encryption algorithm identifiers (encrypted private key)
    TLS_OID_AES_128_GCM,                /**< AES-128-GCM => 2.16.840.1.101.3.4.1.2 */
    TLS_OID_AES_128_CBC,                /**< AES-128-CBC  => 2.16.840.1.101.3.4.1.2 */
    TLS_OID_AES_256_GCM,                /**< AES-256-GCM => 2.16.840.1.101.3.4.2.1 */
    TLS_OID_AES_256_CBC,                /**< AES-256-CBC => 2.16.840.1.101.3.4.1.42 */
    TLS_OID_PBKDF2,                     /**< PBKDF2 => 1.2.840.113549.1.5.12 */
    TLS_OID_PBES2,                      /**< PBES2 => 1.2.840.113549.1.5.13 */
    TLS_OID_HMAC_SHA256,                /**< HMAC-SHA256 => 1.2.840.113549.2.9 */
    
    // X.509 identifiers
    TLS_OID_SHA256_RSA_ENCRYPTION,      /**< sha256WithRSAEncryption => 1.2.840.113549.1.1.11 */
    TLS_OID_SHA384_RSA_ENCRYPTION,      /**< sha384WithRSAEncryption => 1.2.840.113549.1.1.12 */
};

extern uint8_t tls_objectids[][10];


/// @struct Output struct for private key files.
struct tls_private_key_context {
    size_t length;
    size_t type;
    union {
        union {
            struct tls_asn1_serialization fields[8];
            struct {
                struct tls_asn1_serialization modulus;
                struct tls_asn1_serialization public_exponent;
                struct tls_asn1_serialization exponent;
                struct tls_asn1_serialization p;
                struct tls_asn1_serialization q;
                struct tls_asn1_serialization exp1;
                struct tls_asn1_serialization exp2;
                struct tls_asn1_serialization coeff;
            } field;
        } rsa;
        union {
            struct tls_asn1_serialization fields[4];
            struct {
                struct tls_asn1_serialization privkey;
                struct tls_asn1_serialization curve_id;
                struct tls_asn1_serialization pubkey;
            } field;
        } ec;
    } meta;
    uint8_t data[];
};

/// @struct Output struct for public key files.
struct tls_public_key_context {
    size_t length;
    size_t type;
    union {
        union {
            struct tls_asn1_serialization fields[8];
            struct {
                struct tls_asn1_serialization modulus;
                struct tls_asn1_serialization exponent;
            } field;
        } rsa;
        union {
            struct tls_asn1_serialization fields[1];
            struct {
                struct tls_asn1_serialization pubkey;
            } field;
        } ec;
    } meta;
    uint8_t data[];
};

struct tls_certificate_context {
  // no idea??
};


struct tls_private_key_context *tls_private_key_import(const char *pem_data, size_t size, const char *password);
struct tls_public_key_context *tls_public_key_import(const char *pem_data, size_t size);

#endif
