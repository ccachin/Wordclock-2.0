"""Local destructive-to-staging only test; never activates an image."""
import argparse, base64, getpass, hashlib, json, time
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from urllib.parse import urlencode

p = argparse.ArgumentParser()
p.add_argument('--base', default='http://192.168.1.87')
p.add_argument('--image', required=True, type=Path)
p.add_argument('--delay-ms', type=float, default=0)
p.add_argument('--activate', action='store_true')
a = p.parse_args()
if a.delay_ms < 0: p.error('--delay-ms must be nonnegative')
image = a.image.read_bytes()
sig = a.image.with_suffix('.sig').read_bytes()
assert len(sig) == 256 and image[0] == 0xe9
hash_ = hashlib.sha256(image).hexdigest()
auth = 'Basic ' + base64.b64encode(('admin:'+getpass.getpass('Mot de passe administrateur : ')).encode()).decode()

def req(path, params=None, method='POST', body=None, expect=200):
    url = a.base.rstrip('/')+'/api/system/'+path
    if params: url += '?'+urlencode(params)
    r = Request(url, data=body, method=method, headers={'Authorization':auth, 'X-Wordclock-Action':'firmware-update', 'Content-Type':'text/plain'})
    try:
        with urlopen(r, timeout=30) as f: code, payload = f.status, f.read()
    except HTTPError as e: code, payload = e.code, e.read()
    result = json.loads(payload)
    if code != expect: raise RuntimeError(f'{path}: HTTP {code}, expected {expect}: {json.dumps(result, ensure_ascii=False)}')
    return result

def sysinfo(): return json.load(urlopen(a.base.rstrip('/')+'/api/system',timeout=15))

def start():
    for attempt in range(1, 13):
        try:
            t=req('firmware/authorize',dict(size=len(image),sha256=hash_,confirm='INSTALL',signature=sig.hex()))['token']
            break
        except RuntimeError as exc:
            if 'HTTP 503' not in str(exc) or 'Insufficient verification memory' not in str(exc):
                raise
            if attempt == 12:
                raise
            print(f'Memory recovery: attempt {attempt}/12; retry in 5 seconds',flush=True)
            time.sleep(5)
    s=req('firmware/start',dict(token=t,confirm='INSTALL'))
    assert s['chunkSize'] in (512, 1024) and s['offset']==0
    return s['token'], s['chunkSize']

def block(t, off, data, expect=200):
    return req('firmware/chunk',dict(token=t,offset=off), 'PUT',data,expect)

def send(t,data,chunk_size):
    for off in range(0,len(data),chunk_size):
        part=data[off:off+chunk_size]
        try:
            assert block(t,off,part)['offset']==off+len(part)
        except Exception:
            print(f'Failed block: offset={off} length={len(part)}',flush=True)
            raise
        if a.delay_ms: time.sleep(a.delay_ms/1000)
        if off%65536==0:
            state=req('firmware/status',dict(token=t),method='GET')
            print(f'{off+len(part)}/{len(data)} heap={state["freeHeap"]} max={state["maxFreeBlock"]}',flush=True)

before=sysinfo(); print('Initial:', before['buildId'],before['uptime'])
pre=req('firmware',method='GET'); assert pre['protocol']==2
if a.activate:
    t, chunk_size = start()
    activated = False
    try:
        send(t, image, chunk_size)
        r = req('firmware/verify', dict(token=t))
        assert r['verified'] is True
        print('Firmware verified', flush=True)

        if input('Type INSTALL to activate: ') != 'INSTALL':
            raise SystemExit('Activation cancelled')

        r = req('firmware/activate', dict(token=t, confirm='INSTALL'), expect=202)
        activated = True
        print('Activation accepted:', r, flush=True)

    finally:
        if not activated:
            try:
                req('firmware/cancel', dict(token=t))
            except Exception as exc:
                print('Cancel response:', exc, flush=True)

    for attempt in range(30):
        time.sleep(5)
        try:
            after = sysinfo()
        except Exception:
            continue

        if after['buildId'] != before['buildId']:
            assert after['sketchBytes'] == len(image), after
            print(json.dumps(after, indent=2), flush=True)
            print('PASS: new firmware running', flush=True)
            break
    else:
        raise RuntimeError('New firmware build not detected')

    raise SystemExit(0)

for altered in (True,False):
    t,chunk_size=start()
    try:
        if altered:
            block(t,chunk_size,image[:chunk_size],409)
            block(t,0,image[:chunk_size]); block(t,0,image[:chunk_size],409)
            req('firmware/cancel',dict(token=t)); t,chunk_size=start()
        data=bytearray(image)
        if altered: data[len(data)//2]^=1
        send(t,bytes(data),chunk_size)
        r=req('firmware/verify',dict(token=t),expect=422 if altered else 200)
        if altered: assert r['error']=='SHA-256 mismatch'
        else: assert r['verified'] is True
        print('Altered rejected' if altered else 'Valid verified; cancelling',flush=True)
    finally:
        try: req('firmware/cancel',dict(token=t))
        except RuntimeError as exc:
            if 'HTTP 403' in str(exc) and 'Invalid session' in str(exc):
                print('Session already closed',flush=True)
            else:
                raise
    after=sysinfo(); assert after['buildId']==before['buildId'] and after['uptime']>=before['uptime'],after
    print('System:',after['uptime'],after['freeHeap'],after['maxFreeBlock'],flush=True)
print('PASS: no activation requested')
