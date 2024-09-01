
#ifndef tls_asn1_h
#define tls_asn1_h

struct _asn1_node {
    uint8_t *start;
    uint8_t *next;
};

#define ASN1_MAX_DEPTH  10

struct tls_asn1_context {
    uint8_t depth;
    struct _asn1_node root;
    struct _asn1_node node[ASN1_MAX_DEPTH];
};

#define tls_asn1_gettag(tag)        ((tag) & 0b111111)
#define tls_asn1_getclass(tag)      (((tag)>>6) & 0b11)
#define tls_asn1_getform(tag)       (((tag)>>5) & 1)

bool tls_asn1_decoder_init(struct tls_asn1_context *ctx, const uint8_t *data, size_t len);
bool tls_asn1_decoder_next(struct tls_asn1_context *ctx, uint8_t *tag, uint8_t **data, size_t *len, uint8_t *depth);

#endif
