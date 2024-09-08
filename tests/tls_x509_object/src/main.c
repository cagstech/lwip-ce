#include <ti/screen.h>
#include <ti/getkey.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "lwip/mem.h"

#include "tls/includes/keyobject.h"

// test vectors
const char *test1 = "-----BEGIN CERTIFICATE-----\nMIIDnzCCAoegAwIBAgIUHE/g0NoguFZkQL9VBbXbIm/7WDswDQYJKoZIhvcNAQELBQAwXzELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAk5ZMQswCQYDVQQHDAJOWTERMA8GA1UECgwIY2Fnc3RlY2gxETAPBgNVBAsMCGNhZ3N0ZWNoMRAwDgYDVQQDDAdBbnRob255MB4XDTI0MDkwODA1NDc1M1oXDTI1MDkwODA1NDc1M1owXzELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAk5ZMQswCQYDVQQHDAJOWTERMA8GA1UECgwIY2Fnc3RlY2gxETAPBgNVBAsMCGNhZ3N0ZWNoMRAwDgYDVQQDDAdBbnRob255MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA8q4s1a+ReNvXPOhFhdpNGwCwfR6WHzRoksko2SJCqwhO9b9+0cUM6WQxCPDtAxba8g6FgJTc2m9x/I1gybyn7++ZrtNaMXgICIFza5rh5pBNbtHiL+5v1fy7wIkKo34jK3VryRNQTbb5VJqfGD33OJYUp3BfpShRkIwgxocloqXqwB9UOzUF99icUvC3wDy85y4zolIpNEM8zQqEuQSJIISUQuevo0DlvMtB/DMeGQP64pE5/HDz89+agFka1sDWguGyp3TbzvXxiEoigxsj2208unqozsNIYTRGxPF5deNJ/x+3kW4ivBVzpC01/3ETpiMYotxaEARoO0maBDpKzQIDAQABo1MwUTAdBgNVHQ4EFgQUrv2AiZkx1XiN7qY3wGkpiJ5GCjMwHwYDVR0jBBgwFoAUrv2AiZkx1XiN7qY3wGkpiJ5GCjMwDwYDVR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEA6YccSZu9vRgEZ3oHSpB7LRxYF5FxwH2WCUtnxz3uIafzbjnyP7tLkTL845JeVFgAi/ZHpJGKLOxXIqIffGnUe6wuaYFr2M2QdzkKIRvr0/Mi5XFRX0PI7/dAFZhj5DFtdM9avzdczka4r8AB8nHZwcmlQbxdbs/hv1nVsr6mfh5FntuPY3cNulkLwOhqUCKEFl1CoCpz68ejKhszhTrYWVLTfNrm3HwQlMRqXvmv1jWsh9X8sm/IM1psUPmm95VY+2OxBwJRHh1hYVlBn8RxnCM4EGTAqowTv/r8sktY2gW2HulwdMSzxOlApL5f5yiwKkSmPVU7SIUuC5UVOujblw==\n-----END CERTIFICATE-----";

