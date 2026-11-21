
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

#include "ota_manager.h"
#include "ota_config.h"
#include "wifi_manager.h"
#include "settings_manager.h"
#include "leds.h"

namespace OTAManager
{
    static bool started = false;
    static bool updating = false;
    static bool configured = false;
    static unsigned int lastProgress = 101;

    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {
        if (!OTAConfig::ENABLED)
        {
            Serial.println("OTA disabled");
            return;
        }

        if (
            strlen(OTAConfig::PASSWORD) < 8 ||
            strcmp(OTAConfig::PASSWORD, "CHANGE_ME") == 0)
        {
            Serial.println(
                "OTA disabled: configure a password");
            return;
        }

        ArduinoOTA.setHostname(
            OTAConfig::HOSTNAME);

        ArduinoOTA.setPort(
            OTAConfig::PORT);

        ArduinoOTA.setPassword(
            OTAConfig::PASSWORD);

        ArduinoOTA.setRebootOnSuccess(true);

        // ==================================================
        // Update start
        // ==================================================

        ArduinoOTA.onStart([]()
                           {
            updating = true;
            lastProgress = 101;

            Serial.println();
            Serial.println("OTA update started");

            SettingsManager::save();

            Leds::clear();
            Leds::show(); });

        // ==================================================
        // Update completed
        // ==================================================

        ArduinoOTA.onEnd([]()
                         {
            Serial.println();
            Serial.println(
                "OTA completed, restarting..."); });

        // ==================================================
        // Update progress
        // ==================================================

        ArduinoOTA.onProgress(
            [](unsigned int progress,
               unsigned int total)
            {
                if (total == 0)
                {
                    return;
                }

                unsigned int percentage =
                    (progress * 100UL) / total;

                if (
                    percentage != lastProgress &&
                    (percentage % 10 == 0 ||
                     percentage == 100))
                {
                    lastProgress = percentage;

                    Serial.printf(
                        "OTA progress: %u%%\n",
                        percentage);
                }
            });

        // ==================================================
        // Error handling
        // ==================================================

        ArduinoOTA.onError([](ota_error_t error)
                           {
            Serial.printf(
                "OTA error [%u]: ",
                error);

            switch (error)
            {
                case OTA_AUTH_ERROR:
                    Serial.println(
                        "Authentication failed");
                    break;

                case OTA_BEGIN_ERROR:
                    Serial.println(
                        "Begin failed");
                    break;

                case OTA_CONNECT_ERROR:
                    Serial.println(
                        "Connection failed");
                    break;

                case OTA_RECEIVE_ERROR:
                    Serial.println(
                        "Receive failed");
                    break;

                case OTA_END_ERROR:
                    Serial.println(
                        "End failed");
                    break;
            }

            // Restore normal operation after
            // a failed update.

            if (updating)
            {
                delay(100);
                ESP.restart();
            }

            updating = false; });

        configured = true;
    }

    // ==================================================
    // OTA service
    // ==================================================

    void update()
    {
        if (!configured)
        {
            return;
        }

        bool available =
            WifiManager::isConnected() &&
            !WifiManager::isPortalActive();

        if (!available)
        {
            if (started)
            {
                ArduinoOTA.end();

                started = false;

                Serial.println("OTA stopped");
            }

            return;
        }

        if (!started)
        {
            // Start OTA without initializing mDNS.
            // The web server already manages mDNS.

            ArduinoOTA.begin(false);

            started = true;

            Serial.println();
            Serial.println("OTA ready");

            Serial.print("OTA IP: ");
            Serial.println(WiFi.localIP());

            Serial.printf(
                "OTA port: %u\n",
                OTAConfig::PORT);

            Serial.printf(
                "Current sketch size: %lu bytes\n",
                static_cast<unsigned long>(
                    ESP.getSketchSize()));

            Serial.printf(
                "Free sketch space: %lu bytes\n",
                static_cast<unsigned long>(
                    ESP.getFreeSketchSpace()));
        }

        ArduinoOTA.handle();
    }

    // ==================================================
    // Update status
    // ==================================================

    bool isUpdating()
    {
        return updating;
    }
}
