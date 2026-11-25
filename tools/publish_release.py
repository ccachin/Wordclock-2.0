"""Stage verified web-only releases on a dedicated GitHub Pages checkout.
Firmware publication is deliberately disabled until credentials are device-specific.
"""
import argparse
import json
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from prepare_release import verify, versions, ROOT, descriptor


def git(folder, *args):
    return subprocess.check_output(['git', '-C', str(folder), *args], text=True).strip()


ATTRIBUTES = '/releases/** -text -filter -ident -working-tree-encoding\n/catalog.json -text -filter -ident -working-tree-encoding\n'


def protect(pages):
    path = pages / '.gitattributes'
    old = path.read_text(encoding='utf-8') if path.exists() else ''
    if not old.endswith(ATTRIBUTES):
        path.write_text(old.rstrip() + ('\n' if old.strip() else '') + ATTRIBUTES, encoding='utf-8')


def verify_index(pages):
    def blob(path):
        return subprocess.check_output(['git', '-C', str(pages), 'show', ':' + path])
    def digest(data):
        return hashlib.sha256(data).hexdigest()
    catalog = json.loads(blob('catalog.json'))
    if catalog['format'] != 'wordclock-release-catalog' or catalog['version'] != 1:
        raise ValueError('Invalid indexed catalog')
    for entry in catalog['releases']:
        root = 'releases/' + entry['version'] + '/'
        if entry['manifest'] != root + 'release.json':
            raise ValueError('Invalid manifest path')
        raw = blob(entry['manifest'])
        if digest(raw) != entry['sha256']:
            raise ValueError('Indexed release hash mismatch: ' + root)
        meta = json.loads(raw)
        for key, name in {'web':'web.zip','webManifest':'web-manifest.json','notes':'CHANGELOG.md'}.items():
            item = meta['artifacts'][key]
            data = blob(root + name)
            if item['path'] != name or len(data) != item['size'] or digest(data) != item['sha256']:
                raise ValueError('Indexed artifact mismatch: ' + root + name)
    print('All indexed release artifacts verified.')


def stage(source, pages):
    meta = verify(source)
    version = meta['version']
    if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-dev(?:\.\d+)?)?', version):
        raise ValueError('Invalid release version')
    channel = meta['channel']
    if channel not in ('development', 'stable') or (channel == 'development') != ('-dev' in version):
        raise ValueError('Invalid channel')
    if channel == 'stable':
        raise ValueError('Stable publication disabled until security validation')
    if meta['compatibility'] != {'board': 'd1', 'webApiVersion': 1}:
        raise ValueError('Unsupported compatibility')
    names = {'web': 'web.zip', 'webManifest': 'web-manifest.json', 'notes': 'CHANGELOG.md'}
    for key, name in names.items():
        if meta['artifacts'][key] != descriptor(source / name):
            raise ValueError('Unexpected descriptor: ' + key)
    web = json.loads((source / 'web-manifest.json').read_text())
    if web['version'] != meta['webVersion'] or web['compatibility']['webApiVersion'] != 1:
        raise ValueError('Web manifest mismatch')
    catalog_path = pages / 'catalog.json'
    catalog = json.loads(catalog_path.read_text()) if catalog_path.exists() else {'format': 'wordclock-release-catalog', 'version': 1, 'releases': []}
    if catalog['format'] != 'wordclock-release-catalog' or catalog['version'] != 1:
        raise ValueError('Invalid catalog')
    target = pages / 'releases' / version
    if target.exists() or any(r['version'] == version for r in catalog['releases']):
        raise ValueError('Release already exists; use a new version')
    public = dict(meta)
    public['artifacts'] = {key: meta['artifacts'][key] for key in names}
    public['firmwareVersion'] = None
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=target.parent) as tmp:
        staged = Path(tmp) / 'release'
        staged.mkdir()
        for name in names.values():
            shutil.copyfile(source / name, staged / name)
        (staged / 'release.json').write_text(json.dumps(public, indent=2)+'\n', encoding='utf-8')
        entry = {'version': version, 'channel': channel, 'manifest': f'releases/{version}/release.json', 'sha256': descriptor(staged / 'release.json')['sha256']}
        catalog['releases'].append(entry)
        staged.rename(target)
    temporary = pages / 'catalog.json.tmp'
    temporary.write_text(json.dumps(catalog, indent=2)+'\n', encoding='utf-8')
    temporary.replace(catalog_path)
    (pages / '.nojekyll').touch()
    if not (pages / 'index.html').exists():
        (pages / 'index.html').write_text('<!doctype html><meta charset="utf-8"><title>Wordclock releases</title><h1>Wordclock releases</h1><p><a href="catalog.json">Release catalog</a></p>', encoding='utf-8')
    return target


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pages', required=True, type=Path)
    p.add_argument('--verify-index', action='store_true', help='Verify exact staged Git bytes before commit')
    p.add_argument('--release', type=Path, default=ROOT/'dist'/versions()['RELEASE_VERSION'])
    args = p.parse_args()
    pages = args.pages.resolve()
    if git(pages, 'branch', '--show-current') != 'gh-pages':
        raise ValueError('Use a gh-pages checkout')
    if args.verify_index:
        verify_index(pages)
        return
    if git(pages, 'status', '--porcelain'):
        raise ValueError('Use a clean gh-pages checkout')
    if pages == ROOT or ROOT in pages.parents:
        raise ValueError('Use a separate checkout outside source tree')
    print('Web-only staging; firmware.bin will NOT be copied. No push is performed.')
    if input('Type PUBLISH to stage: ') != 'PUBLISH':
        return
    print('Prepared:', stage(args.release.resolve(), pages))
    protect(pages)
    subprocess.run(['git', '-C', str(pages), 'add', '--', '.gitattributes', '.nojekyll', 'index.html', 'catalog.json', 'releases'], check=True)
    verify_index(pages)
    print('Review git status, then commit and push gh-pages manually.')

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as e:
        raise SystemExit(f'ERROR: {e}')
