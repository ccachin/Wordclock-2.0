# 3.6.0-dev — memory optimization, first pass

Baseline: audited 3.5.2 ELF, `.data + .rodata + .bss = 58,472` bytes. `.noinit` is separate. No target measurement is claimed until compilation. The 2,863-byte portal strings are moved to flash; 768 bytes of web hashing buffer are removed; nine factory JSON strings move to flash. Additional allocations, alignment and compiler pooling affect the actual gain.

Portal: `setCustomHeadElement` retains a pointer. Allocate native-RAM copies only while the portal is active, restore the regular head before freeing the save head, clear pointer before freeing on exit. Low memory falls back to the native page. Verify first setup, reconnect, wrong password, timeout, and successful handoff on hardware.

Factory themes: typed values and public v2 JSON use one macro table; v3 catalogue conversion unchanged. The native test compares all nine exact JSON entries with 3.5.2. The embedded UI requests `factoryOnly=1` to avoid loading hidden personal themes; the default API still returns all themes. A currently selected personal theme is shown as Personnalisé. Personal themes and persistent formats are untouched.

Web: reserve JSON response once; return 503 on overflow/serialization failure; smaller SHA-256 verification buffer without changing chunk transfer protocol, hash, signing, or thresholds. Gzip parsing retains the prior exact-token behavior. System info samples memory once per second at the server update and during `/api/system`; minima are not true allocation peaks. The initial memory margin is indicative, not a promise that RSA or transfer will succeed.

Weather: reuse request/header stack buffer, parse bounded response in-place, reject unsupported content encoding. No change to HTTP transport, memory guards or refresh behavior. Diagnostic build is opt-in; normal automatic weather is disabled in that environment only.

Legacy cleanup: `python tools/cleanup_legacy.py` dry-run searches the actual working Git tree and checks audited hashes; `--apply` stages the removal. No removal on ZIP extraction. Keep web manifest placeholders, signing, recovery and studio legacy files. Do not publish the diagnostic build.

Validation: first compile normal d1 and run memory_report.py. Verify web package, install experimentally (USB if needed), run live validator, check weather, factory themes, animations, automation, night mode, setup portal and signed update. The upgrade from 3.5.2 can still fail because it starts on old firmware. A later signed upgrade from 3.6.0 is required to establish OTA reliability. Keep existing safety thresholds.
