/// @file keyfiles.h
/// @author ACagliano
/// @brief Module providing import and export of keyfiles.

#ifndef tls_keyfiles_h
#define tls_keyfiles_h

#include "asn1.h"

enum _tls_key_types {
    TLS_KEY_PRIVATE,
    TLS_KEY_PUBLIC
};

enum tls_object_ids {
    // PKCS/SECG object ids
    TLS_OID_RSA_ENCRYPTION,             /**< RSAEncryption => 1.2.840.113549.1.1.1 */
    TLS_OID_EC_PUBLICKEY,               /**< id-ecPublicKey => 1.2.840.10045.2.1 */
    TLS_OID_EC_PRIVATEKEY,              /**< id-ecPrivateKey => 1.3.132.1.8 */
    TLS_OID_EC_SECP256R1,               /**< secp256r1 (prime256v1) => 1.2.840.10045.3.1.7 */
    
    // encryption algorithm identifiers (encrypted private key)
    TLS_OID_AES_128_GCM,                /**< AES-128-GCM => 2.16.840.1.101.3.4.1.2 */
    TLS_OID_AES_256_GCM,                /**< AES-256-GCM => 2.16.840.1.101.3.4.2.1 */
    TLS_OID_PBKDF2,                     /**< PBKDF2 => 1.2.840.113549.1.5.12 */
    TLS_OID_PBES2,                      /**< PBES2 => 1.2.840.113549.1.5.13 */
    TLS_OID_HMAC_SHA256,                /**< HMAC-SHA256 => 1.2.840.113549.2.9 */
    
    // X.509 identifiers
    TLS_OID_SHA256_RSA_ENCRYPTION,      /**< sha256WithRSAEncryption => 1.2.840.113549.1.1.11 */
    TLS_OID_SHA384_RSA_ENCRYPTION,      /**< sha384WithRSAEncryption => 1.2.840.113549.1.1.12 */
};

uint8_t tls_oid_bytes[][10] = {
    // PKCS and SECG object ids
    {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x01},     // TLS_OID_RSA_ENCRYPTION
    {0x2A,0x86,0x48,0xCE,0x3D,0x02,0x01},               // TLS_OID_EC_PUBLICKEY
    {0x2B,0x81,0x04,0x01,0x08},                         // TLS_OID_EC_PRIVATEKEY
    {0x2A,0x86,0x48,0xCE,0x3D,0x03,0x01,0x07},          // TLS_OID_EC_SECP256R1
    
    // encryption algorithm identifiers (encrypted private key)
    {0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x01,0x06},     // TLS_OID_AES_128_GCM
    {0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01},     // TLS_OID_AES_256_GCM
    {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x05,0x0C},     // TLS_OID_PBKDF2
    {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x05,0x0D},     // TLS_OID_PBES2
    {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x02,0x09},          // TLS_OID_HMAC_SHA256
    
    // X.509 identifiers
    {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0B},     // TLS_OID_SHA256_RSA_ENCRYPTION
    {0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0C}      // TLS_OID_SHA384_RSA_ENCRYPTION
    
};


/// @struct Output struct for private key files.
struct tls_privatekey_context {
    size_t length;
    size_t type;
    union {
        struct {
            struct tls_asn1_serialization version;
            struct tls_asn1_serialization modulus;
            struct tls_asn1_serialization public_exponent;
            struct tls_asn1_serialization exponent;
            struct tls_asn1_serialization p;
            struct tls_asn1_serialization q;
            struct tls_asn1_serialization exp1;
            struct tls_asn1_serialization exp2;
            struct tls_asn1_serialization coeff;
        } rsa;
        struct {
            struct tls_asn1_serialization version;
            struct tls_asn1_serialization privkey;
            struct tls_asn1_serialization curve_id;
            struct tls_asn1_serialization pubkey;
        } ec;
       
    } meta;
    uint8_t data[];
};

/// @struct Output struct for public key files.
struct tls_publickey_context {
    size_t length;
    size_t type;
    union {
        struct {
            struct tls_asn1_serialization modulus;
            struct tls_asn1_serialization exponent;
        } rsa;
        struct {
            struct tls_asn1_serialization pubkey;
        } ec;
    } meta;
    uint8_t data[];
};

struct tls_certificate_context {
  // no idea??
};

#endif
