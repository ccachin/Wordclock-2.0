"""Generate a Wordclock web release manifest without modifying web sources."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile

ROOT = Path(__file__).resolve().parent.parent
REQUIRED = frozenset(('style.css', 'app.js', 'night.js', 'theme-rules.js',
    'studio-visual.js', 'studio.js', 'community.js', 'setup.js',
    'maintenance.js', 'icon.svg', 'manifest.json', 'index.html'))
MAX_FILES = 32
MAX_FILE = 128 * 1024
MAX_TOTAL = 384 * 1024
MAX_MANIFEST = 4 * 1024


def generate(source, version, api):
    if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', version) or len(version) > 32:
        raise ValueError('Version must be a numeric X.Y.Z (max 32 characters)')
    if not 1 <= api <= 65535:
        raise ValueError('API version must be between 1 and 65535')
    files = []
    total = 0
    names = set()
    for path in sorted(source.iterdir(), key=lambda p: p.name):
        name = path.name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f'Not a regular file: {name}')
        if not re.fullmatch(r'[a-z0-9][a-z0-9._-]{0,63}', name) or '..' in name or name == 'web-manifest.json' or name.endswith(('.tmp', '.bak')):
            raise ValueError(f'Invalid or reserved filename: {name}')
        if name not in REQUIRED:
            raise ValueError(f'File not supported by current web server: {name}')
        if name in names:
            raise ValueError(f'Duplicate filename: {name}')
        names.add(name)
        if len(names) > MAX_FILES:
            raise ValueError('Too many files')
        with path.open('rb') as stream:
            data = stream.read(MAX_FILE + 1)
        if not 0 < len(data) <= MAX_FILE:
            raise ValueError(f'Invalid file size: {name} ({len(data)} bytes)')
        total += len(data)
        if total > MAX_TOTAL:
            raise ValueError('Total size exceeds limit')
        files.append({'path': name, 'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    if REQUIRED - names:
        raise ValueError('Missing required files: ' + ', '.join(sorted(REQUIRED - names)))
    manifest = {'format': 'wordclock-web', 'manifestVersion': 1, 'version': version,
                'compatibility': {'webApiVersion': api}, 'fileCount': len(files),
                'totalBytes': total, 'files': files}
    payload = (json.dumps(manifest, indent=2, ensure_ascii=True) + '\n').encode('ascii')
    if len(payload) > MAX_MANIFEST:
        raise ValueError('Manifest exceeds size limit')
    return manifest, payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', required=True)
    parser.add_argument('--api-version', type=int, default=1)
    parser.add_argument('--source', type=Path, default=ROOT / 'data' / 'www')
    parser.add_argument('--output', type=Path, default=ROOT / 'dist' / 'web-manifest.json')
    args = parser.parse_args()
    temporary = None
    try:
        source = args.source.resolve()
        output = args.output.resolve()
        if output == source or source in output.parents:
            raise ValueError('Output must be outside web source directory')
        if output.name != 'web-manifest.json':
            raise ValueError('Output filename must be web-manifest.json')
        manifest, payload = generate(source, args.version, args.api_version)
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(payload)
        os.replace(temporary, output)
        print(f'Web version: {args.version}\nAPI version: {args.api_version}')
        for item in manifest['files']:
            print(f"{item['path']}: {item['size']} bytes")
        print(f"Files: {manifest['fileCount']}\nTotal: {manifest['totalBytes']} bytes")
        print(f'Manifest: {output} ({len(payload)} bytes)')
        return 0
    except (OSError, ValueError) as error:
        print(f'ERROR: {error}', file=__import__('sys').stderr)
        return 1
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    raise SystemExit(main())
