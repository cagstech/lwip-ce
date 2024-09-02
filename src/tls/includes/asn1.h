
#ifndef tls_asn1_h
#define tls_asn1_h


#define ASN1_MAX_DEPTH  10

enum tls_asn1_tags {
    ASN1_RESVD = 0,             /**< RESERVED */
    ASN1_BOOLEAN,               /**< defines a BOOLEAN object */
    ASN1_INTEGER,               /**< defines an INTEGER object */
    ASN1_BITSTRING,             /**< defines a BIT STRING object */
    ASN1_OCTETSTRING,           /**< defines an OCTET STRING object */
    ASN1_NULL,                  /**< defines a NULL object (0 size, no data) */
    ASN1_OBJECTID,              /**< defines an OBJECT IDENTIFIER */
    ASN1_OBJECTDESC,            /**< defines an OBJECT DESCRIPTION */
    ASN1_INSTANCE,              /**< defines an INSTANCE */
    ASN1_REAL,                  /**< defines a REAL object */
    ASN1_ENUMERATED,
    ASN1_EMBEDDEDPDV,
    ASN1_UTF8STRING,
    ASN1_RELATIVEOID,
    ASN1_SEQUENCE = 16,         /**< defines a SEQUENCE */
    ASN1_SET,                   /**< defines a SET */
    ASN1_NUMERICSTRING,
    ASN1_PRINTABLESTRING,
    ASN1_TELETEXSTRING,
    ASN1_VIDEOTEXSTRING,
    ASN1_IA5STRING,
    ASN1_UTCTIME,
    ASN1_GENERALIZEDTIME,
    ASN1_GRAPHICSTRING,
    ASN1_VISIBLESTRING,
    ASN1_GENERALSTRING,
    ASN1_UNIVERSALSTRING,
    ASN1_CHARSTRING,
    ASN1_BMPSTRING
};

enum tls_asn1_classes {
    ASN1_UNIVERSAL      = (0<<6),       /**< tags defined in the ASN.1 standard. Most use cases on calc will be this. */
    ASN1_APPLICATION    = (1<<6),       /**< tags unique to a particular application. */
    ASN1_CONTEXTSPEC    = (2<<6),       /**< tags that need to be identified within a particular, well-definded context. */
    ASN1_PRIVATE        = (3<<6)        /**< reserved for use by a specific entity for their applications. */
};

enum tls_asn1_forms {
    ASN1_PRIMITIVE      = (0<<5),       /**< this element should contain no nested elements. */
    ASN1_CONSTRUCTED    = (1<<5)        /**< this element contains nested elements. */
};

struct _asn1_node {
    const uint8_t *start;
    const uint8_t *next;
};

struct tls_asn1_context {
    uint8_t depth;
    struct _asn1_node node[ASN1_MAX_DEPTH];
};

#define tls_asn1_gettag(tag)        ((tag) & 0b111111)
#define tls_asn1_getclass(tag)      (((tag)>>6) & 0b11)
#define tls_asn1_getform(tag)       (((tag)>>5) & 1)

bool tls_asn1_decoder_init(struct tls_asn1_context *ctx, const uint8_t *data, size_t len);
bool tls_asn1_decode_next(struct tls_asn1_context *ctx, uint8_t *tag, uint8_t **data, size_t *len, uint8_t *depth);


size_t tls_asn1_encode_segment(uint8_t tag, const uint8_t *data, size_t len, uint8_t *output);

#endif
