#pragma once

#include <Arduino.h>

namespace WifiManager
{

    void begin();
    void update();

    bool isConnected();
    bool isPortalActive();
    bool isOfflineAP();

    void startConfigPortal();
    void forgetNetwork();
    bool clearSavedNetwork();

    String getIPAddress();

}