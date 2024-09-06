/*
AlgorithmIdentifier ::= SEQUENCE {
    algorithm            OBJECT IDENTIFIER    -- OID for PBKDF2 (1.2.840.113549.1.5.12)
        parameters           SEQUENCE {           -- PBKDF2 Parameters
            salt             OCTET STRING         -- Salt (e.g., 1234567890ABCDEF)
            iterationCount   INTEGER              -- Iterations (e.g., 1000)
            keyLength        INTEGER OPTIONAL     -- Key length (e.g., 32 bytes for AES-256)
            prf              SEQUENCE {           -- Pseudo-random function (PRF)
                algorithm    OBJECT IDENTIFIER    -- OID for HMAC-SHA256 (1.2.840.113549.2.9)
                    parameters   NULL
                    }
        }
}
*/
#include <stdint.h>
#include <string.h>
#include "lwip/mem.h"

#include "..includes/asn1.h"


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
    {"RSAPrivateKey", ASN1_SEQUENCE, 0, false},
    {"version", ASN1_INTEGER, 1, false},
    {"modulus", ASN1_INTEGER, 1, false},
    {"publicExponent", ASN1_INTEGER, 1, false},
    {"privateExponent", ASN1_INTEGER, 1, false},
    {"prime1", ASN1_INTEGER, 1, false},
    {"prime2", ASN1_INTEGER, 1, false},
    {"exponent1", ASN1_INTEGER, 1, false},
    {"exponent2", ASN1_INTEGER, 1, false},
    {"version", ASN1_INTEGER, 1, false},
    {"coefficient", ASN1_INTEGER, 1, false},
    {NULL}
};
struct tls_asn1_serialization tls_pkcs1_privkey_[];


/* -------------------------------------------------------------------------
 RSAPublicKey ::= SEQUENCE {
     modulus           INTEGER,    -- n (the modulus)
     publicExponent    INTEGER     -- e (the public exponent)
 }
 */
struct tls_asn1_schema tls_pkcs1_pubkey_schema[] = {
    {"RSAPublicKey", ASN1_SEQUENCE, 0, 0},
    {"modulus", ASN1_INTEGER, 1, 0},
    {"publicExponent", ASN1_INTEGER, 1, 0}
};


/* -------------------------------------------------------------------------
 ECPrivateKey ::= SEQUENCE {
     version        1,
     privateKey     OCTET STRING,  -- Encoded private key value (d)
     parameters     EXPLICIT ECParameters OPTIONAL,  -- Omitted if implied by curve
     publicKey      EXPLICIT BIT STRING OPTIONAL     -- Optional encoded public key
 }
 */



bool tls_parse_sec1_private(struct tls_key_context *ks, struct tls_asn1_decoder_context *asn1_ctx){
    
    // just strip the outer sequence tag
    if(!tls_asn1_decode_next(struct tls_asn1_decoder_context *ctx, NULL, NULL, NULL, NULL))
        return false;
    
    if(!tls_asn1_decode_next(struct tls_asn1_decoder_context *ctx, NULL,
                             &ks->meta.rsa.privkey.version.data,
                             &ks->meta.rsa.privkey.version.len, NULL)) return false;
    if(!tls_asn1_decode_next(struct tls_asn1_decoder_context *ctx, NULL,
                             &ks->meta.rsa.privkey.privkey.data,
                             &ks->meta.rsa.privkey.privkey.len, NULL)) return false;
    if(!tls_asn1_decode_next(struct tls_asn1_decoder_context *ctx, NULL,
                             &ks->meta.rsa.privkey.curve_id.data,
                             &ks->meta.rsa.privkey.curve_id.len, NULL)) return false;
    // public key is optional
    tls_asn1_decode_next(struct tls_asn1_decoder_context *ctx, NULL,
                         &ks->meta.rsa.privkey.exponent.data,
                         &ks->meta.rsa.privkey.exponent.len, NULL);
    
    ks->bType = TLS_KEY_ECC_PRIVATE;
}


