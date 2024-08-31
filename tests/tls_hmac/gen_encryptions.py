
import hmac, hashlib

key = b"\x3d\x93\x98\x80\x3e\x06\xae\x7d\x7c\x36\x78\xc7\x5e\x95\xe7\x2a"

msg1 = b"The fox jumped over the dog!";
msg2 = b"Leading the way to the future!";
msg3 = b"What is the answer to this question?";


print(f"{hmac.digest(key, msg1, hashlib.sha256).hex()}")
print(f"{hmac.digest(key, msg2, hashlib.sha256).hex()}")
print(f"{hmac.digest(key, msg3, hashlib.sha256).hex()}")

