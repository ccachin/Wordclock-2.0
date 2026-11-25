"""Build and package a Wordclock release (never uploads or publishes)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile
from generate_web_manifest import ROOT, generate


def versions():
    text = (ROOT / 'include/release_version.h').read_text()
    def get(key):
        m = re.search(r'^#define WORDCLOCK_' + key + r'\s+(?:"([^"\n]+)"|(\d+))\s*$', text, re.M)
        if not m:
            raise ValueError('Missing version: ' + key)
        return m.group(1) or m.group(2)
    result = {k: get(k) for k in ('FIRMWARE_VERSION', 'WEB_VERSION', 'RELEASE_VERSION', 'WEB_API_VERSION', 'RELEASE_CHANNEL')}
    for k in ('FIRMWARE_VERSION', 'RELEASE_VERSION'):
        if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-dev(?:\.\d+)?)?', result[k]):
            raise ValueError('Invalid ' + k)
    if result['RELEASE_CHANNEL'] not in ('development', 'stable'):
        raise ValueError('Invalid release channel')
    if result['RELEASE_CHANNEL'] == 'stable' and any('-' in result[k] for k in ('FIRMWARE_VERSION', 'RELEASE_VERSION')):
        raise ValueError('Stable channel requires stable versions')
    if result['RELEASE_CHANNEL'] == 'development' and '-dev' not in result['RELEASE_VERSION']:
        raise ValueError('Development release requires -dev suffix')
    return result


def descriptor(path):
    data = path.read_bytes()
    return {'path': path.name, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def verify(folder):
    meta = json.loads((folder / 'release.json').read_text())
    for item in meta['artifacts'].values():
        if descriptor(folder / item['path']) != item:
            raise ValueError('Artifact mismatch: ' + item['path'])
    web = json.loads((folder / 'web-manifest.json').read_text())
    with zipfile.ZipFile(folder / 'web.zip') as z:
        if sorted(z.namelist()) != sorted(x['path'] for x in web['files']):
            raise ValueError('ZIP file list mismatch')
        for item in web['files']:
            data = z.read(item['path'])
            if len(data) != item['size'] or hashlib.sha256(data).hexdigest() != item['sha256']:
                raise ValueError('Web asset mismatch: ' + item['path'])
    return meta


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--verify', type=Path, help='Verify an existing release folder instead of building')
    args = p.parse_args()
    if args.verify:
        verify(args.verify)
        print('Release verified')
        return
    v = versions()
    dest = ROOT / 'dist' / v['RELEASE_VERSION']
    if dest.exists():
        raise ValueError(f'Output already exists: {dest}; move it before rebuilding')
    subprocess.run(['pio', 'run', '-e', 'd1', '-t', 'clean'], cwd=ROOT, check=True)
    subprocess.run(['pio', 'run', '-e', 'd1'], cwd=ROOT, check=True)
    firmware = ROOT / '.pio/build/d1/firmware.bin'
    dest.parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dest.parent) as tmp:
        folder = Path(tmp) / 'release'
        folder.mkdir()
        shutil.copyfile(firmware, folder / 'firmware.bin')
        if not 0 < firmware.stat().st_size <= 1044464 or firmware.open('rb').read(1) != b'\xe9':
            raise ValueError('Unexpected ESP8266 firmware image')
        source = ROOT / 'data/www'
        manifest, raw = generate(source, v['WEB_VERSION'], int(v['WEB_API_VERSION']))
        (folder / 'web-manifest.json').write_bytes(raw)
        with zipfile.ZipFile(folder / 'web.zip', 'w', zipfile.ZIP_DEFLATED) as z:
            for item in manifest['files']:
                z.write(source / item['path'], item['path'])
        shutil.copyfile(ROOT / 'CHANGELOG.md', folder / 'CHANGELOG.md')
        meta = {'format': 'wordclock-release', 'manifestVersion': 1,
                'version': v['RELEASE_VERSION'], 'channel': v['RELEASE_CHANNEL'],
                'firmwareVersion': v['FIRMWARE_VERSION'], 'webVersion': v['WEB_VERSION'],
                'compatibility': {'board': 'd1', 'webApiVersion': int(v['WEB_API_VERSION'])},
                'artifacts': {k: descriptor(folder / name) for k, name in [('firmware','firmware.bin'), ('web','web.zip'), ('webManifest','web-manifest.json'), ('notes','CHANGELOG.md')]}}
        (folder / 'release.json').write_text(json.dumps(meta, indent=2) + '\n', encoding='utf-8')
        verify(folder)
        folder.rename(dest)
    print(f'Release verified: {dest}')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError, zipfile.BadZipFile) as e:
        raise SystemExit(f'ERROR: {e}')
