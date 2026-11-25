#pragma once
namespace Credentials {
    void begin();
    bool ready();
    const char* adminPassword();
    const char* otaPassword();
    const char* otaHostname();
}
