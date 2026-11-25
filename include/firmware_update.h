#pragma once

namespace FirmwareUpdate
{
    enum class Owner { None, ArduinoOTA, Web };
    bool begin(Owner requested);
    bool active();
    void progress(Owner requested, unsigned int received, unsigned int total);
    void validated(Owner requested);
    // Caller must establish that Updater is no longer running before recovery.
    void recover(Owner requested);
}