bool tls_parse_sec1_public(struct tls_key_context *ks, struct tls_asn1_decoder_context *asn1_ctx){
    /*
     ECPoint ::= OCTET STRING    -- Represents the public key (Q = d * G)
     */
    
    if(!tls_asn1_decode_next(struct tls_asn1_decoder_context *ctx, NULL,
                             &ks->meta.rsa.pubkey.pubkey.data,
                             &ks->meta.rsa.pubkey.pubkey.len, NULL)) return false;
    
    ks->bType = TLS_KEY_ECC_PUBLIC;
}


bool tls_parse_pkcs8_private(struct tls_key_context *ks, struct tls_asn1_decoder_context *asn1_ctx){
    /*
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
    
}


bool tls_parse_pkcs8_encrypted_private(struct tls_key_context *ks, struct tls_asn1_decoder_context *asn1_ctx){
    /*
     EncryptedPrivateKeyInfo ::= SEQUENCE {
         EncryptionAlgorithmIdentifier ::= SEQUENCE {
             algorithm            OBJECT IDENTIFIER,       -- The OID of the encryption algorithm (e.g., AES)
             parameters           ANY DEFINED BY algorithm OPTIONAL -- Algorithm-specific parameters (e.g., IV)
         }
         encryptedData        OCTET STRING                    -- The encrypted private key
     }
     */
    
}


bool tls_parse_pkcs8_public(struct tls_key_context *ks, struct tls_asn1_decoder_context *asn1_ctx){
    /*
     SubjectPublicKeyInfo ::= SEQUENCE {
         algorithm            SEQUENCE {
             algorithm        OBJECT IDENTIFIER {id-ecPublicKey} (1.2.840.10045.2.1),
             parameters       OBJECT IDENTIFIER {secp256r1} (1.2.840.10045.3.1.7)
         },
         subjectPublicKey     BIT STRING  -- Encoded public key
     }
     */
    
}


bool tls_parse_x509_certificate(struct tls_key_context *ks, struct tls_asn1_decoder_context *asn1_ctx){
    
}


struct tls_pem_banner_opts {
    const char *banner;
    bool (*handler)(struct tls_key_context *ks, struct tls_asn1_decoder_context *asn1_ctx);
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

struct _der_serialization {
    uint8_t tag;
    uint8_t *ptr;
    size_t len;
    uint8_t depth;
};

struct tls_privatekey_context *tls_privatekey_import(const uint8_t *file_contents, size_t file_size){
    bool file_valid = false;
    for (int i = 0; tls_pem_banner_privkey_search_strings[i] != NULL; i++) {
        if(strcmp(file_contents, tls_pem_banner_privkey_search_strings[i]) == 0)
            file_valid = true;
    }
    if(!file_valid) return NULL;
    
    // compute start/end of base64 section
    uint8_t *b64_start = (uint8_t*)(strchr((char*)file_content, '\n') + 1);
    uint8_t *b64_end = strchr(b64_start, '\n');
    size_t b64_size = b64_end - b64_start;
    uint8_t asn1_buf[b64_size / 3 * 4];
    // decode base64 into DER
    size_t asn1_size = tls_base64_decode(b64_start, b64_size, asn1_buf);
    size_t tot_len = sizeof(struct tls_privatekey_context) + asn1_size;
    // allocate context
    struct tls_privatekey_context *kf = mem_malloc(tot_len);
    if(kf==NULL) return NULL;
    kf->wLength = tot_len;
    
    memcpy(kf->data, asn1_buf, asn1_size);
    struct tls_asn1_decoder_context asn1_ctx;
    if(!tls_asn1_decoder_init(&asn1_ctx, kf->data, asn1_size)) return NULL;
    
