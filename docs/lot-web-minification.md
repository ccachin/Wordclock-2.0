# 3.6.3-dev — web packaging

Install the pinned build dependencies once using `npm install --prefix tools` (commit the resulting `tools/package-lock.json`; ignore `tools/node_modules/`). For repeatable future installs use `npm ci --prefix tools` after the lock file is committed.

`python tools/prepare_release.py --web-only` bundles JS, minifies packaged JS/CSS, regenerates gzip, and builds the existing API-1 manifest. Sources remain untouched. No firmware changes. Missing dependencies or a changed parsed syntax tree fail the build; there is no silent unminified fallback.

JS: Acorn checks equivalent parsed syntax trees, retaining original literal token text, line terminators (including ASI), and license/preserve comments. No identifier mangling or expression rewriting. CSS: PostCSS removes formatting around nodes and verifies equivalent parsed trees; selectors, declaration values, custom property separators, and comments are retained. These checks are not substitutes for browser and device tests.

Measured on reconstructed 3.6.2 sources: original packaged JS 99,411 -> 88,887 bytes; CSS 26,582 -> 24,482 bytes; total payload including gzip and other manifest assets 199,325 -> 184,250 bytes. On the reported 401,408 free bytes, the unchanged 204,800-byte reserves yield an estimated 12,358-byte preflight margin. Actual filesystem allocation and live update still require validation. No deletion or formatting of LittleFS is necessary.

Offline validation used locally installed exact Acorn/PostCSS versions; npm registry access was unavailable in the build environment. The Windows installation and browser/device tests remain to be performed. Storage reclamation for the inactive slot remains a separate firmware task.
