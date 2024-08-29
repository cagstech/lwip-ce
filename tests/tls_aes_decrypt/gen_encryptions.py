
from Crypto.Cipher import AES


key = b"\x5a\x99\xaf\x84\x89\x99\xe1\xa1\x76\x99\x30\xbc\x9f\xea\xa2\xbd\xd2\xec\x0a\x03\xaa\x45\xa5\x49\x36\x66\xe6\x99\xa7\x02\x01\x57"
iv = b"\xea\xfb\xb9\xac\xdd\x83\xfb\x66\xda\xa3\xca\x93\xc7\x2e"
associated = b"Some random header"
msg = b"Leading the way to the future!"

cipher = AES.new(key, AES.MODE_GCM, nonce=iv)
#cipher.update(associated)
ciphertext, tag = cipher.encrypt_and_digest(msg)
print(f"{ciphertext.hex()}\n{tag.hex()}\n")
