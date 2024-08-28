#include <ti/screen.h>
#include <ti/getkey.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "tls/includes/hash.h"

// input vectors
const char *test1 = "The fox jumped over the dog!";
const char *test2 = "I need to go to sleep because its 11:30 and I have to be up at 7";
const char *test3 = "What is the answer to this question?";

// test vectors
const uint8_t expected1[] = {0xaf,0x66,0x35,0xa3,0x6f,0x9d,0xdb,0x72,0x28,0x39,0xed,0xde,0xb9,0x8e,0xf4,0xd9,
    0x6b,0xcf,0x3b,0xac,0xa5,0xfa,0xaf,0x46,0x0e,0x44,0x3a,0xa1,0x2b,0xd8,0x1c,0x8e};

const uint8_t expected2[] = {0xa7,0x0b,0xae,0x55,0x33,0x69,0xce,0x92,0x06,0xae,0x94,0xc0,0xe5,0xa2,0xe3,0xd8,
    0x1e,0x46,0xbf,0x69,0x38,0x3c,0xc2,0x25,0xde,0x4f,0xd9,0xfa,0xbe,0x71,0x60,0x7c};

const uint8_t expected3[] = {0xc0,0x49,0x46,0x7e,0xaa,0x7c,0xa7,0x58,0x70,0x40,0xad,0x05,0x75,0xac,0x62,0x12,
    0x9d,0xd9,0xc9,0xe7,0xf8,0x28,0xda,0x9b,0x16,0xe1,0x4c,0x0f,0xc8,0x5c,0xf1,0x6d};


/* Main function, called first */
int main(void)
{
    /* Clear the homescreen */
    os_ClrHome();
    uint8_t digest[TLS_SHA256_DIGEST_LEN];
    struct tls_context_sha256 ctx;
    
    // test 1
    tls_sha256_init(&ctx);
    tls_sha256_update(&ctx, test1, strlen(test1));
    tls_sha256_digest(&ctx, digest);
    if(memcmp(digest, expected1, TLS_SHA256_DIGEST_LEN)==0)
        printf("success");
    else printf("failed");
    os_GetKey();
    os_ClrHome();
    
    // test 2
    tls_sha256_init(&ctx);
    tls_sha256_update(&ctx, test2, strlen(test2));
    tls_sha256_digest(&ctx, digest);
    if(memcmp(digest, expected2, TLS_SHA256_DIGEST_LEN)==0)
        printf("success");
    else printf("failed");
    os_GetKey();
    os_ClrHome();
    
    // test 3
    tls_sha256_init(&ctx);
    tls_sha256_update(&ctx, test3, strlen(test3));
    tls_sha256_digest(&ctx, digest);
    if(memcmp(digest, expected3, TLS_SHA256_DIGEST_LEN)==0)
        printf("success");
    else printf("failed");
    os_GetKey();
    os_ClrHome();
    return 0;
}
