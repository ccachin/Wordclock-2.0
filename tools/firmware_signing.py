"""Generate a local RSA signing key; sign and verify firmware with the pinned public key."""
import argparse
import re
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, padding

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / 'include/firmware_signing_key.h'

def render(public):
    n = public.public_numbers()
    if public.key_size != 2048 or n.e != 65537:
        raise ValueError('Expected RSA-2048, exponent 65537')
    return '#pragma once\n#include <stdint.h>\nstatic const uint8_t WC_RSA_N[] = {' + ','.join(f'0x{x:02x}' for x in n.n.to_bytes(256,'big')) + '};\n'

def pinned():
    raw = bytes(int(x,16) for x in re.findall(r'0x([0-9a-f]{2})', HEADER.read_text()))
    if len(raw) != 256: raise ValueError('Invalid pinned key')
    return rsa.RSAPublicNumbers(65537,int.from_bytes(raw,'big')).public_key()

def verify(image, signature):
    pinned().verify(signature, image, padding.PKCS1v15(), hashes.SHA256())

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=['init','sign','verify'])
    p.add_argument('--key', type=Path, default=Path.home()/'.wordclock-signing'/'private.pem')
    p.add_argument('--image', type=Path)
    a = p.parse_args()
    if ROOT == a.key.resolve() or ROOT in a.key.resolve().parents: raise ValueError('Keep private key outside repository')
    if a.action == 'init':
        if HEADER.exists() and not a.key.exists(): raise ValueError('Pinned key already exists: restore private key')
        if not a.key.exists():
            a.key.parent.mkdir(parents=True,exist_ok=True)
            key = rsa.generate_private_key(public_exponent=65537,key_size=2048)
            with a.key.open('xb') as f:
                f.write(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()))
        key = serialization.load_pem_private_key(a.key.read_bytes(),None)
        header = render(key.public_key())
        if HEADER.exists() and HEADER.read_text() != header: raise ValueError('Pinned key differs; refusing rotation')
        HEADER.write_text(header)
    else:
        if not a.image: p.error('--image required')
        data = a.image.read_bytes()
        sig = a.image.with_suffix('.sig')
        if a.action == 'sign':
            key = serialization.load_pem_private_key(a.key.read_bytes(),None)
            signed = key.sign(data,padding.PKCS1v15(),hashes.SHA256())
            verify(data,signed)
            sig.write_bytes(signed)
        else: verify(data,sig.read_bytes())
    print(a.action + ': OK')

if __name__ == '__main__': main()
