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

    // Presentation only: keep WiFiManager captive portal and native forms.
    static const char PORTAL_HEAD[] = R"WCHEAD(
<style>
html{background:#f5f5f3;color:#252525}body{margin:0;font-family:system-ui,-apple-system,Segoe UI,sans-serif;text-align:center;color:#252525} .wrap{display:block;width:100%;min-width:0;max-width:460px;margin:0 auto;padding:28px 20px;text-align:left} .wrap h1{font-size:30px;letter-spacing:-1.5px;margin:0 0 4px}.wrap h2{font-size:23px;margin:24px 0 10px}.wrap h3{font-size:14px;color:#656565;font-weight:400} .wrap p{line-height:1.5;color:#555}.wrap form{margin:12px 0}.wrap input:not([type=checkbox]){background:white;border:1px solid #ccc;border-radius:10px;padding:12px;font-size:16px;min-height:46px;margin:8px 0 16px}.wrap label{font-size:14px;font-weight:500}.wrap button{background:#252525;color:#fff;border-radius:10px;font-size:16px;line-height:1.3;min-height:46px;padding:12px}.wrap form[action*='refresh'] button,.wrap form[action='/'] button{background:transparent;color:#252525;border:1px solid #ccc}.wrap>div:has(>a[data-ssid]){background:white;border:1px solid #ddd;border-radius:9px;padding:14px 10px;overflow:hidden}.wrap a[data-ssid]{display:inline-block;max-width:75%;overflow-wrap:anywhere;font-weight:500}.wrap .q{padding:0 2px;min-width:32px}.wrap .msg{background:white;border:1px solid #ddd;border-left:3px solid #999;border-radius:10px;padding:14px;font-size:14px;line-height:1.5}.wrap .msg.D{border-left-color:#a33}.wrap .msg.S{border-left-color:#56866a}.wrap hr{border:0;border-top:1px solid #ddd}.wc-intro{padding:0;margin:0 0 20px}.wc-foot{font-size:12px;text-align:center;color:#666;margin-top:24px}
</style>
<script>
document.addEventListener('DOMContentLoaded',function(){
 document.documentElement.lang='fr';document.title='Wordclock | Configuration Wi-Fi';
 var w=document.querySelector('.wrap');if(!w)return;
 var field=document.getElementById('s'),root=!!w.querySelector("form[action='/wifi']"),saved=location.pathname==='/wifisave';
 var intro=document.createElement('section');intro.className='wc-intro';
 var h=document.createElement('h1');h.textContent='wordclock.';intro.appendChild(h);
 var p=document.createElement('p');p.textContent='Le temps en toutes lettres.';intro.appendChild(p);
 var title=document.createElement('h2');title.textContent=field?'Connexion Wi-Fi':saved?'Connexion en cours':'Bienvenue.';intro.appendChild(title);
 var desc=document.createElement('p');desc.textContent=field?'Choisissez un réseau ou saisissez son nom manuellement.':saved?'Patientez pendant la tentative de connexion.':'Connectez votre horloge à votre réseau domestique.';intro.appendChild(desc);
 if(root){var old=w.querySelector('h1');if(old)old.remove();old=w.querySelector('h3');if(old)old.remove();}w.insertBefore(intro,w.firstChild);
 var translations={'Configure WiFi':'Choisir mon réseau Wi-Fi','Save':'Connecter mon horloge','Refresh':'Actualiser les réseaux','Back':'Retour','SSID':'Nom du réseau (SSID)','Password':'Mot de passe Wi-Fi','Show Password':'Afficher le mot de passe','Connected':'Connectée','Not connected':'Non connectée','No AP set':'Aucun réseau enregistré','Authentication failure':'Mot de passe incorrect ou authentification refusée','AP not found':'Réseau introuvable','Could not connect':'Connexion impossible'};
 w.querySelectorAll('button,label,strong').forEach(function(e){var t=e.textContent.trim();if(translations[t])e.textContent=translations[t];});
 w.querySelectorAll('.msg').forEach(function(e){Array.from(e.childNodes).forEach(function(n){if(n.nodeType===3){var t=n.textContent.trim();if(translations[t])n.textContent=translations[t];}});});
 if(field){var pass=document.getElementById('p');if(pass)pass.placeholder='Mot de passe du réseau';field.addEventListener('input',function(){if(pass)pass.disabled=false;});}
 if(saved){
 var msg=w.querySelector('.msg');if(msg)msg.textContent='Vérification de la connexion…';
 var attempts=0;
 async function poll(){
  try{
   var r=await fetch('/wc-handoff',{cache:'no-store'}),d=await r.json();
   if(d.ready&&d.ip){
    title.textContent='Votre horloge est connectée.';
    desc.textContent='Reconnectez votre téléphone au réseau domestique pour accéder à votre horloge.';
    if(msg)msg.textContent='Adresse de votre horloge : http://'+d.ip;
    var a=document.createElement('a');a.href='http://'+d.ip;a.textContent=a.href;w.appendChild(a);
    var b=document.createElement('button');b.textContent='Ouvrir ma Wordclock';w.appendChild(b);
    b.onclick=async function(){b.disabled=true;try{await fetch('/wc-finish',{method:'POST'});}catch(e){}if(msg)msg.textContent='Reconnectez-vous au réseau domestique. Si cette fenêtre se ferme, ouvrez '+a.href+' dans votre navigateur.';setTimeout(function(){location.href=a.href;},2500);};
    return;
   }
  }catch(e){}
  if(++attempts<35)setTimeout(poll,1000);else if(msg)msg.textContent='Connexion non confirmée. Revenez à la sélection du réseau pour réessayer.';
 }
 setTimeout(poll,1000);
 }
 var foot=document.createElement('p');foot.className='wc-foot';foot.textContent='Aucune application. Aucun compte.';w.appendChild(foot);
});
</script>
)WCHEAD";

    // Keep the captive portal available briefly after a verified save.
    static bool savePending = false;
    static bool saveSucceeded = false;
    static bool finishRequested = false;
    static uint32_t successAt = 0;
    static uint32_t finishAt = 0;
    constexpr uint32_t HANDOFF_TIMEOUT = 60000;

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
        const char* menu[] = {"wifi"};
        manager.setMenu(menu, 1);
        manager.setCustomHeadElement(PORTAL_HEAD);
        manager.setShowPassword(false);
        manager.setDisableConfigPortal(false);
        manager.setBreakAfterConfig(false);
        manager.setPreSaveConfigCallback([]() {
            savePending = true;
            saveSucceeded = false;
            finishRequested = false;
        });
        manager.setSaveConfigCallback([]() {
            if (savePending && isConnected()) {
                saveSucceeded = true;
                successAt = millis();
            }
        });
        manager.setWebServerCallback([]() {
            manager.server->on("/wc-handoff", HTTP_GET, []() {
                String body = "{\"ready\":";
                const bool ready = saveSucceeded && isConnected();
                body += ready ? "true" : "false";
                body += ",\"ip\":\"";
                if (ready) body += getIPAddress();
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

    void forgetNetwork()
    {
        manager.resetSettings();
        ESP.restart();
    }

    void update()
    {
        manager.process();
        if (savePending && !saveSucceeded) {
            // process() handles a submitted connection synchronously.
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
                } else if (!manualPortal && !savePending) {
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
