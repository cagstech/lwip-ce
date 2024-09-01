
from Crypto.PublicKey import RSA

mykey = RSA.generate(2048)


with open("testfile.pem", "wb") as f:
    f.write(mykey.public_key().export_key())