const char *test2 = "-----BEGIN CERTIFICATE-----\nMIICGTCCAb6gAwIBAgIUelyvLQVjwgP/NKxkCEhKNR4+ihYwCgYIKoZIzj0EAwIwfjELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAk5ZMQwwCgYDVQQHDANOWUMxGjAYBgNVBAoMEUNhZ3NUZWNoIFNlcnZpY2VzMRUwEwYDVQQDDAxjYWdzdGVjaC5jb20xITAfBgkqhkiG9w0BCQEWEmFkbWluQGNhZ3N0ZWNoLmNvbTAeFw0yNDA5MDgxNzI4NTNaFw0yNTA5MDgxNzI4NTNaMFYxCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJOWTEMMAoGA1UEBwwDTllDMRUwEwYDVQQKDAxsd0lQLUNFIFRlc3QxFTATBgNVBAMMDGx3SVAtQ0UgVGVzdDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABK8QfpEyfVr5muIzTlM1zkRG3ggoaXNZi1FDX1JUDDWi49+wFgZTjY9+Jljb6qQB6rdBFGPH+suhcI5Pk52bc3qjQjBAMB0GA1UdDgQWBBQzpfSZI6ZuFuN3md3gLlCT+w3EljAfBgNVHSMEGDAWgBSXy6NjMevtANrQhdJ6whQX/GAmVTAKBggqhkjOPQQDAgNJADBGAiEA7+TFhvDMTydQ9boop5kODjMydBoSH56wBl+40OjEz+ICIQCfmxOLuQTDHsvBvaP8w0OkYQgz4rSAt09ZNmit+vq4Hw==-----END CERTIFICATE\n-----";

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
    int i;
    char buf[128];
    mem_configure(&conf);
    
    struct tls_certificate_context *pk = NULL;
    pk = tls_x509_cert_import(test1, strlen(test1));
    if(pk==NULL){
        printf("error");
        os_GetKey();
        os_ClrHome();
        return 1;
    }
    os_FontDrawText("--certificate--", 5, 40);

    char *sigalg = "unknown";
    char *casigalg = "unknown";
    if(memcmp(pk->algorithms.signature.data, tls_objectids[TLS_OID_SHA256_RSA_ENCRYPTION], pk->algorithms.signature.len) == 0)
        sigalg = "rsa-sha256";
    else if(memcmp(pk->algorithms.signature.data, tls_objectids[TLS_OID_SHA256_ECDSA], pk->algorithms.signature.len) == 0)
        sigalg = "ecdsa-sha256";
    
    if(memcmp(pk->algorithms.ca_signature.data, tls_objectids[TLS_OID_SHA256_RSA_ENCRYPTION], pk->algorithms.ca_signature.len) == 0)
        casigalg = "rsa-sha256";
    else if(memcmp(pk->algorithms.ca_signature.data, tls_objectids[TLS_OID_SHA256_ECDSA], pk->algorithms.ca_signature.len) == 0)
        casigalg = "ecdsa-sha256";
   
    sprintf(buf, "sigAlg: %s", sigalg);
    os_FontDrawText(buf, 10, 52);
    sprintf(buf, "issuer: %.*s", pk->issuer.len, pk->issuer.data);
    os_FontDrawText(buf, 10, 64);
    sprintf(buf, "subject: %.*s", pk->subject.len, pk->subject.data);
    os_FontDrawText(buf, 10, 76);
    os_FontDrawText("expiry:", 10, 88);
    sprintf(buf, "before: %.*s", pk->valid.before.len, pk->valid.before.data);
    os_FontDrawText(buf, 20, 100);
    sprintf(buf, "after: %.*s", pk->valid.after.len, pk->valid.after.data);
    os_FontDrawText(buf, 20, 112);
    sprintf(buf, "caSigAlg: %s", casigalg);
    os_FontDrawText(buf, 10, 124);
    for(i=0; i < 2; i++){
        sprintf(buf, "%s:tag=%u, size=%u: %02x%02x..%02x%02x",
                pk->pubkey.rsa.fields[i].name,
                pk->pubkey.rsa.fields[i].tag,
                pk->pubkey.rsa.fields[i].len,
                pk->pubkey.rsa.fields[i].data[0],
                pk->pubkey.rsa.fields[i].data[1],
                pk->pubkey.rsa.fields[i].data[pk->pubkey.rsa.fields[i].len - 2],
                pk->pubkey.rsa.fields[i].data[pk->pubkey.rsa.fields[i].len - 1]
                );
        os_FontDrawText(buf, 10, 136+i*12);
    }
    sprintf(buf, "%s:tag=%u, size=%u: %02x%02x..%02x%02x",
            pk->signature.name,
            pk->signature.tag,
            pk->signature.len,
            pk->signature.data[0],
            pk->signature.data[1],
            pk->signature.data[pk->signature.len - 2],
            pk->signature.data[pk->signature.len - 1]
            );
    os_FontDrawText(buf, 10, 136+i*12);
    free(pk);
    pk = NULL;
    
    os_GetKey();
    os_ClrHome();
    
    pk = tls_x509_cert_import(test2, strlen(test2));
    if(pk==NULL){
        printf("error");
        os_GetKey();
        os_ClrHome();
        return 1;
    }
    os_FontDrawText("--certificate--", 5, 40);
    
    sigalg = "unknown";
    casigalg = "unknown";
    if(memcmp(pk->algorithms.signature.data, tls_objectids[TLS_OID_SHA256_RSA_ENCRYPTION], pk->algorithms.signature.len) == 0)
        sigalg = "rsa-sha256";
    else if(memcmp(pk->algorithms.signature.data, tls_objectids[TLS_OID_SHA256_ECDSA], pk->algorithms.signature.len) == 0)
        sigalg = "ecdsa-sha256";
    
    if(memcmp(pk->algorithms.ca_signature.data, tls_objectids[TLS_OID_SHA256_RSA_ENCRYPTION], pk->algorithms.ca_signature.len) == 0)
        casigalg = "rsa-sha256";
    else if(memcmp(pk->algorithms.ca_signature.data, tls_objectids[TLS_OID_SHA256_ECDSA], pk->algorithms.ca_signature.len) == 0)
        casigalg = "ecdsa-sha256";
   
    sprintf(buf, "sigAlg: %s", sigalg);
    os_FontDrawText(buf, 10, 52);
    sprintf(buf, "issuer: %.*s", pk->issuer.len, pk->issuer.data);
    os_FontDrawText(buf, 10, 64);
    sprintf(buf, "subject: %.*s", pk->subject.len, pk->subject.data);
    os_FontDrawText(buf, 10, 76);
    os_FontDrawText("expiry:", 10, 88);
    sprintf(buf, "before: %.*s", pk->valid.before.len, pk->valid.before.data);
    os_FontDrawText(buf, 20, 100);
    sprintf(buf, "after: %.*s", pk->valid.after.len, pk->valid.after.data);
    os_FontDrawText(buf, 20, 112);
    sprintf(buf, "caSigAlg: %s", casigalg);
    os_FontDrawText(buf, 10, 124);
    for(i=0; i < 1; i++){
        sprintf(buf, "%s:tag=%u, size=%u: %02x%02x..%02x%02x",
                pk->pubkey.ec.fields[i].name,
                pk->pubkey.ec.fields[i].tag,
                pk->pubkey.ec.fields[i].len,
                pk->pubkey.ec.fields[i].data[0],
                pk->pubkey.ec.fields[i].data[1],
                pk->pubkey.ec.fields[i].data[pk->pubkey.ec.fields[i].len - 2],
                pk->pubkey.ec.fields[i].data[pk->pubkey.ec.fields[i].len - 1]
                );
        os_FontDrawText(buf, 10, 136+i*12);
    }
    sprintf(buf, "%s:tag=%u, size=%u: %02x%02x..%02x%02x",
            pk->signature.name,
            pk->signature.tag,
            pk->signature.len,
            pk->signature.data[0],
            pk->signature.data[1],
            pk->signature.data[pk->signature.len - 2],
            pk->signature.data[pk->signature.len - 1]
            );
    os_FontDrawText(buf, 10, 136+i*12);
    free(pk);
    pk = NULL;
    
    os_GetKey();
    os_ClrHome();
    
    return 0;
}
