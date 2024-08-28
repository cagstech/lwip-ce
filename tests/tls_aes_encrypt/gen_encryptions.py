
from Crypto.Cipher import AES

key = b"\xEE\x89\x19\xC3\x8D\x53\x7A\xD6\x04\x19\x9E\x77\x0B\xE0\xE0\x4C"
iv = b"\x79\xA6\xDE\xDF\xF0\xA2\x7C\x7F\xEE\x0B\x8E\xF5\x12\x63\xA4\x8A"
associated = b"Some random header"
msg = b"The lazy fox jumped over the dog!"

cipher = AES.new(key, AES.MODE_GCM, nonce=iv)
cipher.update(associated)
ciphertext, tag = cipher.encrypt_and_digest(msg)
print(f"{ciphertext.hex()}\n{tag.hex()}\n")
