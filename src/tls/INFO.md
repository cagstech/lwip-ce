QUICKNOTES - Implementing TLS
==============================

https://www.rfc-editor.org/rfc/pdfrfc/rfc8446.txt.pdf
I think we should only implement TLS 1.3. It seems to be a shorter handshake.

Mandatory (rfc8446)
--------------------
- TLS_AES_128_GCM_SHA256 (symmetric)
- rsa_pkcs1_sha256 (for certificates/digital signatures)
- rsa_pss_rsae_sha256 (for CertificateVerify and certificates)
- ecdsa_secp256r1_sha256
- secp256r1 (kex)

** This means we must implement the following **
- SHA256
- AES-128 galois counter mode
- RSA encrypt and decrypt
- probabilistic signature scheme for RSA
- elliptic curve digital signing algorithm using secp256r1
- elliptic curve kex using secp256r1

** That also means I get to undertake the fun task of researching the ObjectIDs for these suites and adding them here. **



TLS NEGOTIATION PROCESS
========================

Source: https://www.cloudflare.com/learning/ssl/what-happens-in-a-tls-handshake/

TLS v1.0, 1.1, 1.2
--------------------

1. The 'client hello' message: The client initiates the handshake by sending a "hello" message to the server. The message will include which TLS version the client supports, the cipher suites supported, and a string of random bytes known as the "client random."

2. The 'server hello' message: In reply to the client hello message, the server sends a message containing the server's SSL certificate, the server's chosen cipher suite, and the "server random," another random string of bytes that's generated by the server.

3. Authentication: The client verifies the server's SSL certificate with the certificate authority that issued it. This confirms that the server is who it says it is, and that the client is interacting with the actual owner of the domain.

4. The premaster secret: The client sends one more random string of bytes, the "premaster secret." The premaster secret is encrypted with the public key and can only be decrypted with the private key by the server. (The client gets the public key from the server's SSL certificate.)

5. Private key used: The server decrypts the premaster secret.

6. Session keys created: Both client and server generate session keys from the client random, the server random, and the premaster secret. They should arrive at the same results.

7. Client is ready: The client sends a "finished" message that is encrypted with a session key.

8. Server is ready: The server sends a "finished" message encrypted with a session key.

9. Secure symmetric encryption achieved: The handshake is completed, and communication continues using the session keys.


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

