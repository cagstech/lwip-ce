#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "../includes/asn1.h"

void rmemcpy(void *dest, void *src, size_t len);

void tls_asn1_set_node(struct tls_asn1_context *ctx, const uint8_t *data, size_t len){
    ctx->node[ctx->depth].start = ctx->node[ctx->depth].start = data;
    ctx->node[ctx->depth].next = data + len;
}

bool tls_asn1_decoder_init(struct tls_asn1_context *ctx, const uint8_t *data, size_t len){
    if((ctx==NULL) ||
       (data==NULL) ||
       (len==0)) return false;
    memset(ctx, 0, sizeof(struct tls_asn1_context));
    tls_asn1_set_node(ctx, data, len);
    return true;
}


bool tls_asn1_decoder_next(struct tls_asn1_context *ctx, uint8_t *tag, uint8_t **data, size_t *len, uint8_t *depth){
    
    if(ctx==NULL) return false;
    
    // get start of data at current depth
    uint8_t *asn1_current = ctx->node[ctx->depth].start;
    printf("d=%u, start=%p end=%p\n", ctx->depth, asn1_current, ctx->node[ctx->depth].next);
    
    uint8_t this_tag;
    size_t this_len = 0;
    
    while(*asn1_current == 0){
        // remove leading 0s
        asn1_current++;
    }
    
    
    
    if(ctx->depth == ASN1_MAX_DEPTH) {
        printf("max depth reached");
        return false;
    }
    
    // get tag of element
    this_tag = *asn1_current++;
    
    // get byte 2 => size or size of size
    uint8_t byte_2nd = *asn1_current++;
    if((byte_2nd>>7) & 1){
        uint8_t size_len = byte_2nd & 0x7f;
        if(size_len > 3) {
            printf("invalid size");
            return false;
        }
        rmemcpy((uint8_t*)&this_len, asn1_current, size_len);
        asn1_current += size_len;
    }
    else
        this_len = byte_2nd;
    
    if(data) *data = asn1_current;
    if(tag) *tag = this_tag;
    if(len) *len = this_len;
    if(depth) *depth = ctx->depth;
    
    if(ctx->depth){
        if((asn1_current + this_len) < ctx->node[ctx->depth].next)
            ctx->depth++;
        else{
            ctx->depth--;
            if(ctx->depth==0) return false;
        }
    }
    else
        ctx->depth++;
    // set stats for inner node
    ctx->node[ctx->depth].start = asn1_current;
    ctx->node[ctx->depth].next = asn1_current + this_len;
    
    
    return true;
}

