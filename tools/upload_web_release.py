"""Transfer, verify and optionally activate a web release."""
import argparse
import base64
import getpass
import hashlib
import json
from pathlib import Path
import urllib.request
import urllib.parse
import urllib.error
import uuid


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host', default='http://wordclock.local')
    p.add_argument('--manifest', type=Path, default=Path('dist/web-manifest.json'))
    p.add_argument('--source', type=Path, default=Path('data/www'))
    p.add_argument('--keep', action='store_true', help='Leave the completed test session until timeout or manual cancellation')
    p.add_argument('--test', choices=['corrupt','incomplete','unknown'], help='Expect device rejection')
    p.add_argument('--activate', action='store_true')
    p.add_argument('--rollback', action='store_true')
    a = p.parse_args()
    if (a.activate or a.rollback) and (a.test or a.keep) or (a.activate and a.rollback): p.error('Incompatible options')
    if a.test and a.keep: p.error('--test and --keep are incompatible')
    token = base64.b64encode(('admin:' + getpass.getpass('Admin password: ')).encode()).decode()
    base = a.host.rstrip('/') + '/api/system/web-update/'
    def request(route, method='GET', body=None, mime='text/plain'):
        req = urllib.request.Request(base + route, data=body, method=method, headers={
            'Authorization': 'Basic ' + token, 'X-Wordclock-Action': 'web-update',
            'Content-Type': mime, 'Connection': 'close'})
        with urllib.request.urlopen(req, timeout=20) as response:
            return json.load(response)
    if a.rollback:
        if input('Restore previous interface? Type SWITCH: ') != 'SWITCH': return 1
        print(request('rollback?confirm=SWITCH', 'POST', b''))
        return 0
    if a.activate and input('Activate verified interface? Type SWITCH: ') != 'SWITCH': return 1
    raw = a.manifest.read_bytes()
    manifest = json.loads(raw)
    assets = []
    for item in manifest['files']:
        name = item['path']
        if Path(name).name != name or '/' in name or '\\' in name:
            raise ValueError('Invalid asset name')
        data = (a.source / name).read_bytes()
        if len(data) != item['size'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError(f'{name}: regenerate manifest')
        assets.append((name, data))
    boundary = uuid.uuid4().hex
    body = (f'--{boundary}\r\nContent-Disposition: form-data; name="manifest"; filename="web-manifest.json"\r\nContent-Type: application/json\r\n\r\n'.encode() + raw + f'\r\n--{boundary}--\r\n'.encode())
    session = None
    success = False
    try:
        result = request('start', 'POST', body, 'multipart/form-data; boundary=' + boundary)
        session = result['id']
        print(f'Session {session}, target {result["target"]}')
        def expect_rejection(route, method, body, code, error):
            try:
                request(route, method, body)
            except urllib.error.HTTPError as exc:
                result = json.load(exc)
                if exc.code == code and result.get('error') == error:
                    print(f'PASS: HTTP {code}, {error}')
                    return
                raise ValueError(f'Unexpected rejection: {exc.code} {result}') from exc
            raise ValueError('Unexpected acceptance')
        verify_route = 'verify?' + urllib.parse.urlencode(dict(id=session))
        if a.test == 'incomplete':
            expect_rejection(verify_route, 'POST', b'', 409, 'incomplete')
        if a.test == 'unknown':
            expect_rejection('chunk?' + urllib.parse.urlencode(dict(id=session,name='unknown.js',offset=0)), 'PUT', b'x', 409, 'chunk-rejected')
        for name, data in ([] if a.test in ('incomplete','unknown') else assets):
            if a.test == 'corrupt' and name == assets[0][0]:
                data = bytes([data[0] ^ 1]) + data[1:]
            for offset in range(0, len(data), 512):
                chunk = data[offset:offset+512]
                query = urllib.parse.urlencode(dict(id=session, name=name, offset=offset))
                result = request('chunk?' + query, 'PUT', chunk)
                if result['offset'] != offset + len(chunk):
                    raise ValueError('Offset mismatch')
                print(f'{name}: {offset+len(chunk)}/{len(data)}', flush=True)
        state = request('status')
        print(json.dumps(state, indent=2))
        if state['id'] != session:
            raise ValueError('Session mismatch')
        if a.test not in ('incomplete','unknown'):
            if not state['complete']:
                raise ValueError('Transfer incomplete')
            if a.test == 'corrupt':
                expect_rejection(verify_route, 'POST', b'', 422, 'sha256-mismatch')
                if request('status')['verified']:
                    raise ValueError('Corrupt session marked verified')
            else:
                result = request(verify_route, 'POST', b'')
                state = request('status')
                if not result['verified'] or not state['verified'] or state['verifiedFiles'] != len(assets):
                    raise ValueError('Verification incomplete')
                print('SHA-256 verified: all assets.')
                if a.activate:
                    result = request('activate?' + urllib.parse.urlencode(dict(id=session,confirm='SWITCH')), 'POST', b'')
                    if not result.get('activated'): raise ValueError('Activation not confirmed')
                    session = None
                    print(result)
                else:
                    print('NOT activated.')
        success = True
    except (Exception, KeyboardInterrupt) as exc:
        print(f'Transfer stopped: {exc}')
    finally:
        if session and not (success and a.keep):
            try:
                print(request('cancel?' + urllib.parse.urlencode(dict(id=session)), 'POST', b''))
            except Exception as exc:
                print(f'Cancellation not confirmed: {exc}; device session expires automatically.')
                success = False
    return 0 if success else 1


if __name__ == '__main__':
    raise SystemExit(main())
