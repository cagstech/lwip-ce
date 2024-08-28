#include <ti/screen.h>
#include <ti/getkey.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "tls/includes/aes.h"


#define KEYSIZE (128>>3)    // 256 bits converted to bytes

// input vectors
uint8_t key[KEYSIZE] = {0xEE,0x89,0x19,0xC3,0x8D,0x53,0x7A,0xD6,0x04,0x19,0x9E,0x77,0x0B,0xE0,0xE0,0x4C};
uint8_t iv[TLS_AES_BLOCK_SIZE] = {0x79,0xA6,0xDE,0xDF,0xF0,0xA2,0x7C,0x7F,0xEE,0x0B,0x8E,0xF5,0x12,0x63,0xA4,0x8A};
char *msg = "The lazy fox jumped over the dog!";
char *associated = "Some random header";

// test vectors
uint8_t tciphertext[] = {
    0x68,0x7d,0xb1,0x88,0xd1,0x37,0x84,0x42,0xf8,0x84,0x76,0x19,0x31,0x0d,0x7c,0xd1,
    0x9a,0xe4,0x3a,0x78,0x20,0xdb,0x7d,0x54,0x45,0x5a,0x35,0xba,0xe0,0x37,0x01,0x56,0x0d
};
uint8_t ttag[] = {0x23,0x62,0x9b,0x0d,0xfe,0xd6,0x01,0x8e,0x46,0x32,0x86,0x8c,0x07,0xc3,0xa8,0x3c};

/* Main function, called first */
int main(void)
{
    /* Clear the homescreen */
    struct tls_aes_context ctx;
    os_ClrHome();
    uint8_t tbuf[100] = {0};
    uint8_t tag[TLS_AES_AUTH_TAG_SIZE];
    bool status = true;
    
    // verify and decrypt
    status &= tls_aes_init(&ctx, key, KEYSIZE, iv, TLS_AES_IV_SIZE);
    status &= tls_aes_update_aad(&ctx, associated, strlen(associated));
    status &= tls_aes_encrypt(&ctx, msg, strlen(msg), tbuf);
    status &= tls_aes_digest(&ctx, tag);
    
    // await message match string
    if(status &&
       (memcmp(tbuf, tciphertext, strlen(msg))==0) &&
       (memcmp(tag, ttag, TLS_AES_AUTH_TAG_SIZE)==0))
        printf("success");
    else
        printf("failed");
    os_GetKey();
    os_ClrHome();
    return 0;
}
