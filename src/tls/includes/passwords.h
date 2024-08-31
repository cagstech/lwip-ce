
#ifndef tls_passwords_h
#define tls_passwords_h

bool tls_pbkdf2(const char* password, size_t passlen,
                const uint8_t* salt, size_t saltlen,
                uint8_t *key, size_t keylen,
                size_t rounds, uint8_t algorithm);





#endif
