#include "wifi_manager.h"
#include "web_server.h"
#include "ota_manager.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <pgmspace.h>
#include <stdlib.h>

namespace WifiManager
{
    static WiFiManager manager;
    constexpr char AP_NAME[] = "Wordclock-Setup";
    constexpr char AP_PASSWORD[] = "Wordclock123";
    constexpr uint32_t STARTUP_TIMEOUT = 30000;
    constexpr uint32_t RECONNECT_TIMEOUT = 30000;
    constexpr uint32_t PORTAL_RETRY = 15000;

    // Visual styling only. Keep native forms, scripts and routes intact.
    static const char PORTAL_STYLE[] PROGMEM = R"WCSTYLE(<style>
body{margin:0;background:#f5f4f1;color:#242424;font-family:system-ui,-apple-system,sans-serif}
.wrap{display:block;width:100%;max-width:480px;min-width:0;margin:0 auto;padding:24px 20px}
h1{font-size:2rem;letter-spacing:-.06em;margin:12px 0 20px}
h2,h3{font-weight:600}
button,input[type=submit],input[type=button]{background:#242424;color:white;border-radius:12px;min-height:48px;padding:6px 12px;font-size:1rem}
input[type=text],input[type=password],select{background:white;color:#242424;border:1px solid #aaa;border-radius:9px;padding:12px;min-height:44px}
a[data-ssid]{display:inline-block;max-width:75%;overflow-wrap:anywhere;padding:8px 0;color:#242424}
.msg{background:white;border-radius:12px;overflow-wrap:anywhere}
</style>)WCSTYLE";

    // Render the waiting face without requesting another resource from the ESP.
    static const char SAVE_WAIT_STYLE[] PROGMEM = R"WCWAIT(<style>
.msg:not(.wc-managed){text-align:center;font-size:0}
.msg:not(.wc-managed) .wc-face{font-size:clamp(9px,3vw,14px)}
.wc-face{display:grid;grid-template-columns:repeat(11,minmax(0,1fr));aspect-ratio:1;align-items:center;text-align:center;background:#191919;border-radius:9px;padding:12px;margin:0 auto 20px;max-width:310px;color:#555;font:500 clamp(9px,3vw,14px) monospace}
.wc-face .lit{color:#fff5df;text-shadow:0 0 6px #ffe8b688}
.wc-face .star{animation:wcboot 2s steps(1,end) infinite}
@keyframes wcboot{0%,90%,100%{color:#555}25%{color:#fff5df;text-shadow:0 0 6px #ffe8b688}}
@media(prefers-reduced-motion:reduce){.wc-face .star{animation:none;color:#fff5df}}
</style><script>
document.addEventListener('DOMContentLoaded',function(){
 var msg=document.querySelector('.msg');if(!msg)return;
 var face=document.createElement('div');face.className='wc-face';face.setAttribute('aria-hidden','true');
 var cells=('ILAESTBDEUXQUATREDCINQSIXNEUFONZEUNEHSEPTDIXHUITFTROISZUMIDIHEURESMINUITSETFAMOINSOVINGTDIXTLESDEMIQUARTUCINQOWGBSDLO****').split('');
 var lit={};['CONNEXION','AU RESEAU','EN COURS'].forEach(function(line,j){var start=(3+j)*11+Math.floor((11-line.length)/2);line.split('').forEach(function(c,k){if(c!==' '){cells[start+k]=c;lit[start+k]=true;}});});
 for(var i=0;i<cells.length;i++){var el=document.createElement('span');el.textContent=cells[i];if(lit[i])el.className='lit';if(i>=117){el.className='star';el.style.animationDelay=((i-117)*200)+'ms';}face.appendChild(el);}
 msg.textContent='';msg.appendChild(face);
 var note=document.createElement('p');note.style.fontSize='1rem';note.setAttribute('role','status');note.textContent='Connexion au réseau Wi-Fi en cours…';msg.appendChild(note);
 var started=Date.now();
 function load(){if(Date.now()-started>45000){note.textContent='Résultat non confirmé. Vérifiez votre connexion Wi-Fi ou reconnectez-vous à Wordclock-Setup.';return;}var script=document.createElement('script');script.src='/wc-save.js?t='+Date.now();script.onerror=function(){script.remove();setTimeout(load,1000);};document.head.appendChild(script);}
 load();
});
</script>)WCWAIT";

    static const char SAVE_SCRIPT[] PROGMEM = R"WCSCRIPT(
(function () {
  const msg = document.querySelector('.msg');
  if (!msg) return;
  const status = document.createElement('p');
  const detail = document.createElement('p');
  msg.textContent = '';
  const style = document.createElement('style');
  style.textContent = '.wc-face{display:grid;grid-template-columns:repeat(11,minmax(0,1fr));aspect-ratio:1;align-items:center;text-align:center;background:#191919;border-radius:9px;padding:12px;margin:0 auto 20px;max-width:310px;color:#555;font:500 clamp(9px,3vw,14px) monospace}.wc-face .lit{color:#fff5df;text-shadow:0 0 6px #ffe8b688}.wc-face.busy .star{animation:wcstar 2s steps(1,end) infinite}@keyframes wcstar{0%,90%,100%{color:#555;text-shadow:none}20%{color:#fff5df;text-shadow:0 0 6px #ffe8b688}}@keyframes wcstar2{0%,90%,100%{color:#555;text-shadow:none}40%{color:#fff5df;text-shadow:0 0 6px #ffe8b688}}@keyframes wcstar3{0%,90%,100%{color:#555;text-shadow:none}60%{color:#fff5df;text-shadow:0 0 6px #ffe8b688}}@keyframes wcstar4{0%,90%,100%{color:#555;text-shadow:none}80%{color:#fff5df;text-shadow:0 0 6px #ffe8b688}}.wc-face.busy .star:nth-child(119){animation-name:wcstar2}.wc-face.busy .star:nth-child(120){animation-name:wcstar3}.wc-face.busy .star:nth-child(121){animation-name:wcstar4}@media(prefers-reduced-motion:reduce){.wc-face.busy .star{animation:none;color:#fff5df}}';
  document.head.appendChild(style);
  const face = document.createElement('div');
  face.className = 'wc-face';
  face.setAttribute('aria-hidden','true');
  status.setAttribute('role','status');
  msg.append(face, status, detail);
  msg.classList.add('wc-managed');
  function matrix(lines, mode) {
    const rows = ['ILAESTBDEUX','QUATREDCINQ','SIXNEUFONZE','UNEHSEPTDIX','HUITFTROISZ','UMIDIHEURES','MINUITSETFA','MOINSOVINGT','DIXTLESDEMI','QUARTUCINQO','WGBSDLO****'];
    const cells = rows.join('').split('');
    const lit = new Set();
    lines.forEach((line,j) => {
      const start = (4-Math.floor((lines.length-1)/2)+j)*11+Math.floor((11-line.length)/2);
      Array.from(line).forEach((c,k)=>{if(c!==' '){cells[start+k]=c;lit.add(start+k);}});
    });
    face.className = 'wc-face'+(mode==='busy'?' busy':'');
    face.textContent = '';
    cells.forEach((c,i)=>{
      const el=document.createElement('span'); el.textContent=c;
      if(lit.has(i)||(i>=117&&mode==='ready')) el.className='lit';
      if(i>=117&&mode==='busy'){el.className='star';}
      face.appendChild(el);
    });
  }
  matrix(['CONNEXION','AU RESEAU','EN COURS'],'busy');
  status.textContent = 'Connexion en cours...';
  detail.textContent = 'Votre horloge rejoint votre r\u00e9seau Wi-Fi.';
  const started = Date.now();
  function link(label, href) {
    const a = document.createElement('a');
    a.textContent = label; a.href = href;
    a.style.display = 'block'; a.style.marginTop = '16px';
    msg.appendChild(a);
  }
  function waitForApplication(ip, device) {
    const url = 'http://' + ip;
    matrix(['EN ATTENTE','DU RESEAU'],'busy');
    status.textContent = 'Votre horloge est connect\u00e9e.';
    detail.textContent = 'Le r\u00e9seau de configuration va se fermer.';
    const button = document.createElement('button');
    button.type = 'button'; button.disabled = true;
    button.style.width = '100%'; button.style.opacity = '.55';
    button.textContent = 'Connexion au r\u00e9seau domestique...';
    msg.appendChild(button);
    const since = Date.now();
    async function check() {
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 2500);
      try {
        const response = await fetch(url + '/api/handoff-ready', {cache:'no-store', credentials:'omit', signal:controller.signal});
        if (!response.ok) throw new Error('HTTP');
        const data = await response.json();
        if (data.ready !== true || data.device !== device) throw new Error('Not ready');
        matrix(['TOUT','EST PRET'],'ready');
        status.textContent = 'Votre Wordclock est pr\u00eate.';
        detail.textContent = 'Votre appareil peut joindre votre horloge.';
        button.disabled = false; button.style.opacity = '1';
        button.textContent = 'Ouvrir l\u2019application de la Wordclock';
        button.onclick = () => { window.location.href = url + '/?shortcut=1'; };
        const help = document.createElement('details');
        const summary = document.createElement('summary');
        summary.textContent = 'Probl\u00e8me de connexion ?';
        const fallback = document.createElement('p');
        fallback.textContent = 'Ouvrez dans votre navigateur : ' + url + '/';
        help.append(summary, fallback); msg.appendChild(help);
        return;
      } catch (error) {
        if (Date.now()-since >= 20000) {
          detail.textContent = 'Reconnectez votre appareil au r\u00e9seau domestique si n\u00e9cessaire. Si la fen\u00eatre se ferme ou si la v\u00e9rification est bloqu\u00e9e, ouvrez cette adresse dans votre navigateur : ' + url;
        }
      } finally { clearTimeout(timeout); }
      setTimeout(check, 1000);
    }
    check();
  }
  async function poll() {
    if (Date.now() - started > 45000) {
      matrix(['RESULTAT','NON','CONFIRME'],'error');
      status.textContent = 'R\u00e9sultat non confirm\u00e9.';
      detail.textContent = 'V\u00e9rifiez votre r\u00e9seau Wi-Fi. Si Wordclock-Setup est encore disponible, reconnectez-vous pour r\u00e9essayer.';
      link('Retour \u00e0 la configuration', '/wifi');
      return;
    }
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 3000);
    try {
      const response = await fetch('/wc-handoff', {cache:'no-store', signal:controller.signal});
      if (!response.ok) throw new Error('HTTP');
      const data = await response.json();
      if (data.ready && /^\d{1,3}(\.\d{1,3}){3}$/.test(data.ip)) {
        waitForApplication(data.ip, data.device);
        return;
      }
      if (data.failed) {
        matrix(['CONNEXION','IMPOSSIBLE'],'error');
        status.textContent = 'Connexion impossible.';
        detail.textContent = 'V\u00e9rifiez le mot de passe et la disponibilit\u00e9 du r\u00e9seau.';
        link('R\u00e9essayer', '/wifi');
        return;
      }
    } catch (error) { /* The ESP may be busy connecting. */ }
    finally { clearTimeout(timeout); }
    setTimeout(poll, 1000);
  }
  setTimeout(poll, 1000);
})();
)WCSCRIPT";

    // WiFiManager retains a RAM pointer (not a copy); keep it valid until the
    // portal closes. The large source strings live in flash, not DRAM.
    static char* portalHead = nullptr;
    static char* saveHead = nullptr;
    static void restorePortalHead() {
        manager.setCustomHeadElement(portalHead ? portalHead : "");
        free(saveHead);
        saveHead = nullptr;
    }
    static void releasePortalHeads() {
        manager.setCustomHeadElement("");
        free(saveHead); saveHead = nullptr;
        free(portalHead); portalHead = nullptr;
    }
    static void preparePortalHead() {
        releasePortalHeads();
        portalHead = static_cast<char*>(malloc(sizeof(PORTAL_STYLE)));
        if (portalHead) {
            memcpy_P(portalHead, PORTAL_STYLE, sizeof(PORTAL_STYLE));
            manager.setCustomHeadElement(portalHead);
        } else {
            Serial.println(F("Wi-Fi portal: native style (low memory)"));
        }
    }
    static void prepareSaveHead() {
        restorePortalHead();
        constexpr size_t base = sizeof(PORTAL_STYLE)-1;
        saveHead = static_cast<char*>(malloc(base+sizeof(SAVE_WAIT_STYLE)));
        if (!saveHead) {
            Serial.println(F("Wi-Fi portal: native save response (low memory)"));
            return;
        }
        memcpy_P(saveHead, PORTAL_STYLE, base);
        memcpy_P(saveHead+base, SAVE_WAIT_STYLE, sizeof(SAVE_WAIT_STYLE));
        manager.setCustomHeadElement(saveHead);
    }

    // Keep the captive portal available briefly after a verified save.
    static bool savePending = false;
    static bool saveSucceeded = false;
    static bool saveFailed = false;
    static bool finishRequested = false;
    static uint32_t successAt = 0;
    static uint32_t finishAt = 0;
    constexpr uint32_t HANDOFF_TIMEOUT = 15000;

    static bool wasConnected = false;
    static bool everConnected = false;
    static bool needsSetup = false;
    static bool portalAttempted = false;
    static bool manualPortal = false;
    static uint32_t disconnectedSince = 0;
    static uint32_t lastPortalAttempt = 0;

    static bool offlineAP = false;
    bool isOfflineAP() { return offlineAP; }
    static void stopOfflineAP() {
        if (!offlineAP) return;
        WiFi.softAPdisconnect(true);
        offlineAP = false;
    }
    static bool startOfflineAP() {
        if (offlineAP) return true;
        char name[24];
        snprintf(name, sizeof(name), "Wordclock-%06X", static_cast<unsigned>(ESP.getChipId()));
        WiFi.mode(WIFI_AP_STA);
        offlineAP = WiFi.softAP(name, AP_PASSWORD);
        if (!offlineAP) WiFi.mode(WIFI_STA);
        else Serial.println("Offline access: http://192.168.4.1");
        return offlineAP;
    }

    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    bool isPortalActive() { return manager.getConfigPortalActive(); }
    String getIPAddress() { return (isConnected() ? WiFi.localIP() : WiFi.softAPIP()).toString(); }

    static void openPortal(bool manual)
    {
        if (isPortalActive()) return;
        savePending = false;
        saveSucceeded = false;
        saveFailed = false;
        finishRequested = false;
        portalAttempted = true;
        manualPortal = manual;
        // Release application listeners before WiFiManager binds port 80.
        OTAManager::stop();
        WebServer::stop();
        stopOfflineAP();
        Serial.println("Starting Wi-Fi setup portal");
        preparePortalHead();
        manager.startConfigPortal(AP_NAME, AP_PASSWORD);
        lastPortalAttempt = millis();
        if (!isPortalActive())
        {
            releasePortalHeads();
            manualPortal = false;
            Serial.println("Wi-Fi setup portal closed or failed; retry delayed");
        }
    }

    void begin()
    {
        WiFi.mode(WIFI_STA);
        WiFi.hostname("wordclock");
        WiFi.setAutoReconnect(true);
        manager.setConfigPortalBlocking(false);
        manager.setConfigPortalTimeout(120);
        manager.setTitle("Wordclock");
        manager.setCustomHeadElement("");
        const char* menu[] = {"wifi"};
        manager.setMenu(menu, 1);
        manager.setShowPassword(false);
        manager.setDisableConfigPortal(false);
        manager.setBreakAfterConfig(false);
        manager.setConnectTimeout(20);
        manager.setSaveConnectTimeout(20);
        manager.setConnectRetries(1);
        manager.setPreSaveConfigCallback([]() {
            // Only the native save response receives the status script.
            prepareSaveHead();
            savePending = true;
            saveSucceeded = false;
            saveFailed = false;
            finishRequested = false;
        });
        manager.setSaveConfigCallback([]() {
            if (savePending && isConnected()) {
                saveSucceeded = true;
                successAt = millis();
            }
        });
        manager.setWebServerCallback([]() {
            manager.server->on("/wc-save.js", HTTP_GET, []() {
                manager.server->sendHeader("Cache-Control", "no-store");
                manager.server->send_P(200, "application/javascript", SAVE_SCRIPT);
            });
            manager.server->on("/wc-handoff", HTTP_GET, []() {
                String body = "{\"ready\":";
                const bool ready = saveSucceeded && isConnected();
                body += ready ? "true" : "false";
                body += ",\"failed\":";
                body += saveFailed ? "true" : "false";
                body += ",\"ip\":\"";
                if (ready) body += getIPAddress();
                body += "\",\"device\":\"";
                body += String(ESP.getChipId(), HEX);
                body += "\"}";
                manager.server->sendHeader("Cache-Control", "no-store");
                manager.server->send(200, "application/json", body);
            });
            manager.server->on("/wc-finish", HTTP_POST, []() {
                if (!saveSucceeded || !isConnected()) {
                    manager.server->send(409, "text/plain", "Not connected");
                    return;
                }
                finishRequested = true;
                finishAt = millis();
                manager.server->send(200, "text/plain", "OK");
            });
        });
        needsSetup = !manager.getWiFiIsSaved();
        disconnectedSince = millis();
        if (!needsSetup) WiFi.begin();
        // Open the portal in update(), after the other services initialize.
    }

    void startConfigPortal()
    {
        openPortal(true);
    }

    bool clearSavedNetwork()
    {
        manager.resetSettings();
        return !manager.getWiFiIsSaved();
    }

    void forgetNetwork()
    {
        manager.resetSettings();
        ESP.restart();
    }

    void update()
    {
        manager.process();
        if (savePending) {
            // process() handles a submitted connection synchronously.
            restorePortalHead();
            saveFailed = !saveSucceeded;
            savePending = false;
        }
        if (saveSucceeded && !isConnected()) {
            saveSucceeded = false;
            finishRequested = false;
        }
        const bool connected = isConnected();
        if (connected)
        {
            if (offlineAP && WiFi.softAPgetStationNum() == 0 && !WebServer::firmwareSessionActive()) stopOfflineAP();
            if (!wasConnected)
            {
                Serial.println("Wi-Fi connected");
                Serial.println(getIPAddress());
                everConnected = true;
                needsSetup = false;
            }
            if (isPortalActive()) {
                const uint32_t now = millis();
                if (saveSucceeded) {
                    if ((finishRequested && now - finishAt >= 1200) ||
                        now - successAt >= HANDOFF_TIMEOUT)
                        manager.stopConfigPortal();
                } else if (!manualPortal && !savePending && !saveFailed) {
                    manager.stopConfigPortal();
                }
            }
            if (!isPortalActive())
            {
                manualPortal = false;
                portalAttempted = false;
            }
        }
        else
        {
            if (wasConnected)
            {
                disconnectedSince = millis();
                portalAttempted = false;
                Serial.println("Wi-Fi disconnected");
            }
            const uint32_t now = millis();
            const uint32_t timeout = everConnected ? RECONNECT_TIMEOUT : STARTUP_TIMEOUT;
            if (!isPortalActive() && !offlineAP &&
                (needsSetup || now - disconnectedSince >= timeout) &&
                (!portalAttempted || now - lastPortalAttempt >= PORTAL_RETRY))
            {
                portalAttempted = true;
                lastPortalAttempt = now;
                startOfflineAP();
            }
        }
        if (!isPortalActive() && (portalHead || saveHead)) releasePortalHeads();
        wasConnected = connected;
    }
}
