#ifndef tls_bytes_h
#define tls_bytes_h

#include <stdbool.h>
#include <stdint.h>

/***********************************************************************
 * @brief Secure comparison of two buffers.
 * @param buf1      Pointer to first buffer to compare.
 * @param buf2      Pointer to second buffer to compare.
 * @param len       Number of bytes to compare.
 */
bool tls_bytes_compare(const void *buf1, const void *buf2, size_t len);


#endif

