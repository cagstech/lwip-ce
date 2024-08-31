#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ti/vars.h>
#include "../includes/hash.h"


void boot_Sha256Init(void);
void boot_Sha256Part(uint8_t *data, size_t len);
void boot_Sha256Hash(uint8_t *digest);

void *sequence = NULL;
 
enum flash_lock_status {
    LOCKED = 0x00,
    PARTIALLY_UNLOCKED = 0x04,
    FULLY_UNLOCKED = 0x0C
};

void flash_unlock(void);
void flash_lock(void);
void flash_sequence(void *sequence);
uint8_t get_flash_lock_status(void);
void set_priv(void);
void reset_priv(void);
uint8_t priv_upper(void);

 /* Privileged code must be reset prior to running this */
void *getSequence(void) {
    static const uint8_t seq[] = {0x3E, 0x04, 0xF3, 0x18, 0x00, 0xF3, 0xED, 0x7E, 0xED, 0x56, 0xED, 0x39, 0x28, 0xED, 0x38, 0x28, 0xCB, 0x57, 0xC9};
    int archived;
    var_t *fp;
    if ((fp = os_GetAppVarData("FW", &archived))) return fp->data;
    if((fp = os_CreateAppVar("FW", sizeof seq))){
        memcpy(fp->data, seq, sizeof seq);
        // how to set archive?
        return fp->data;
    }
    return NULL;
}

void unlock_flash_full(void) {
    if(!sequence) {
        if(priv_upper() == 0xFF) {
            reset_priv();
        }
        sequence = getSequence();
    }
    set_priv();
    if(get_flash_lock_status() == LOCKED)
        flash_unlock();
    if(get_flash_lock_status() == PARTIALLY_UNLOCKED)
        flash_sequence(sequence);
}

bool tls_sha256hw_init(struct tls_sha256_context *ctx __attribute__((unused))){
    unlock_flash_full();
    boot_Sha256Init();
    flash_lock();
    return true;
}

void tls_sha256hw_update(struct tls_sha256_context *ctx __attribute__((unused)), const uint8_t *data, size_t len){
    unlock_flash_full();
    boot_Sha256Part(data, len);
    flash_lock();
}

void tls_sha256hw_digest(struct tls_sha256_context *ctx __attribute__((unused)), uint8_t *digest){
    unlock_flash_full();
    boot_Sha256Hash(digest);
    flash_lock();
}
