#include <stdint.h>
#include <string.h>
#include "lwip/mem.h"

#include "../includes/base64.h"
#include "../includes/asn1.h"
#include "../includes/keyfiles.h"

/* -------------------------------------------------------------------------
 RSAPrivateKey ::= SEQUENCE {
     version           Version,          -- Always set to 0 for PKCS#1 v1.5
     modulus           INTEGER,          -- n (modulus)
     publicExponent    INTEGER,          -- e (public exponent)
     privateExponent   INTEGER,          -- d (private exponent)
     prime1            INTEGER,          -- p (first prime factor)
     prime2            INTEGER,          -- q (second prime factor)
     exponent1         INTEGER,          -- d mod (p-1) (private exponent mod p-1)
     exponent2         INTEGER,          -- d mod (q-1) (private exponent mod q-1)
     coefficient       INTEGER,          -- (inverse of q) mod p
     otherPrimeInfos   OtherPrimeInfos OPTIONAL  -- Only for multi-prime RSA, usually omitted
 }
 */
struct tls_asn1_schema tls_pkcs1_privkey_schema[] = {
    {"RSAPrivateKey", ASN1_SEQUENCE, 0, false, false},
    {"version", ASN1_INTEGER, 1, false, false},
    {"modulus", ASN1_INTEGER, 1, false, false},
    {"publicExponent", ASN1_INTEGER, 1, false, false},
    {"privateExponent", ASN1_INTEGER, 1, false, false},
    {"prime1", ASN1_INTEGER, 1, false, false},
    {"prime2", ASN1_INTEGER, 1, false, false},
    {"exponent1", ASN1_INTEGER, 1, false, false},
    {"exponent2", ASN1_INTEGER, 1, false, false},
    {"version", ASN1_INTEGER, 1, false, false},
    {"coefficient", ASN1_INTEGER, 1, false, false},
    {NULL, 0, 0, false, false}
};


/* -------------------------------------------------------------------------
 RSAPublicKey ::= SEQUENCE {
     modulus           INTEGER,    -- n (the modulus)
     publicExponent    INTEGER     -- e (the public exponent)
 }
 */
struct tls_asn1_schema tls_pkcs1_pubkey_schema[] = {
    {"RSAPublicKey", ASN1_SEQUENCE, 0, false, false},
    {"modulus", ASN1_INTEGER, 1, false, false},
    {"publicExponent", ASN1_INTEGER, 1, false, false},
    {NULL, 0, 0, false, false}
};


/* -------------------------------------------------------------------------
 ECPrivateKey ::= SEQUENCE {
     version        1,
     privateKey     OCTET STRING,  -- Encoded private key value (d)
     parameters     [0] EXPLICIT ECParameters OPTIONAL,  -- Omitted if implied by curve
     publicKey      [0] EXPLICIT BIT STRING OPTIONAL     -- Optional encoded public key
 }
 */
struct tls_asn1_schema tls_sec1_privkey_schema[] = {
    {"ECPrivateKey", ASN1_SEQUENCE, 0, false, false},
    {"version", ASN1_INTEGER, 1, false, false},
    {"parameters [0]", ASN1_CONTEXTSPEC | ASN1_CONSTRUCTED | 0, 1, true, false},
    {"curve oid", ASN1_OBJECTID, 2, true, false},
    {"parameters [1]", ASN1_CONTEXTSPEC | ASN1_CONSTRUCTED | 1, 1, true, false},
    {"publicKey", ASN1_BITSTRING, 2, true, false},
    {NULL, 0, 0, false, false}
};


/* -------------------------------------------------------------------------
 ECPoint ::= OCTET STRING    -- Represents the public key (Q = d * G)
 */
struct tls_asn1_schema tls_sec1_pubkey_schema[] = {
    {"EC Point", ASN1_OCTETSTRING, 0, false, false},
    {NULL, 0, 0, false, false}
};


/* -------------------------------------------------------------------------
 PrivateKeyInfo ::= SEQUENCE {
     version                   Version,          -- Version is usually v1 (INTEGER 0)
     privateKeyAlgorithm        SEQUENCE {
         algorithm              OBJECT IDENTIFIER {id-ecPublicKey} (1.2.840.10045.2.1),
         parameters             OBJECT IDENTIFIER {secp256r1} (1.2.840.10045.3.1.7)
     },
     privateKey                 OCTET STRING,     -- Encoded private key
     attributes                 [0] IMPLICIT SET OF Attribute OPTIONAL   -- Optional attributes
 }
 */
struct tls_asn1_schema tls_pkcs8_privkey_schema[] = {
    {"PrivateKeyInfo", ASN1_SEQUENCE, 0, false, false},
    {"version", ASN1_INTEGER, 1, false, false},
    {"PrivateKeyAlgorithm", ASN1_SEQUENCE, 1, false, false},
    {"algorithm", ASN1_OBJECTID, 2, false, false},
    {"parameters", ASN1_OBJECTID, 2, false, true},
    {"privateKey", ASN1_OCTETSTRING, 1, false},
    {NULL, 0, 0, false, false}
};


