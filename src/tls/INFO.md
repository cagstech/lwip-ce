# TLS IMPLEMENTATION NOTES #


## True Random Number Generator ##

### Source-Finding Algorithm ###
- Poll 513 bytes starting at $D65800 - unmapped memory.
- Repeat 256 times per byte:
    - Xor two consecutive reads from byte together
    - Add number of set bits to a "score".
    - If new score better than current, set new score.
- Save address with highest score.
- If selected score less than `256 * 8 / 3`, return false.
- Else return true.

### Entropy Extraction Algorithm ###
- For each byte in a 119-byte entropy pool:
    - Read from selected source address once
    - Read 16 more times xoring into cumulative result
    - Write to current location in pool.
- Feed entropy pool to SHA-256 hash.
- Compress output hash into a `uint64_t` by xoring 4 bytes of the digest into a each byte of the `uint64_t`.
- Return the resulting `uint64_t`.

### Approximate Entropy of Generator ###
- I will approximate minimum entropy for the source using the minimum score like so:
    - P0 = probability 0s = `(256 * 8 / 3) / (256 * 8)` = 0.333
    - P1 = probability 1s = `1 - P0` = 0.667
    - *vice versa doesn't affect the computation*
    - H = `(P0 * log2(1/P0)) + (P1 * log2(1/P1))`
    - H = `(0.333 * log2(1/0.333)) + (0.667 * log2(1/0.667))`
    - H = `(0.333 * 1.586) + (0.667 * 0.584) = 0.918`
    - E = `2.576 * sqrt((0.333 * (1 - 0.333)) / 256) = 0.076` margin of error at 99% confidence
    - H = `0.918 +/- 0.076 bits/byte`
    - H[low-high] = `[0.842-0.994] bits/byte`
    - H[low] on a 119-byte pool yields 100.2 bits of entropy, sufficient for a u64.
    - *This is an estimate based on minimum allowed score. Actual source may be higher.*
    - *It should be noted that Cemetech user `Zeroko` ran Dieharders on 1G of data extracted directly from an entropic source in the unmapped region (using our extraction process) and it passed all tests.*


https://www.rfc-editor.org/rfc/pdfrfc/rfc8446.txt.pdf

TLS 1.3
----------
- TLS_AES_128_GCM_SHA256 (0x13, 0x01)
- TLS_AES_256_GCM_SHA384 (0x13, 0x02) ** if we implement SHA-384
- TLS_AES_128_CCM_SHA256 (0x13, 0x04) ** if we implement CCM
- TLS_AES_128_CCM_8_SHA256 (0x13, 0x05) ** if we implement CCM
  
I think we should only implement TLS 1.3. It seems to be a shorter handshake?

TODO
---------
- RSA bigint ^ bigint % bigint
- RSA PSS
- SECP256r1 ECDHE/ECDSA


TLS NEGOTIATION PROCESS
========================

Source: https://www.cloudflare.com/learning/ssl/what-happens-in-a-tls-handshake/

TLS v1.3
---------

1. Client hello: The client sends a client hello message with the protocol version, the client random, and a list of cipher suites. Because support for insecure cipher suites has been removed from TLS 1.3, the number of possible cipher suites is vastly reduced. The client hello also includes the parameters that will be used for calculating the premaster secret. Essentially, the client is assuming that it knows the server’s preferred key exchange method (which, due to the simplified list of cipher suites, it probably does). This cuts down the overall length of the handshake — one of the important differences between TLS 1.3 handshakes and TLS 1.0, 1.1, and 1.2 handshakes.

2. Server generates master secret: At this point, the server has received the client random and the client's parameters and cipher suites. It already has the server random, since it can generate that on its own. Therefore, the server can create the master secret.

3. Server hello and "Finished": The server hello includes the server’s certificate, digital signature, server random, and chosen cipher suite. Because it already has the master secret, it also sends a "Finished" message.

4. Final steps and client "Finished": Client verifies signature and certificate, generates master secret, and sends "Finished" message.

5. Secure symmetric encryption achieved


0-RTT mode for session resumption
----------------------------------

TLS 1.3 also supports an even faster version of the TLS handshake that does not require any round trips, or back-and-forth communication between client and server, at all. If the client and the server have connected to each other before (as in, if the user has visited the website before), they can each derive another shared secret from the first session, called the "resumption main secret." The server also sends the client something called a session ticket during this first session. The client can use this shared secret to send encrypted data to the server on its first message of the next session, along with that session ticket. And TLS resumes between client and server.