    if(strcmp(file_contents, tls_pem_banner_privkey_search_strings[0]) == 0)
        goto sec1_ec_private;     // skip decrypt and pkcs8 decoding
    if(strcmp(file_contents, tls_pem_banner_privkey_search_strings[1]) == 0)
        goto pkcs8_private;     // skip decrypt
    
    if(strcmp(file_contents, tls_pem_banner_privkey_search_strings[3]) == 0){
        /*
         EncryptedPrivateKeyInfo ::= SEQUENCE {
         EncryptionAlgorithmIdentifier ::= SEQUENCE {
         algorithm            OBJECT IDENTIFIER,       -- The OID of the encryption algorithm (e.g., AES)
         parameters           ANY DEFINED BY algorithm OPTIONAL -- Algorithm-specific parameters (e.g., IV)
         }
         encryptedData        OCTET STRING                    -- The encrypted private key
         }
         */
        uint8_t tag, *oid;
        size_t oid_len;
        // unwrap sequence EncryptedPrivateKeyInfo
        if(!tls_asn1_decode_next(&asn1_ctx, NULL, NULL, NULL, NULL)) return NULL;
        // unwrap sequence EncryptionAlgorithmIdentifier
        if(!tls_asn1_decode_next(&asn1_ctx, NULL, NULL, NULL, NULL)) return NULL;
        // Get object id
        if(!tls_asn1_decode_next(&asn1_ctx, &tag, &oid, &oid_len, NULL)) return NULL;
        if(tag != ASN1_OBJECTID) return NULL;
        if(memcmp(tls_oid_bytes[TLS_OID_PBKDF2], oid, oid_len)==0) {
            /*
             PBKDF2-params ::= SEQUENCE {
             salt                 OCTET STRING,                    -- Random salt for PBKDF2
             iterationCount       INTEGER,                         -- Number of iterations for PBKDF2
             keyLength            INTEGER OPTIONAL,                -- Length of derived key (e.g., 16 for AES-128)
             prf                  AlgorithmIdentifier DEFAULT {hmacWithSHA256}  -- HMAC-SHA256 PRF
             encryptionScheme     SEQUENCE {
             algorithm        OBJECT IDENTIFIER {aes-128-gcm or aes-256-gcm}, -- OID for AES-GCM
             parameters       AES-GCM-params
             }
             }
             */
#define PKCS8_PBKDF2_SALT_IDX 0
#define PKCS8_PBKDF2_ROUNDS_IDX 1
#define PKCS8_PBKDF2_KEYLEN_IDX 2
#define PKCS8_PBKDF2_PRF_IDX 3
#define PKCS8_PBKDF2_ENCRYPTION_IDX 4
            struct _der_serialization pbkdf2_meta[PKCS8_PBKDF2_ENCRYPTION_IDX+1];
            // unwrap outer sequence
            if(!tls_asn1_decode_next(&asn1_ctx, NULL, NULL, NULL, NULL)) return NULL;
            // decode pbkdf2 salt
            for(int i=0; i < PKCS8_PBKDF2_ENCRYPTION_IDX+1; i++);
                tls_asn1_decode_next(&asn1_ctx, NULL, NULL, NULL, NULL))
            
            
            if((memcmp(tls_oid_bytes[TLS_OID_AES_128_GCM], oid, oid_len)==0) ||
               (memcmp(tls_oid_bytes[TLS_OID_AES_256_GCM], oid, oid_len)==0)){
                uint8_t
                
            }
        }
        else return NULL;
        
        
    }
    
    
    // copy ASN.1 data to struct data field
    
    
pkcs8_private:
    
pkcs1_private:
    
sec1_private:
    
    return kf;
}

struct tls_pem_banner_opts sec1_opts[] = {
    {"-----BEGIN EC PRIVATE KEY-----", tls_parse_sec1_private},
    {"-----BEGIN EC PUBLIC KEY-----", tls_parse_sec1_public}
};




{"-----BEGIN CERTIFICATE-----", tls_parse_x509_certificate},
