#include <ti/screen.h>
#include <ti/getkey.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lwip/mem.h"

#include "tls/includes/keyobject.h"

// test vectors
const char *test1 = "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDGyysU2q4d1q5X2gC4cSZIgRScpt6W0w3ypyYWrM+85s4YeIniKhjuA7EUSWlAG3BJuElaEJNsWFtDFItptYzIkLLvPzz4ecJfvfFu5o4r3H//a7DpyiXwe2e4GEwwCV8FtHlrZaUcqb/mjRiziEwvmKPTCO/GkQyXI0wHQCOijQIDAQAB\n-----END PUBLIC KEY-----";

const char *test2 = "-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEhhE3TnZBQDSZiLQzxYhdrMSD7Y1fJVijgONxCi4f7nbggjZous05eRjeDoVjw1960Vci2ueuUFNsG7A1mjkPow==\n-----END PUBLIC KEY-----";

/* Main function, called first */
int main(void)
{
    /* Clear the homescreen */
    os_ClrHome();
    os_FontSelect(os_SmallFont);
    
    struct mem_configurator conf = {
        MEM_CONFIGURATOR_V1,
        malloc,
        free,
        1024*12
    };
    
    char buf[128];
    mem_configure(&conf);
    
    struct tls_public_key_context *pk = tls_public_key_import(test1, strlen(test1));
    if(pk==NULL){
        printf("error");
        os_GetKey();
        os_ClrHome();
        return 1;
    }
    for(int i=0; i < 2; i++){
        sprintf(buf, "%s:tag=%u, size=%u: %02x%02x..%02x%02x",
                pk->meta.rsa.fields[i].name,
                pk->meta.rsa.fields[i].tag,
                pk->meta.rsa.fields[i].len,
                pk->meta.rsa.fields[i].data[0],
                pk->meta.rsa.fields[i].data[1],
                pk->meta.rsa.fields[i].data[pk->meta.rsa.fields[i].len - 2],
                pk->meta.rsa.fields[i].data[pk->meta.rsa.fields[i].len - 1]
                );
        os_FontDrawText(buf, 5, 40+i*12);
    }
    free(pk);
    pk = NULL;
    
    os_GetKey();
    os_ClrHome();
    
    
    // PKCS#8 EC key
    pk = tls_public_key_import(test2, strlen(test2));
    if(pk==NULL){
        printf("error");
        os_GetKey();
        os_ClrHome();
        return 1;
    }
    for(int i=0; i < 1; i++){
        sprintf(buf, "%s:tag=%u, size=%u: %02x%02x..%02x%02x",
                pk->meta.ec.fields[i].name,
                pk->meta.ec.fields[i].tag,
                pk->meta.ec.fields[i].len,
                pk->meta.ec.fields[i].data[0],
                pk->meta.ec.fields[i].data[1],
                pk->meta.ec.fields[i].data[pk->meta.rsa.fields[i].len - 2],
                pk->meta.ec.fields[i].data[pk->meta.rsa.fields[i].len - 1]
                );
        os_FontDrawText(buf, 5, 40+i*12);
    }
    free(pk);
    pk = NULL;
    
    os_GetKey();
    os_ClrHome();
    
    return 0;
}
