"""Exercise the temporary Wordclock manifest validator; no filesystem writes on device."""
import argparse
import base64
import copy
import getpass
import json
from pathlib import Path
import sys
import time
import uuid
from urllib.request import Request, build_opener, ProxyHandler, HTTPRedirectHandler
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parent.parent


def encode(obj):
    return json.dumps(obj, separators=(',', ':'), ensure_ascii=True).encode('ascii')


def cases(base):
    raw = encode(base)
    yield 'valid baseline', raw, 200
    def changed(name, edit):
        obj = copy.deepcopy(base)
        edit(obj)
        return name, encode(obj), 400
    for key in base:
        yield changed('missing ' + key, lambda m: m.pop(key))
    for key, value in [('format','other'), ('manifestVersion',2), ('manifestVersion',True), ('version','01.0.0'), ('version','3.0'), ('version','3.0.0-dev'), ('version','1'*33+'.0.0'), ('version',3), ('fileCount',11), ('fileCount','12'), ('totalBytes',0), ('totalBytes',base['totalBytes']+1), ('files',{}), ('extra',1)]:
        yield changed('root '+key+'='+str(value)[:40], lambda m: m.update({key:value}))
    for value in [0,2,'1',True,1.5]:
        yield changed('API '+repr(value), lambda m: m['compatibility'].update(webApiVersion=value))
    yield changed('missing file', lambda m: m['files'].pop())
    yield changed('duplicate file', lambda m: m['files'].__setitem__(1, copy.deepcopy(m['files'][0])))
    for key in ['path','size','sha256']:
        yield changed('missing file '+key, lambda m: m['files'][0].pop(key))
    for key, values in [('path',['unknown.js','../app.js','/app.js','APP.JS','app.js.tmp']), ('size',[0,-1,131073,1.5,'123',True,4294967296]), ('sha256',['a'*63,'a'*65,'G'*64,'A'*64,123])]:
        for value in values:
            yield changed('file '+key+'='+repr(value)[:25], lambda m: m['files'][0].update({key:value}))
    def excessive(m):
        for f in m['files'][:4]: f['size']=131072
        m['totalBytes']=sum(f['size'] for f in m['files'])
    yield changed('total above 384 KiB', excessive)
    yield 'invalid JSON', b'invalid',400
    yield 'truncated JSON',raw[:-1],400
    yield 'array root',b'[]',400
    yield 'duplicate root key', b'{"version":"0.0.0",'+raw[1:],400
    yield 'duplicate file key',raw.replace(b'"path":',b'"path":"unknown.js","path":',1),400
    yield 'trailing text',raw+b'garbage',400
    yield 'trailing object',raw+b'{}',400
    yield 'escaped NUL in version',encode(dict(base,version=base['version']+'\x00suffix')),400
    yield 'escaped NUL in path',encode(dict(base,files=[dict(base['files'][0],path=base['files'][0]['path']+'\x00suffix')]+base['files'][1:])),400
    yield 'raw NUL suffix',raw+b'\x00',400
    yield 'duplicate compatibility key',raw.replace(b'"webApiVersion":',b'"webApiVersion":2,"webApiVersion":',1),400
    yield 'Unicode alias key',raw.replace(b'"version":',b'"vers\\u0069on":"0.0.0","version":',1),400
    yield 'escaped ASCII key',raw.replace(b'"version":',b'"vers\\u0069on":',1),400
    yield 'raw NUL inside',raw.replace(b'"version":',b'"version\x00":',1),400
    yield 'trailing comma',raw[:-1]+b',}',400
    yield 'deep nesting',b'['*10+b'0'+b']'*10,400
    yield 'valid whitespace', b' \n'+raw+b'\r\n ',200
    yield 'valid reversed files',encode(dict(base,files=list(reversed(base['files'])))),200


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs):
        return None


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host',required=True)
    p.add_argument('--manifest',type=Path,default=ROOT/'dist'/'web-manifest.json')
    p.add_argument('--repeat',type=int,default=15,choices=range(1,51),metavar='1-50')
    p.add_argument('--oversize',action='store_true',help='Also send one 8193-byte request; may stress RAM')
    a=p.parse_args()
    url=urlsplit(a.host)
    if url.scheme!='http' or not url.hostname or url.username or url.password or url.path not in ('','/') or url.query or url.fragment:
        p.error('Use the local HTTP origin, e.g. http://192.168.1.87')
    host=a.host.rstrip('/')
    raw=a.manifest.read_bytes()
    base=json.loads(raw)
    if base.get('compatibility',{}).get('webApiVersion')!=1 or len(base.get('files',[]))!=12 or len(raw)>8192:
        p.error('Generate a valid API 1 reference manifest first')
    auth='Basic '+base64.b64encode(('admin:'+getpass.getpass('Web update password: ')).encode()).decode()
    opener=build_opener(ProxyHandler({}),NoRedirect())
    def request(path,body=None):
        headers={'Connection':'close'}
        if body is not None:
            boundary = 'wordclock-' + uuid.uuid4().hex
            headers.update({'Authorization':auth,'X-Wordclock-Action':'manifest-test','Content-Type':'multipart/form-data; boundary='+boundary})
            body = ('--'+boundary+'\r\nContent-Disposition: form-data; name="manifest"; filename="web-manifest.json"\r\nContent-Type: application/octet-stream\r\n\r\n').encode('ascii') + body + ('\r\n--'+boundary+'--\r\n').encode('ascii')
        try:
            with opener.open(Request(host+path,data=body,headers=headers),timeout=10) as r:
                return r.status,r.read(16384)
        except HTTPError as e:
            with e: return e.code,e.read(16384)
    def system():
        status,data=request('/api/system')
        if status!=200: raise RuntimeError('System diagnostic HTTP '+str(status))
        return json.loads(data)
    endpoint='/api/dev/web-manifest-test'
    before=system(); print('BEFORE:',json.dumps(before))
    failures=0; passed=0
    def run(name,body,expected):
        nonlocal failures,passed
        status,data=request(endpoint,body)
        try: result=json.loads(data)
        except ValueError: result={}
        ok=status==expected and isinstance(result,dict) and result.get('valid') is (expected==200)
        if expected==200 and ok:
            ok=all(result.get(k)==base[k] for k in ('version','fileCount','totalBytes'))
        print(f'{"PASS" if ok else "FAIL"} | {name} | expected {expected}, got {status} | {data[:240].decode(errors="replace")}')
        passed+=int(ok); failures+=int(not ok)
        time.sleep(.15)
        return ok
    if not run('reference file',raw,200):
        print('STOP: reference rejected; fix baseline first.'); return 1
    try:
        for name,body,expected in cases(base): run(name,body,expected)
        if a.oversize:
            run('8192 bytes',encode(base).ljust(8192,b' '),200)
            run('8193 bytes',encode(base).ljust(8193,b' '),413)
            run('32768 bytes',encode(base).ljust(32768,b' '),413)
            run('recovery after oversize',raw,200)
        for i in range(a.repeat): run(f'repeat {i+1}/{a.repeat}',raw,200)
    except (OSError,URLError,RuntimeError) as e:
        failures+=1; print('STOP: transport error:',e)
    after=system(); print('AFTER:',json.dumps(after))
    for k in ('buildId','storageUsed','webSlot','webSlotSource','webSlotGeneration'):
        if before.get(k)!=after.get(k): failures+=1; print('FAIL | diagnostic changed:',k)
    if after['uptime']<before['uptime']: failures+=1; print('FAIL | possible restart')
    print('Heap delta:',after['freeHeap']-before['freeHeap'],'bytes (not peak usage)')
    print(f'SUMMARY: {passed} passed, {failures} failed')
    return int(failures!=0)


if __name__=='__main__':
    try: sys.exit(main())
    except (OSError, ValueError, RuntimeError) as e:
        print('ERROR:',e,file=sys.stderr); sys.exit(1)