/* -------------------------------------------------------------------------
 EncryptedPrivateKeyInfo ::= SEQUENCE {
     EncryptionAlgorithmIdentifier ::= SEQUENCE {
         algorithm            OBJECT IDENTIFIER,       -- The OID of the encryption algorithm (e.g., AES)
         parameters           ANY DEFINED BY algorithm OPTIONAL -- Algorithm-specific parameters (e.g., IV)
     }
     encryptedData        OCTET STRING                    -- The encrypted private key
 }
*/
struct tls_asn1_schema tls_pkcs8_encrypted_privkey_schema[] = {
    {"EncryptedPrivateKeyInfo", ASN1_SEQUENCE, 0, false, false},
    {"EncryptionAlgorithmIdentifier", ASN1_SEQUENCE, 1, false, false},
    {"algorithm", ASN1_OBJECTID, 2, false, false},
    {"parameters", ASN1_SEQUENCE, 2, false, false},
    {"keyDerivFunc", ASN1_SEQUENCE, 3, false, false},
    {"algorithm", ASN1_OBJECTID, 4, false, false},
    {"parameters", ASN1_SEQUENCE, 4, false, false},
    {"salt", ASN1_OCTETSTRING, 5, false, false},
    {"rounds", ASN1_INTEGER, 5, false, false},
    {"keylen", ASN1_INTEGER, 5, true, false},
    {"prf", ASN1_SEQUENCE, 5, false, false},
    {"algorithm", ASN1_OBJECTID, 6, false, false},
    {"parameters", ASN1_NULL, 6, false, false},
    {"encryptionScheme", ASN1_SEQUENCE, 3, false, false},
    {"algorithm", ASN1_OBJECTID, 4, false, false},
    {"iv", ASN1_OCTETSTRING, 4, false, false},
    {"encryptedData", ASN1_OCTETSTRING, 1, false, false},
    {NULL, 0, 0, false, false}
};


/* -------------------------------------------------------------------------
 SubjectPublicKeyInfo ::= SEQUENCE {
     algorithm            SEQUENCE {
         algorithm        OBJECT IDENTIFIER {id-ecPublicKey} (1.2.840.10045.2.1),
         parameters       OBJECT IDENTIFIER {secp256r1} (1.2.840.10045.3.1.7)
     },
     subjectPublicKey     BIT STRING  -- Encoded public key
 }
 */
struct tls_asn1_schema tls_pkcs8_pubkey_schema[] = {
    {"SubjectPublicKeyInfo", ASN1_SEQUENCE, 0, false, false},
    {"algorithm", ASN1_SEQUENCE, 1, false, false},
    {"algorithm", ASN1_OBJECTID, 2, false, false},
    {"parameters", ASN1_OBJECTID, 2, true, true},
    {"subjectPublicKey", ASN1_BITSTRING, 1, false, false},
    {NULL, 0, 0, false, false}
};


char *tls_pem_banner_pubkey_search_strings[] = {
    "-----BEGIN RSA PUBLIC KEY-----",
    "-----BEGIN EC PUBLIC KEY-----",
    "-----BEGIN PUBLIC KEY-----",
    NULL
};

char *tls_pem_banner_privkey_search_strings[] = {
    "-----BEGIN RSA PRIVATE KEY-----",
    "-----BEGIN EC PRIVATE KEY-----",
    "-----BEGIN PRIVATE KEY-----",
    "-----BEGIN ENCRYPTED PRIVATE KEY-----",
    NULL
};

char *tls_pem_banner_certificate_search_strings[] = {
    "-----BEGIN CERTIFICATE-----",
    NULL
};


struct tls_privatekey_context *tls_privatekey_import(const uint8_t *file_contents, size_t file_size){
    bool file_valid = false;
    for (int i = 0; tls_pem_banner_privkey_search_strings[i] != NULL; i++) {
        if(strcmp(file_contents, tls_pem_banner_privkey_search_strings[i]) == 0)
            file_valid = true;
    }
    if(!file_valid) return NULL;
    
    // compute start/end of base64 section
    uint8_t *b64_start = (uint8_t*)(strchr((char*)file_contents, '\n') + 1);
    uint8_t *b64_end = strchr(b64_start, '\n');
    size_t b64_size = b64_end - b64_start;
    if(b64_size > file_size) return NULL;
    uint8_t asn1_buf[b64_size / 3 * 4];
    // decode base64 into DER
    size_t asn1_size = tls_base64_decode(b64_start, b64_size, asn1_buf);
    size_t tot_len = sizeof(struct tls_privatekey_context) + asn1_size;
    // allocate context
    struct tls_privatekey_context *kf = mem_malloc(tot_len);
    if(kf==NULL) return NULL;
    kf->length = tot_len;
    
    // copy ASN.1 data to struct data field
    memcpy(kf->data, asn1_buf, asn1_size);
    struct tls_asn1_decoder_context asn1_ctx;
    if(!tls_asn1_decoder_init(&asn1_ctx, kf->data, asn1_size)) return NULL;
    
    if(strcmp(file_contents, tls_pem_banner_privkey_search_strings[0]) == 0)
        goto sec1_private;     // skip decrypt and pkcs8 decoding
    if(strcmp(file_contents, tls_pem_banner_privkey_search_strings[1]) == 0)
        goto pkcs8_private;     // skip decrypt
    
    if(strcmp(file_contents, tls_pem_banner_privkey_search_strings[3]) == 0){
  
    }
    
    
    
pkcs8_private:
    
pkcs1_private:
    
sec1_private:
    
    return kf;
}
