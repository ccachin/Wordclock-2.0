"""Stage verified web-only releases on a dedicated GitHub Pages checkout.
Firmware publication is deliberately disabled until credentials are device-specific.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from prepare_release import verify, versions, ROOT, descriptor


def git(folder, *args):
    return subprocess.check_output(['git', '-C', str(folder), *args], text=True).strip()


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
    p.add_argument('--release', type=Path, default=ROOT/'dist'/versions()['RELEASE_VERSION'])
    args = p.parse_args()
    pages = args.pages.resolve()
    if git(pages, 'branch', '--show-current') != 'gh-pages' or git(pages, 'status', '--porcelain'):
        raise ValueError('Use a clean gh-pages checkout')
    if pages == ROOT or ROOT in pages.parents:
        raise ValueError('Use a separate checkout outside source tree')
    print('Web-only staging; firmware.bin will NOT be copied. No push is performed.')
    if input('Type PUBLISH to stage: ') != 'PUBLISH':
        return
    print('Staged:', stage(args.release.resolve(), pages))
    print('Review git status, then commit and push gh-pages manually.')

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as e:
        raise SystemExit(f'ERROR: {e}')
