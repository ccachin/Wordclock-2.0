#pragma once

namespace WebServer
{

    bool maintenancePending();
    bool firmwareSessionActive();
    void begin();
    void update();
    void stop();

}