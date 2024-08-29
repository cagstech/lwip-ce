
#ifndef tls_rsa_h
#define tls_rsa_h

#define RSA_MODULUS_MAX_SUPPORTED    4096>>3
#define RSA_MODULUS_MIN_SUPPORTED    1024>>3

bool tls_rsa_encode_oaep(const uint8_t *inbuf, size_t in_len, uint8_t *outbuf,
                         size_t modulus_len, const char *auth, uint8_t hash_alg);

size_t tls_rsa_decode_oaep(const uint8_t *inbuf, size_t in_len, uint8_t *outbuf, const char *auth, uint8_t hash_alg);

bool tls_rsa_encrypt(const uint8_t* inbuf, size_t in_len, uint8_t *outbuf,
                     const uint8_t* pubkey, size_t keylen, uint8_t hash_alg);

bool tls_mgf1(const uint8_t* data, size_t datalen, uint8_t* outbuf, size_t outlen, uint8_t hash_alg);

#endif
