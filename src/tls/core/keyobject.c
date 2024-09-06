#include <stdint.h>
#include <string.h>
#include "lwip/mem.h"

#include "../includes/base64.h"
#include "../includes/asn1.h"
#include "../includes/keyobject.h"

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
    {"RSAPrivateKey", ASN1_SEQUENCE, 0, false, false, false},
    {"version", ASN1_INTEGER, 1, false, false, false},
    {"modulus", ASN1_INTEGER, 1, false, false, true},
    {"publicExponent", ASN1_INTEGER, 1, false, false, true},
    {"privateExponent", ASN1_INTEGER, 1, false, false, true},
    {"prime1", ASN1_INTEGER, 1, false, false, true},
    {"prime2", ASN1_INTEGER, 1, false, false, true},
    {"exponent1", ASN1_INTEGER, 1, false, false, true},
    {"exponent2", ASN1_INTEGER, 1, false, false, true},
    {"coefficient", ASN1_INTEGER, 1, false, false, true},
    {NULL, 0, 0, false, false, false}
};


/* -------------------------------------------------------------------------
 RSAPublicKey ::= SEQUENCE {
     modulus           INTEGER,    -- n (the modulus)
     publicExponent    INTEGER     -- e (the public exponent)
 }
 */
struct tls_asn1_schema tls_pkcs1_pubkey_schema[] = {
    {"RSAPublicKey", ASN1_SEQUENCE, 0, false, false, false},
    {"modulus", ASN1_INTEGER, 1, false, false, true},
    {"publicExponent", ASN1_INTEGER, 1, false, false, true},
    {NULL, 0, 0, false, false, false}
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
    {"ECPrivateKey", ASN1_SEQUENCE, 0, false, false, false},
    {"version", ASN1_INTEGER, 1, false, false, false},
    {"privateKey", ASN1_OCTETSTRING, 1, false, false, true},
    {"parameters [0]", ASN1_CONTEXTSPEC | ASN1_CONSTRUCTED | 0, 1, true, false, false},
    {"curve oid", ASN1_OBJECTID, 2, true, false, true},
    {"parameters [1]", ASN1_CONTEXTSPEC | ASN1_CONSTRUCTED | 1, 1, true, false, false},
    {"publicKey", ASN1_BITSTRING, 2, true, false, true},
    {NULL, 0, 0, false, false, false}
};


/* -------------------------------------------------------------------------
 ECPoint ::= OCTET STRING    -- Represents the public key (Q = d * G)
 */
struct tls_asn1_schema tls_sec1_pubkey_schema[] = {
    {"EC Point", ASN1_OCTETSTRING, 0, false, false, true},
    {NULL, 0, 0, false, false, false}
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
    {"PrivateKeyInfo", ASN1_SEQUENCE, 0, false, false, false},
    {"version", ASN1_INTEGER, 1, false, false, false},
    {"PrivateKeyAlgorithm", ASN1_SEQUENCE, 1, false, false, false},
    {"algorithm", ASN1_OBJECTID, 2, false, false, true},
    {"parameters", ASN1_OBJECTID, 2, false, true, true},
    {"privateKey", ASN1_OCTETSTRING, 1, false, false, true},
    {NULL, 0, 0, false, false, false}
};
struct tls_asn1_serialization tls_pkcs8_private_serialize[3];


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
    {"EncryptedPrivateKeyInfo", ASN1_SEQUENCE, 0, false, false, false},
    {"EncryptionAlgorithmIdentifier", ASN1_SEQUENCE, 1, false, false, false},
    {"algorithm", ASN1_OBJECTID, 2, false, false, true},
    {"parameters", ASN1_SEQUENCE, 2, false, false, false},
    {"keyDerivFunc", ASN1_SEQUENCE, 3, false, false, false},
    {"algorithm", ASN1_OBJECTID, 4, false, false, true},
    {"parameters", ASN1_SEQUENCE, 4, false, false, false},
    {"salt", ASN1_OCTETSTRING, 5, false, false, true},
    {"rounds", ASN1_INTEGER, 5, false, false, true},
    {"keylen", ASN1_INTEGER, 5, true, false, true},
    {"prf", ASN1_SEQUENCE, 5, false, false, false},
    {"algorithm", ASN1_OBJECTID, 6, false, false, true},
    {"parameters", ASN1_NULL, 6, false, false, false},
    {"encryptionScheme", ASN1_SEQUENCE, 3, false, false, false},
    {"algorithm", ASN1_OBJECTID, 4, false, false, true},
    {"iv", ASN1_OCTETSTRING, 4, false, false, true},
    {"encryptedData", ASN1_OCTETSTRING, 1, false, false, true},
    {NULL, 0, 0, false, false, false}
};
struct tls_asn1_serialization tls_pkcs8_encrypted_private_serialize[9];

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
    {"SubjectPublicKeyInfo", ASN1_SEQUENCE, 0, false, false, false},
    {"algorithm", ASN1_SEQUENCE, 1, false, false, false},
    {"algorithm", ASN1_OBJECTID, 2, false, false, true},
    {"parameters", ASN1_OBJECTID, 2, true, true, true},
    {"subjectPublicKey", ASN1_BITSTRING, 1, false, false, true},
    {NULL, 0, 0, false, false, false}
};
struct tls_asn1_serialization tls_pkcs_public_serialize[3];


struct tls_lookup_data {
    char *banner;
    struct tls_asn1_schema *decoder_schema;
};

