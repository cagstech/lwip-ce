#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "../includes/asn1.h"

void rmemcpy(void *dest, void *src, size_t len);

bool tls_asn1_decoder_init(struct tls_asn1_context *ctx, const uint8_t *data, size_t len){
    if((ctx==NULL) ||
       (data==NULL) ||
       (len==0)) return false;
    memset(ctx, 0, sizeof(struct tls_asn1_context));
    ctx->node[0].start = data;
    ctx->node[0].next = data + len;
    return true;
}


bool tls_asn1_decode_next(struct tls_asn1_context *ctx, uint8_t *tag, uint8_t **data, size_t *len, uint8_t *depth){
   

    if(ctx==NULL) return false;
    uint8_t this_tag;
    size_t this_len;
    uint8_t *asn1_current;
    
restart:
    asn1_current = (uint8_t*)ctx->node[ctx->depth].start;
    this_len = 0;
        
    while(*asn1_current == 0){
        // remove leading 0s
        asn1_current++;
    }
        
    if(ctx->depth >= ASN1_MAX_DEPTH) return false;
        
    // get tag of element
    this_tag = *asn1_current++;

    if(asn1_current > ctx->node[ctx->depth].next){
        if(--ctx->depth==0) return false;
        goto restart;
    }
    
    // get byte 2 => size or size of size
    uint8_t byte_2nd = *asn1_current++;
    if((byte_2nd>>7) & 1){
        uint8_t size_len = byte_2nd & 0x7f;
        if(size_len > 3) return false;
        rmemcpy((uint8_t*)&this_len, asn1_current, size_len);
        asn1_current += size_len;
    }
    else
        this_len = byte_2nd;
            
    if(data) *data = asn1_current;
    if(tag) *tag = this_tag;
    if(len) *len = this_len;
    if(depth) *depth = ctx->depth;
        
    ctx->node[ctx->depth].start = asn1_current + this_len;
    if(tls_asn1_getform(this_tag)){
        ctx->depth++;
        ctx->node[ctx->depth].start = asn1_current;
        ctx->node[ctx->depth].next = asn1_current + this_len;
    }
    
    return true;
}

size_t tls_asn1_encode_segment(uint8_t tag, const uint8_t *data, size_t len, uint8_t *output){
    if((data==NULL) ||
       (len==0) ||
       (output==NULL)) return 0;
    
    // if sequence or set, constructed should be set
    if(((tag & 0b11111) == ASN1_SEQUENCE) ||
        ((tag & 0b11111) == ASN1_SET))
        tag |= ASN1_CONSTRUCTED;
    
    
    size_t pos = 0;
    output[pos++] = tag;
    if(len > (0b1111111)){
        // generate weird length format
        uint8_t size_len = (len > 0xffff) + (len > 0xFF) + 1;
        output[pos++] = size_len | 0b10000000;
        rmemcpy((uint8_t*)&output[pos], (uint8_t*)&len, size_len);
        pos += size_len;
    }
    else
        output[pos++] = len;
    
    memcpy(&output[pos], data, len);
    return pos+len;
}
