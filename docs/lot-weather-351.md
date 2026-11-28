# 3.5.1 — Weather transport and location search

The firmware still stores integer coordinates in units of 1e-5 degrees. The browser queries the Open-Meteo geocoding HTTPS endpoint on explicit search, displays disambiguated results, fills the coordinate inputs, and requires explicit save. The locality label is browser-local only; another browser still sees the saved coordinates. Manual entry remains available.

The weather transport uses WiFiClient and HTTP/1.0 with Connection: close and identity encoding. The parser accepts bounded Content-Length or connection-close bodies, rejects transfer encoding, and caps the body at 1535 bytes and headers at 2048 bytes. The observed HTTP/1.0 server response has no Transfer-Encoding. HTTP status, reception, JSON, fields, timestamp, forecast and range failures are distinguished. Serial log records duration and heap before/after the attempt. The existing memory guards remain unchanged. This is synchronous; DNS, connection and writes are not covered by the read-loop deadline. No authenticated weather transport is provided.

Validation required on ESP8266: build, signed update, city lookup in browser, saved coordinates, current and forecast responses, memory recovery, physical appearance. Native palette/expiry tests do not validate the HTTP transport.
