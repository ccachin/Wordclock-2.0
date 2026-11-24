
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include <ESP8266WiFi.h>

#include "ota_manager.h"
#include "ota_config.h"
#include "wifi_manager.h"
#include "settings_manager.h"
#include "storage.h"
#include "leds.h"
#include "config.h"

namespace OTAManager
{
    static bool started = false;
    static bool updating = false;
    static bool configured = false;
    static bool waitingForStorage = false;
    static bool recoveryPending = false;
    static unsigned int lastProgress = 101;

    // Preserve the rendered frame so an interrupted update can restore it
    // even when RenderManager does not need to draw a new frame.
    static CRGB savedFrame[Config::TOTAL_LED_COUNT];
    static uint8_t savedBrightness = 0;
    static bool frameSaved = false;
    static uint8_t displayedSteps = 255;

    static void showProgress(uint8_t steps)
    {
        if (steps == displayedSteps) return;
        displayedSteps = steps;
        Leds::clear();
        Leds::setBrightness(32);
        for (uint8_t i = 0; i < 4; ++i)
        {
            Leds::setPixel(Config::MINUTE_LEDS[i],
                           i < steps ? CRGB(0, 160, 255) : CRGB(0, 0, 35));
        }
        Leds::show();
    }

    static void restoreFrame()
    {
        if (!frameSaved) return;
        for (uint16_t i = 0; i < Config::TOTAL_LED_COUNT; ++i)
            Leds::setPixel(i, savedFrame[i]);
        Leds::setBrightness(savedBrightness);
        Leds::show();
        frameSaved = false;
    }

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

            // Settings must already be persisted before ArduinoOTA.handle().
            Serial.println("OTA: settings already saved");

            savedBrightness = FastLED.getBrightness();
            for (uint16_t i = 0; i < Config::TOTAL_LED_COUNT; ++i)
                savedFrame[i] = Leds::getPixel(i);
            frameSaved = true;
            displayedSteps = 255;
            showProgress(0); });

        // ==================================================
        // Update completed
        // ==================================================

        ArduinoOTA.onEnd([]()
                         {
            showProgress(4);
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

                // The fourth indicator is reserved for successful validation.
                showProgress(static_cast<uint8_t>(min(percentage / 25U, 3U)));

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

            // Keep the existing emergency restart for receive errors:
            // ArduinoOTA may continue its receive loop after this callback.
            if (error == OTA_RECEIVE_ERROR && updating)
            {
                Serial.println("OTA receive recovery: restarting");
                Serial.flush();
                delay(100);
                ESP.restart();
                return;
            }

            // Authentication errors do not involve Updater.
            // Recover outside the library callback for other errors.
            if (error != OTA_AUTH_ERROR)
            {
                recoveryPending = true;
            } });

        configured = true;
    }

    // ==================================================
    // OTA service
    // ==================================================

    void stop()
    {
        if (started) {
            ArduinoOTA.end();
            started = false;
            Serial.println("OTA stopped");
        }
    }

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
            stop();

            return;
        }

        // ArduinoOTA runs the transfer synchronously from handle().
        // Let the normal loop persist pending settings before serving OTA.
        if (!Storage::isReady() ||
            SettingsManager::savePending() ||
            SettingsManager::saveFailed())
        {
            if (!waitingForStorage)
            {
                Serial.println("OTA waiting: settings or storage not ready");
                waitingForStorage = true;
            }
            return;
        }

        if (waitingForStorage)
        {
            Serial.println("OTA available: settings and storage ready");
            waitingForStorage = false;
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

        if (recoveryPending)
        {
            recoveryPending = false;
            Serial.printf("OTA recovery: updater error %u, running=%u\n",
                          static_cast<unsigned>(Update.getError()),
                          static_cast<unsigned>(Update.isRunning()));
            if (Update.hasError())
            {
                Update.printError(Serial);
            }

            // Do not call Update.end() again: a complete image could be
            // scheduled for installation. Restart if state is uncertain.
            if (Update.isRunning())
            {
                Serial.println("OTA recovery: updater still active, restarting");
                Serial.flush();
                ESP.restart();
                return;
            }

            restoreFrame();
            updating = false;
            Update.clearError();
            stop();
            Serial.println("OTA recovery: normal operation resumed");
            // The next loop starts the listener again when safe.
        }
    }

    // ==================================================
    // Update status
    // ==================================================

    bool isUpdating()
    {
        return updating;
    }
}
