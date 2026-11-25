"""Stage a web release without activating it. Test transport only."""
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
    a = p.parse_args()
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
    token = base64.b64encode(('admin:' + getpass.getpass('Admin password: ')).encode()).decode()
    base = a.host.rstrip('/') + '/api/system/web-update/'
    def request(route, method='GET', body=None, mime='text/plain'):
        req = urllib.request.Request(base + route, data=body, method=method, headers={
            'Authorization': 'Basic ' + token, 'X-Wordclock-Action': 'web-update',
            'Content-Type': mime, 'Connection': 'close'})
        with urllib.request.urlopen(req, timeout=20) as response:
            return json.load(response)
    boundary = uuid.uuid4().hex
    body = (f'--{boundary}\r\nContent-Disposition: form-data; name="manifest"; filename="web-manifest.json"\r\nContent-Type: application/json\r\n\r\n'.encode() + raw + f'\r\n--{boundary}--\r\n'.encode())
    session = None
    success = False
    try:
        result = request('start', 'POST', body, 'multipart/form-data; boundary=' + boundary)
        session = result['id']
        print(f'Session {session}, target {result["target"]}')
        for name, data in assets:
            for offset in range(0, len(data), 1024):
                chunk = data[offset:offset+1024]
                query = urllib.parse.urlencode(dict(id=session, name=name, offset=offset))
                result = request('chunk?' + query, 'PUT', chunk)
                if result['offset'] != offset + len(chunk):
                    raise ValueError('Offset mismatch')
                print(f'{name}: {offset+len(chunk)}/{len(data)}', flush=True)
        state = request('status')
        print(json.dumps(state, indent=2))
        if not state['complete'] or state['id'] != session:
            raise ValueError('Transfer incomplete')
        success = True
        print('Transfer complete; NOT hash verified or activated on device.')
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
