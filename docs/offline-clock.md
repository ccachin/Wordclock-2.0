# 3.2.0-dev: offline clock

- The device runs its normal HTTP application on its direct AP when no station connection is available. With saved Wi-Fi, fallback starts after about 30 seconds, otherwise immediately. The SSID is `Wordclock-` followed by the chip ID in hex; connect to `http://192.168.4.1/`. This experimental build reuses the existing setup AP passphrase `Wordclock123`. A unique device AP passphrase and secure provisioning remain release blockers. The local settings API is not a public Internet service: do not port-forward it. Existing maintenance authentication remains in place; a device without provisioned credentials can start network setup only on its disconnected direct AP.
- The existing WiFiManager portal remains available via Change network; it is separate from the normal offline UI. It times out after 120 seconds. The AP remains while a client is connected after station reconnection, then closes when the clients leave. Station reconnection and AP channel changes can briefly interrupt the direct connection.
- Phone sync sends a UTC epoch. Manual input is interpreted in the configured clock timezone, rejecting normalized calendar dates and nonexistent spring-forward hours. For ambiguous autumn hours, use phone sync to disambiguate. NTP can supersede a manual setting on an online clock. Source reports the last successful update, not current Internet availability. Time is not persisted as valid after loss of power: no RTC.
- GET `/api/clock` reports validity, local timestamp and source. POST requires the same-origin custom `X-Wordclock-Action: clock` header, with either `epoch` or all six local date/time fields. Like other everyday settings this is not admin-authenticated; maintenance actions are still protected. Clock changes are blocked during firmware/web updates.
- Keep the API-1 manifest and old web slots compatible. Install firmware before the 3.2 web interface. A web-only rollback removes new clock controls but not firmware offline support. USB recovery remains available.

## Validation

`python tools/prepare_release.py` builds and signs the release. `python tools/validate.py` checks the package. `python tools/validate.py --host http://<device-ip>` checks 20 HTTP cycles with clock and Wi-Fi status. Native time tests (Linux GNU toolchain):

```
g++ -std=c++11 -Itest/clock_native -Iinclude src/time_manager.cpp test/clock_native/test.cpp -Wl,--wrap=time -Wl,--wrap=settimeofday -o /tmp/wc-clock-test && /tmp/wc-clock-test
```

On hardware, confirm phone sync, then temporarily make the configured Wi-Fi unavailable, join the device AP and verify that the displayed time advances and the normal web page opens; reconnect the normal network. Do not interrupt power during firmware activation. A cold boot without Wi-Fi requires a new time setting. No RTC or cloud dependence is added.
