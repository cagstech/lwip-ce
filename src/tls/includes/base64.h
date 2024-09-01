#ifndef tls_base64_t
#define tls_base64_t

#include <stdint.h>

size_t tls_base64_encode(const uint8_t *in, size_t len, uint8_t *out);
size_t tls_base64_decode(const uint8_t *in, size_t len, uint8_t *out);

#endif
