#pragma once

#include <Arduino.h>

namespace WifiManager
{

    void begin();
    void update();

    bool isConnected();
    bool isPortalActive();

    void startConfigPortal();
    void forgetNetwork();

    String getIPAddress();

}