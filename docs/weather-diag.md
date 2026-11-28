# 3.5.0-test1 (USB only)

Default d1 firmware and the installed web package are unchanged. The diagnostic is
compiled only with d1_weather_diag. It deliberately keeps the installed firmware
version metadata, and identifies itself on serial. Never publish its binary.

Start with B (budget). No network or settings writes. The guard uses actual
Xtensa sizeof values, 6200-byte secondary stack (if not already allocated),
512+325 and 512+85 buffers, and a conservative 3072-byte allowance for CA,
allocators, network and normal runtime. Not a proof of safe execution if allowed.
If SKIP_LOW_MEMORY, stop. Do not bypass the guard.

Optional: provision one CA with tools/weather_diag_ca.py, using the PC trust
store and a hostname-verified TLS 1.2 connection. W then runs a single request;
T runs up to ten, stopping on failure, with 15 seconds between requests. X
cancels pending attempts. No TLS verification bypass. Fixed public sample
coordinates, no personal location. No writes to LittleFS or settings.

TLS operations are synchronous, so LEDs and HTTP handling may pause; elapsed
pause is logged, but memory during the handshake is not directly sampled. If a
reset occurs, there is no automatic retry. Observe clock physically. Restore
using normal d1 USB upload, never uploadfs. Diagnostic is not signed/released.
