"""Upload web assets to Wordclock without replacing LittleFS."""
import argparse
import base64
import getpass
import http.client
import json
from pathlib import Path
import time
import urllib.error
import urllib.parse
import urllib.request

FILES = ('style.css', 'app.js', 'night.js', 'index.html')
ROOT = Path(__file__).resolve().parent.parent / 'data' / 'www'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='http://wordclock.local')
    parser.add_argument('--force', action='store_true')
    args = parser.parse_args()
    host = args.host.rstrip('/')
    if '://' not in host:
        host = 'http://' + host
    print(f'Wordclock Web Updater\nDestination: {host}\nSource: {ROOT}\n')
    token = base64.b64encode(('admin:' + getpass.getpass('Web update password: ')).encode()).decode()
    def request(path, data=None):
        headers = {'Authorization': 'Basic ' + token, 'Connection': 'close'}
        if data is not None:
            headers['Content-Type'] = 'text/plain'
        req = urllib.request.Request(host + path, data=data, headers=headers, method='PUT' if data is not None else 'GET')
        for attempt in range(3 if data is None else 1):
            try:
                with urllib.request.urlopen(req, timeout=20) as response:
                    return response.read()
            except urllib.error.HTTPError:
                raise
            except (OSError, http.client.HTTPException):
                if data is not None or attempt == 2:
                    raise
                time.sleep(attempt + 1)
    try:
        info = json.loads(request('/api/dev/web'))
        if not info.get('enabled'):
            raise ValueError('Web updates disabled')
        limit = int(info['maxFileSize'])
        assets = [(name, (ROOT / name).read_bytes()) for name in FILES]
        for name, data in assets:
            if not 0 < len(data) <= limit:
                raise ValueError(f'{name} invalid size ({len(data)} bytes, maximum {limit})')
        for name, data in assets:
            if not args.force:
                try:
                    if request('/' if name == 'index.html' else '/' + name) == data:
                        print(f'{name}: unchanged'); continue
                except urllib.error.HTTPError as exc:
                    if exc.code != 404:
                        raise
            try:
                result = json.loads(request('/api/dev/web?name=' + urllib.parse.quote(name), data))
                if not result.get('ok') or result.get('size') != len(data):
                    raise ValueError(f'Unexpected upload response for {name}: {result}')
            except Exception as exc:
                raise RuntimeError(f'{name}: upload not confirmed; rerun to verify device contents ({exc})') from exc
            time.sleep(0.5)
            if request('/' if name == 'index.html' else '/' + name) != data:
                raise ValueError(f'{name}: verification mismatch')
            print(f'{name}: uploaded and verified ({len(data)} bytes)')
        print('Web update successful.')
        return 0
    except Exception as exc:
        print(f'Error: {exc}\nWeb update failed.')
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
