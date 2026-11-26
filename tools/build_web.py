"""Build a reduced, backward-compatible web release without changing source files."""
from pathlib import Path
import re
import gzip
import shutil

BUNDLE = ('app.js', 'night.js', 'theme-rules.js', 'studio-visual.js', 'studio.js', 'setup.js', 'maintenance.js')

def build(source: Path, target: Path, version: str):
    target.mkdir(parents=True, exist_ok=True)
    for file in source.iterdir():
        shutil.copyfile(file, target / file.name)
    html = (source / 'index.html').read_text(encoding='utf-8')
    scripts = re.findall(r'<script src="/([^"?]+)" defer>\s*</script>', html)
    if tuple(scripts) != BUNDLE:
        raise ValueError(f'Unexpected script order: {scripts}')
    # Keep each original name in the strict API-1 manifest for old firmware.
    # Only app.js is requested by the browser in a packaged release.
    bundle = '\n;\n'.join((source / name).read_text(encoding='utf-8') for name in BUNDLE)
    (target / 'app.js').write_text(bundle, encoding='utf-8')
    for name in BUNDLE[1:]:
        (target / name).write_text('// Bundled into app.js.\n', encoding='utf-8')
    # Reserved manifest entries hold optional gzip representations. Older firmware
    # continues to serve the uncompressed originals requested by index.html.
    for plain, compressed in [('app.js', 'community.js'), ('style.css', 'night.js')]:
        (target / compressed).write_bytes(gzip.compress((target / plain).read_bytes(), compresslevel=9, mtime=0))
    html = re.sub(r'<script src="/([^"?]+)" defer>\s*</script>', lambda m: f'<script src="/app.js?v={version}" defer></script>' if m[1]=='app.js' else '', html)
    html = html.replace('href="/style.css"', f'href="/style.css?v={version}"').replace('href="/icon.svg"', f'href="/icon.svg?v={version}"').replace('href="/manifest.json"', f'href="/manifest.json?v={version}"')
    (target / 'index.html').write_text(html, encoding='utf-8')
