
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Updater.h>
#include <ESP8266WiFi.h>

#include "ota_manager.h"
#include "firmware_update.h"
#include "web_update_manager.h"
#include "web_server.h"
#include "credentials_manager.h"
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
    // ==================================================
    // Initialization
    // ==================================================

    void begin()
    {
        if (!Credentials::ready())
        {
            Serial.println("OTA disabled");
            return;
        }

        if (
            strlen(Credentials::otaPassword()) < 8 ||
            strcmp(Credentials::otaPassword(), "CHANGE_ME") == 0)
        {
            Serial.println(
                "OTA disabled: configure a password");
            return;
        }

        ArduinoOTA.setHostname(
            Credentials::otaHostname());

        ArduinoOTA.setPort(
            8266);

        ArduinoOTA.setPassword(
            Credentials::otaPassword());

        ArduinoOTA.setRebootOnSuccess(true);

        // ==================================================
        // Update start
        // ==================================================

        ArduinoOTA.onStart([]()
                           {
            updating = true;


            Serial.println();
            Serial.println("OTA update started");

            // Settings must already be persisted before ArduinoOTA.handle().
            Serial.println("OTA: settings already saved");

            if (!FirmwareUpdate::begin(FirmwareUpdate::Owner::ArduinoOTA)) {
                Serial.println("OTA ownership conflict: restarting");
                Serial.flush();
                ESP.restart();
            } });

        // ==================================================
        // Update completed
        // ==================================================

        ArduinoOTA.onEnd([]()
                         {
            FirmwareUpdate::validated(FirmwareUpdate::Owner::ArduinoOTA);
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
                FirmwareUpdate::progress(FirmwareUpdate::Owner::ArduinoOTA, progress, total);
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
        if (!configured || (FirmwareUpdate::active() && !updating))
        {
            return;
        }

        if (WebUpdateManager::active() || WebServer::maintenancePending()) {
            stop();
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
                8266);

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

            FirmwareUpdate::recover(FirmwareUpdate::Owner::ArduinoOTA);
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
        return updating || FirmwareUpdate::active();
    }
}
