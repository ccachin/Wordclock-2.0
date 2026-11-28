#!/usr/bin/env python3
"""Generate a local trust anchor for the optional diagnostic. No insecure TLS."""
import hashlib
import socket
import ssl
from pathlib import Path
HOST = 'api.open-meteo.com'

def connect(ctx, verified_chain=False):
    ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
    with socket.create_connection((HOST,443),timeout=8) as tcp:
        with ctx.wrap_socket(tcp,server_hostname=HOST) as tls:
            if verified_chain and hasattr(tls,'get_verified_chain'):
                return tls.get_verified_chain()
            return None

def main():
    default=ssl.create_default_context()
    chain=connect(default, True)
    roots=default.get_ca_certs(binary_form=True)
    # Older Python has no verified-chain API. Try only likely public CA roots
    # from the existing OS trust store; each candidate must validate hostname
    # and chain by itself. Never trust a certificate simply sent by the server.
    hints=(b'GTS Root',b'GlobalSign',b'ISRG Root',b'DigiCert',b'Amazon Root',b'Sectigo',b'USERTrust',b'Baltimore')
    candidates=[]
    if chain and chain[-1] in roots: candidates.append(chain[-1])
    candidates.extend(r for r in roots if any(h in r for h in hints) and r not in candidates)
    for root in candidates:
        pem=ssl.DER_cert_to_PEM_cert(root)
        single=ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        single.load_verify_locations(cadata=pem)
        try: connect(single)
        except ssl.SSLCertVerificationError: continue
        print('Verified single CA SHA256:',hashlib.sha256(root).hexdigest())
        path=Path(__file__).resolve().parents[1]/'include'/'weather_diag_ca.local.h'
        path.write_text('#pragma once\nstatic const char WeatherDiagRootCA[] PROGMEM = R"WCCA(\n'+pem+')WCCA";\n',encoding='ascii')
        print('Generated:',path)
        return
    raise SystemExit('No matching public trust anchor found; do not bypass verification. Budget-only test remains available.')
if __name__=='__main__': main()