struct tls_lookup_data pkcs_lookups[] = {
    {"-----BEGIN RSA PRIVATE KEY-----", tls_pkcs1_privkey_schema},
    {"-----BEGIN EC PRIVATE KEY-----", tls_sec1_privkey_schema},
    {"-----BEGIN PRIVATE KEY-----", tls_pkcs8_privkey_schema},
    {"-----BEGIN ENCRYPTED PRIVATE KEY-----", tls_pkcs8_encrypted_privkey_schema},
    {"-----BEGIN RSA PUBLIC KEY-----", tls_pkcs1_pubkey_schema},
    {"-----BEGIN EC PUBLIC KEY-----", tls_sec1_pubkey_schema},
    {"-----BEGIN PUBLIC KEY-----", tls_pkcs8_pubkey_schema},
    //{ "-----BEGIN CERTIFICATE-----", NULL},
    {NULL, NULL}
};


struct tls_private_key_context *tls_private_key_import(const uint8_t *pem_data, size_t size){
    struct tls_asn1_schema *decoder_schema;
    struct tls_asn1_serialization tmp_serialize[9] = {0};
    uint8_t serialized_count = 0;
    struct tls_asn1_decoder_context asn1_ctx;
    uint8_t *b64_start = (uint8_t*)(strchr((char*)pem_data, '\n') + 1);
    uint8_t *b64_end = (uint8_t*)strchr((char*)b64_start, '\n');
    size_t b64_size = b64_end - b64_start;
    
    // locate banner, load schema for format
    for(int i=0; pkcs_lookups[i].banner != NULL; i++)
        if(strcmp((char*)pem_data, pkcs_lookups[i].banner) == 0){
            decoder_schema = pkcs_lookups[i].decoder_schema;
            goto file_type_ok;
        }
    return NULL;        // if no matching banner found, fail
    
file_type_ok:
    // compute start/end of base64 section, sanity checks on size
    if(b64_size > size) return NULL;
    // decode base64 into DER
    uint8_t asn1_buf[b64_size / 3 * 4];
    size_t asn1_size = tls_base64_decode(b64_start, b64_size, asn1_buf);
    size_t tot_len = sizeof(struct tls_private_key_context) + asn1_size;
    // allocate context
    struct tls_private_key_context *kf = mem_malloc(tot_len);
    if(kf==NULL) return NULL;
    kf->length = tot_len;
    // copy ASN.1 data to struct data field
    memcpy(kf->data, asn1_buf, asn1_size);
    if(!tls_asn1_decoder_init(&asn1_ctx, kf->data, asn1_size)) goto error;
    
    kf->type = 0 | TLS_KEY_PRIVATE;
    
    if(decoder_schema == tls_pkcs8_encrypted_privkey_schema){
        goto error;
    }
    if(decoder_schema ==  tls_pkcs8_privkey_schema){
        for(uint24_t i=0; decoder_schema[i].name != NULL; i++){
            if(!tls_asn1_decode_next(&asn1_ctx,
                                     &decoder_schema[i],
                                     &tmp_serialize[serialized_count].tag,
                                     &tmp_serialize[serialized_count].data,
                                     &tmp_serialize[serialized_count].len,
                                     NULL)) goto error;
            if(decoder_schema[i].output) serialized_count++;
        }
        // set decoder to start at private key bit/octet string
        if(!tls_asn1_decoder_init(&asn1_ctx, tmp_serialize[2].data, tmp_serialize[2].len)) goto error;
    }
    
    if(memcmp(tmp_serialize[0].data, tls_oid_bytes[TLS_OID_RSA_ENCRYPTION], tmp_serialize[0].len)==0)
        goto is_rsa;
    else if(
            (memcmp(tmp_serialize[0].data, tls_oid_bytes[TLS_OID_EC_PRIVATEKEY], tmp_serialize[0].len)==0) &&
            (memcmp(tmp_serialize[1].data, tls_oid_bytes[TLS_OID_EC_SECP256R1], tmp_serialize[1].len)==0))
        goto is_ecc;
    else goto error;
    
is_rsa:
    kf->type |= TLS_KEY_RSA;
    decoder_schema = tls_pkcs1_privkey_schema;
    serialized_count = 0;
    for(uint24_t i=0; decoder_schema[i].name != NULL; i++){
        if(!tls_asn1_decode_next(&asn1_ctx,
                                 &decoder_schema[i],
                                 &kf->meta.rsa.fields[serialized_count].tag,
                                 &kf->meta.rsa.fields[serialized_count].data,
                                 &kf->meta.rsa.fields[serialized_count].len,
                                 NULL)) goto error;
        if(decoder_schema[i].output) serialized_count++;
    }
    return kf;
    
is_ecc:
    kf->type |= TLS_KEY_ECC;
    decoder_schema = tls_sec1_privkey_schema;
    serialized_count = 0;
    for(uint24_t i=0; decoder_schema[i].name != NULL; i++){
        if(!tls_asn1_decode_next(&asn1_ctx,
                                 &decoder_schema[i],
                                 &kf->meta.ec.fields[serialized_count].tag,
                                 &kf->meta.ec.fields[serialized_count].data,
                                 &kf->meta.ec.fields[serialized_count].len,
                                 NULL)) goto error;
        if(decoder_schema[i].output) serialized_count++;
    }
    return kf;
    
error:
    memset(kf, 0, kf->length);
    mem_free(kf);
    return NULL;
}
