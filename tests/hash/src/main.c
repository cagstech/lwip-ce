#include <ti/screen.h>
#include <ti/getkey.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../../../src/tls/includes/hash.h"
const char *teststr = "The fox jumped over the dog!";
const uint8_t expected[TLS_SHA256_DIGEST_LEN] = {0xaf,0x66,0x35,0xa3,0x6f,0x9d,0xdb,0x72,0x28,0x39,0xed,0xde,0xb9,0x8e,0xf4,0xd9,
    0x6b,0xcf,0x3b,0xac,0xa5,0xfa,0xaf,0x46,0x0e,0x44,0x3a,0xa1,0x2b,0xd8,0x1c,0x8e};

/* Main function, called first */
int main(void)
{
    /* Clear the homescreen */
    os_ClrHome();
    uint8_t digest[TLS_SHA256_DIGEST_LEN];
    struct tls_context_sha256 ctx;
    tls_sha256_init(&ctx);
    tls_sha256_update(&ctx, teststr, strlen(teststr));
    tls_sha256_digest(&ctx, digest);
    if(memcmp(digest, expected, TLS_SHA256_DIGEST_LEN)==0)
        printf("success");
    else printf("failed");
    os_GetKey();
    return 0;
}
