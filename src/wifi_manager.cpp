#include "wifi_manager.h"
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

namespace WifiManager
{
    static WiFiManager manager;
    constexpr char AP_NAME[] = "Wordclock-Setup";
    constexpr char AP_PASSWORD[] = "Wordclock123";
    constexpr uint32_t STARTUP_TIMEOUT = 30000;
    constexpr uint32_t RECONNECT_TIMEOUT = 300000;
    constexpr uint32_t PORTAL_RETRY = 15000;

    // Visual styling only. Keep native forms, scripts and routes intact.
    static const char PORTAL_STYLE[] = R"WCSTYLE(<style>
body{margin:0;background:#f5f4f1;color:#242424;font-family:system-ui,-apple-system,sans-serif}
.wrap{display:block;width:100%;max-width:480px;min-width:0;margin:0 auto;padding:24px 20px}
h1{font-size:2rem;letter-spacing:-.06em;margin:12px 0 20px}
h2,h3{font-weight:600}
button,input[type=submit],input[type=button]{background:#242424;color:white;border-radius:12px;min-height:48px;padding:6px 12px;font-size:1rem}
input[type=text],input[type=password],select{background:white;color:#242424;border:1px solid #aaa;border-radius:9px;padding:12px;min-height:44px}
a[data-ssid]{display:inline-block;max-width:75%;overflow-wrap:anywhere;padding:8px 0;color:#242424}
.msg{background:white;border-radius:12px;overflow-wrap:anywhere}
</style>)WCSTYLE";

    static const char SAVE_WAIT_STYLE[] = R"WCWAIT(<style>
.msg:not(.wc-managed){text-align:center}
.msg:not(.wc-managed)::before{content:'CONNEXION\A AU RESEAU\A EN COURS';display:block;white-space:pre;box-sizing:border-box;max-width:310px;margin:0 auto;background:#191919;color:#fff5df;border-radius:9px;padding:24px 8px 8px;font:500 clamp(12px,4vw,16px)/2 monospace;letter-spacing:.2em;text-shadow:0 0 6px #ffe8b688}
.msg:not(.wc-managed)::after{content:'*';display:block;box-sizing:border-box;max-width:310px;margin:12px auto 0;background:#191919;color:#fff5df;border-radius:9px;padding:0 8px 18px;font:500 18px/2 monospace;letter-spacing:.35em;animation:wc-wait 1.6s steps(1,end) infinite}
@keyframes wc-wait{0%{content:'*'}25%{content:'**'}50%{content:'***'}75%{content:'****'}}
@media(prefers-reduced-motion:reduce){.msg:not(.wc-managed)::after{animation:none;content:'****'}}
</style>)WCWAIT";

    static const char SAVE_SCRIPT[] PROGMEM = R"WCSCRIPT(
window.addEventListener('DOMContentLoaded', function () {
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
    detail.textContent = 'Le r\u00e9seau de configuration va se fermer. Adresse : ' + url;
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
        const address = document.createElement('div');
        address.style.cssText = 'background:#f5f4f1;border-radius:12px;padding:12px;margin:12px 0;text-align:left';
        const label = document.createElement('p');
        label.textContent = 'Adresse locale recommand\u00e9e';
        const field = document.createElement('input');
        field.type = 'text'; field.readOnly = true;
        field.value = 'http://wordclock.local/';
        field.style.cssText = 'width:100%;box-sizing:border-box;margin:4px 0';
        const copy = document.createElement('button');
        copy.type = 'button'; copy.textContent = 'Copier';
        copy.onclick = async () => {
          let copied = false;
          try {
            if (navigator.clipboard && window.isSecureContext) {
              await navigator.clipboard.writeText(field.value); copied = true;
            } else {
              field.focus(); field.select(); field.setSelectionRange(0,field.value.length);
              copied = document.execCommand('copy');
            }
          } catch (error) { /* Manual selection remains available. */ }
          copy.textContent = copied ? 'Copi\u00e9' : 'S\u00e9lectionnez et copiez l\u2019adresse';
        };
        const fallback = document.createElement('p');
        fallback.textContent = 'Adresse IP de secours : ' + url;
        address.append(label, field, copy, fallback);
        msg.insertBefore(address, button);
        button.disabled = false; button.style.opacity = '1';
        button.textContent = 'Ouvrir ma Wordclock';
        button.onclick = () => { window.location.href = url + '/'; };
        const help = document.createElement('p');
        help.style.cssText = 'margin-top:20px;border-top:1px solid #ddd;padding-top:16px';
        help.textContent = 'Gardez votre Wordclock \u00e0 port\u00e9e de main : ouvrez l\u2019adresse locale dans votre navigateur habituel, puis ajoutez un raccourci \u00e0 votre \u00e9cran d\u2019accueil. Les instructions sont aussi disponibles dans les r\u00e9glages.';
        msg.appendChild(help);
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
});
)WCSCRIPT";

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

    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    bool isPortalActive() { return manager.getConfigPortalActive(); }
    String getIPAddress() { return WiFi.localIP().toString(); }

    static void openPortal(bool manual)
    {
        if (isPortalActive()) return;
        savePending = false;
        saveSucceeded = false;
        saveFailed = false;
        finishRequested = false;
        portalAttempted = true;
        manualPortal = manual;
        Serial.println("Starting Wi-Fi setup portal");
        manager.startConfigPortal(AP_NAME, AP_PASSWORD);
        lastPortalAttempt = millis();
        if (!isPortalActive())
        {
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
        manager.setTitle("Wordclock");
        manager.setCustomHeadElement(PORTAL_STYLE);
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
            static const String saveHead = String(PORTAL_STYLE) + SAVE_WAIT_STYLE + "<script defer src=\"/wc-save.js\"></script>";
            manager.setCustomHeadElement(saveHead.c_str());
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
            manager.setCustomHeadElement(PORTAL_STYLE);
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
            if (!isPortalActive() &&
                (needsSetup || now - disconnectedSince >= timeout) &&
                (!portalAttempted || now - lastPortalAttempt >= PORTAL_RETRY))
            {
                openPortal(false);
            }
        }
        wasConnected = connected;
    }
}
