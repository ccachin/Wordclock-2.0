const animationControls = Object.fromEntries(["front", "rear"].map(face => [face, {
    effect: document.getElementById(face + "Effect"),
    speed: document.getElementById(face + "Speed"),
    row: document.getElementById(face + "SpeedRow"),
    value: document.getElementById(face + "SpeedValue")
}]));
function updateAnimationControls() {
    for (const c of Object.values(animationControls)) {
        c.row.hidden = c.effect.value === "solid";
        c.value.textContent = c.speed.value + " %";
    }
}

const timeElement =
    document.getElementById("time");

const statusElement =
    document.getElementById("status");

const frontColor =
    document.getElementById("frontColor");

const rearColor =
    document.getElementById("rearColor");

const brightness =
    document.getElementById("brightness");

const brightnessValue =
    document.getElementById("brightnessValue");

const harmony =
    document.getElementById("harmony");

const harmonyDescription =
    document.getElementById("harmonyDescription");

const manualRearRow =
    document.getElementById("manualRearRow");

const automaticRearRow =
    document.getElementById("automaticRearRow");

const effectiveRearColor =
    document.getElementById("effectiveRearColor");

const frontPreview =
    document.getElementById("frontPreview");

const rearPreview =
    document.getElementById("rearPreview");

const themeList =
    document.getElementById("themeList");

const activeTheme =
    document.getElementById("activeTheme");

const message =
    document.getElementById("message");


let themes = [];

let themeTimer = null;

let pending = false;

let currentRequest = null;

let applyingTheme = false;

let initialized = false;

let revision = 0;

let effectiveRear = "#3c64b4";


const harmonyDescriptions = {

    "manual":
        "Deux couleurs entièrement indépendantes.",

    "identical":
        "La même couleur à l'avant et à l'arrière.",

    "complementary":
        "Une couleur opposée sur le cercle chromatique.",

    "analogous-plus":
        "Une couleur voisine, décalée de +30°.",

    "analogous-minus":
        "Une couleur voisine, décalée de -30°.",

    "triadic":
        "Une couleur décalée de 120°."

};


// ======================================================
// HTTP API
// ======================================================

// One local HTTP request at a time, including body consumption. The ESP8266
// has a small heap and handles clients sequentially. Maintenance can exclude
// normal requests while a release is in progress.
window.WordclockLocal = (() => {
    let tail = Promise.resolve(), maintenance = false;
    return {
        get busy() { return maintenance; },
        setBusy(value) { maintenance = value; },
        run(task, privileged=false) {
            const next = tail.then(() => {
                if (maintenance && !privileged) throw Error('Mise à jour en cours');
                return task();
            });
            tail = next.catch(() => {});
            return next;
        }
    };
})();

async function request(url, options = {}) {
    return window.WordclockLocal.run(async () => {
    const response = await fetch(url, {

        cache: "no-store",
        signal: AbortSignal.timeout(10000),
        ...options

    });

    if (!response.ok) {

        throw new Error(
            "HTTP " + response.status
        );
    }

    return await response.json();
    });
}


// ======================================================
// Messages
// ======================================================

function showMessage(text, error = false) {

    message.textContent = text;

    message.classList.toggle(
        "error",
        error
    );
}


// ======================================================
// System status
// ======================================================

let clockAnchor = null;
function tickLocalClock() {
    if (!clockAnchor) return;
    const sec=(clockAnchor.seconds+Math.floor((performance.now()-clockAnchor.at)/1000))%86400;
    const h=Math.floor(sec/3600),m=Math.floor(sec/60)%60;
    homeTime=[h,m];
    timeElement.textContent=[h,m,sec%60].map(n=>String(n).padStart(2,'0')).join(':');
}
setInterval(tickLocalClock,1000);
async function updateStatus() {
    if (window.WordclockLocal.busy || document.hidden) { setTimeout(updateStatus,5000); return; }
    try {

        const data =
            await request("/api/status");

        timeElement.textContent =
            data.timeValid
                ? data.time
                : "--:--:--";

        const match=data.timeValid&&/^(\d{2}):(\d{2})/.exec(data.time);
        homeTime=match?[Number(match[1]),Number(match[2])]:[-1,-1];
        const full=data.timeValid&&/^(\d{2}):(\d{2}):(\d{2})$/.exec(data.time);
        clockAnchor=full?{seconds:Number(full[1])*3600+Number(full[2])*60+Number(full[3]),at:performance.now()}:null;
        statusElement.textContent =
            "Connectée";

        statusElement.style.color =
            "var(--success)";

    } catch (error) {

        statusElement.textContent =
            "Connexion perdue";

        statusElement.style.color =
            "var(--error)";

        clockAnchor=null;
        homeTime=[-1,-1];
        timeElement.textContent =
            "--:--:--";

    } finally {

        setTimeout(updateStatus, 5000);
    }
}


// ======================================================
// User controls
// ======================================================

let frontBrightness = 100;
let rearBrightness = 100;
function setControlsEnabled(enabled) {
    document.getElementById("powerToggle").disabled = !enabled;

    frontColor.disabled = !enabled;

    rearColor.disabled = !enabled;

    brightness.disabled = !enabled;

    harmony.disabled = !enabled;
    for (const c of Object.values(animationControls)) { c.effect.disabled = !enabled; c.speed.disabled = !enabled; }

    document.getElementById("homeTheme").textContent=activeTheme.textContent;
    themeList
        .querySelectorAll(".theme-card")
        .forEach(button => {

            button.disabled = !enabled;
        });
}

function updateBrightnessLabel() {

    document.getElementById("powerToggle").textContent=Number(brightness.value)===0?"Allumer":"Éteindre";
    if(Number(brightness.value)>0){lastBrightness=Number(brightness.value);try{localStorage.setItem("wordclock-last-brightness",String(lastBrightness));}catch(e){}}
    brightnessValue.textContent =

        Math.round(
            Number(brightness.value) / 255 * 100
        ) + " %";
}


// ======================================================
// Palette preview
// ======================================================

function readableTextColor(hex) {

    const rgb = parseInt(
        hex.replace("#", ""),
        16
    );

    const r = (rgb >> 16) & 255;

    const g = (rgb >> 8) & 255;

    const b = rgb & 255;

    const luminance =

        (r * 299 + g * 587 + b * 114) / 1000;

    return luminance > 155

        ? "#000000"

        : "#ffffff";
}

function setPreviewColor(element, color) {

    element.style.backgroundColor = color;

    element.style.color =
        readableTextColor(color);
}

function updatePalettePreview() {

    const rear = harmony.value === "manual"

        ? rearColor.value

        : effectiveRear;

    setPreviewColor(
        frontPreview,
        frontColor.value
    );

    setPreviewColor(
        rearPreview,
        rear
    );

    effectiveRearColor.style.backgroundColor =
        rear;

    effectiveRearColor.title =
        rear.toUpperCase();
}


// ======================================================
// Harmony controls
// ======================================================

function updateHarmonyControls() {

    const manual =
        harmony.value === "manual";

    manualRearRow.hidden = !manual;

    automaticRearRow.hidden = manual;

    harmonyDescription.textContent =
        harmonyDescriptions[harmony.value] || "";

    updatePalettePreview();
}


// ======================================================
// Synchronize controls
// ======================================================

function updateControls(data) {
    for (const [face,c] of Object.entries(animationControls)) {
        c.effect.value = data[face + "Effect"] || "solid";
        c.speed.value = data[face + "Speed"] ?? 50;
    }
    updateAnimationControls();

    frontColor.value =
        data.front.toLowerCase();

    rearColor.value =
        data.rear.toLowerCase();

    brightness.value =
        data.brightness;
    frontBrightness = data.frontBrightness ?? 100;
    rearBrightness = data.rearBrightness ?? 100;

    harmony.value =
        data.harmony || "manual";

    effectiveRear =

        (
            data.effectiveRear ||
            data.rear
        ).toLowerCase();

    updateBrightnessLabel();

    updateHarmonyControls();

    updateActiveTheme();
}


// ======================================================
// Active theme detection
// ======================================================

function updateActiveTheme() {

    let selected = null;

    {

        selected = themes.find(theme =>

            (theme.harmony || "manual") === harmony.value &&

            theme.front.toLowerCase() ===
            frontColor.value.toLowerCase() &&

            theme.rear.toLowerCase() ===
            rearColor.value.toLowerCase() &&
            (theme.frontBrightness ?? 100) === frontBrightness &&
            (theme.rearBrightness ?? 100) === rearBrightness &&
            ["front", "rear"].every(face => {
                const c = animationControls[face];
                const effect = theme[face + "Effect"] || "solid";
                return effect === c.effect.value && (effect === "solid" || Number(c.speed.value) === (theme[face + "Speed"] ?? 50));
            })
        );
    }

    activeTheme.textContent =

        selected

            ? selected.name

            : "Personnalisé";

    document.getElementById("homeTheme").textContent=activeTheme.textContent;
    themeList
        .querySelectorAll(".theme-card")
        .forEach(button => {

            const isActive =

                !!selected &&

                button.dataset.id === selected.id;

            button.classList.toggle(
                "active",
                isActive
            );

            button.setAttribute(
                "aria-pressed",
                String(isActive)
            );
        });
}


// ======================================================
// Theme library
// ======================================================

function renderThemeLibrary() {

    themeList.replaceChildren();

    for (const theme of themes) {

        const button =
            document.createElement("button");

        button.type = "button";

        button.className = "theme-card";

        button.dataset.id = theme.id;

        const preview =
            document.createElement("div");

        preview.className = "theme-preview";

        const v = window.WordclockStudioVisual;
        const f = v.rgb(theme.front);
        const offsets = {identical:0,complementary:180,'analogous-plus':30,'analogous-minus':-30,triadic:120};
        const r = theme.harmony && theme.harmony !== 'manual' ? v.rotate(f, offsets[theme.harmony] ?? 0) : v.rgb(theme.rear);
        const fl = (theme.frontBrightness ?? 100) / 100;
        const rl = (theme.rearBrightness ?? 100) / 100;
        preview.style.setProperty('--front', fl === 0 ? 'var(--face-unlit,#393939)' : v.css(f.map(n=>n*(0.45+0.55*Math.pow(fl,0.4)))));
        preview.style.setProperty('--rear', v.css(r.map(n=>n*(rl>0?0.45+0.55*Math.pow(rl,0.4):0))));
        if(rl === 0) preview.style.boxShadow = 'none';
        v.matrixPreview(preview);

        const name =
            document.createElement("span");

        name.className = "theme-name";

        name.textContent = theme.name;
        if (theme.id.startsWith("custom-")) {
            name.textContent += " (Perso)";
        }

        const colors =
            document.createElement("div");

        colors.className = "theme-colors";

        for (const color of [
            theme.front,
            theme.rear
        ]) {

            const dot =
                document.createElement("span");

            dot.className = "color-dot";

            dot.style.backgroundColor = color;

            colors.appendChild(dot);
        }

        button.append(
            preview,
            name,
            colors
        );

        button.addEventListener(
            "click",
            () => applyTheme(theme.id)
        );

        const tile=document.createElement('div');tile.className='library-tile';
        const more=document.createElement('button');more.type='button';more.className='library-action library-more';more.textContent='Options';more.setAttribute('aria-label','Options : '+theme.name);
        more.addEventListener('click',()=>window.wordclockLibrary.open(theme.id));
        tile.append(button,more);themeList.appendChild(tile);
    }

    updateActiveTheme();

    setControlsEnabled(
        initialized && !applyingTheme
    );
}


// ======================================================
// Initialization
// ======================================================

async function initialize() {
    if (window.WordclockLocal.busy) { setTimeout(initialize,3000); return; }
    setControlsEnabled(false);

    try {

        const data =
            await request("/api/theme");

        if (!data.harmony || !data.frontEffect || !data.rearEffect) {

            throw new Error(
                "Firmware update required"
            );
        }

        updateControls(data);

        themes =
            await request("/api/themes?version=3");

        initialized = true;

        renderThemeLibrary();

        setControlsEnabled(true);

        showMessage("");

    } catch (error) {

        showMessage(

            "Impossible de charger les paramètres : " +
            error.message,

            true
        );

        setTimeout(initialize, 3000);
    }
}


// ======================================================
// Manual theme updates
// ======================================================

function sendTheme() {

    if (!initialized || applyingTheme) {

        return;
    }

    if (currentRequest) {

        pending = true;

        return;
    }

    pending = false;

    const requestRevision = revision;

    const params = new URLSearchParams({

        front: frontColor.value,

        rear: rearColor.value,

        brightness: brightness.value,

        harmony: harmony.value,
        frontEffect: animationControls.front.effect.value,
        rearEffect: animationControls.rear.effect.value,
        frontSpeed: animationControls.front.speed.value,
        rearSpeed: animationControls.rear.speed.value

    });

    currentRequest = request(

        "/api/theme",

        {

            method: "POST",

            body: params

        }

    )

        .then(data => {

            // Ignore stale responses.

            if (
                requestRevision === revision &&
                !applyingTheme
            ) {

                updateControls(data);

                showMessage(
                    "Réglages appliqués"
                );
            }
        })

        .catch(error => {

            if (requestRevision === revision) {

                showMessage(

                    "Impossible d'appliquer les réglages.",

                    true
                );
            }
        })

        .finally(() => {

            currentRequest = null;

            if (pending && !applyingTheme) {

                sendTheme();
            }
        });
}

function scheduleThemeUpdate() {

    revision++;
    updateAnimationControls();

    updateBrightnessLabel();

    updateHarmonyControls();

    updateActiveTheme();

    clearTimeout(themeTimer);

    themeTimer = setTimeout(
        sendTheme,
        100
    );
}


// ======================================================
// Apply predefined theme
// ======================================================

async function applyTheme(id) {

    if (applyingTheme || !initialized) {

        return false;
    }

    applyingTheme = true;

    revision++;

    clearTimeout(themeTimer);

    pending = false;

    setControlsEnabled(false);

    if (currentRequest) {

        await currentRequest;
    }

    try {

        // Preserve the latest brightness.

        await request(

            "/api/theme",

            {

                method: "POST",

                body: new URLSearchParams({

                    brightness: brightness.value

                })

            }
        );

        const data = await request(

            "/api/themes/apply",

            {

                method: "POST",

                body: new URLSearchParams({
                    id
                })

            }
        );

        updateControls(data);

        showMessage("Thème appliqué");
        return true;

    } catch (error) {

        showMessage(

            "Impossible d'appliquer le thème.",

            true
        );

        try {

            const data =
                await request("/api/theme");

            updateControls(data);

        } catch (syncError) {

            console.error(syncError);
        }

        return false;

    } finally {

        applyingTheme = false;

        setControlsEnabled(true);
    }
}


// ======================================================
// Event listeners
// ======================================================

frontColor.addEventListener(

    "input",

    scheduleThemeUpdate

);

rearColor.addEventListener(

    "input",

    scheduleThemeUpdate

);

brightness.addEventListener(

    "input",

    scheduleThemeUpdate

);

harmony.addEventListener(

    "change",

    scheduleThemeUpdate

);


// ======================================================
// Start
// ======================================================

let homeTime=[-1,-1], lastBrightness=8, nightPreview=null;
try {const n=Number(localStorage.getItem('wordclock-last-brightness'));if(Number.isInteger(n)&&n>0&&n<=255)lastBrightness=n;}catch(e){}
const powerToggle=document.getElementById('powerToggle');
powerToggle.addEventListener('click',()=>{brightness.value=Number(brightness.value)?0:lastBrightness;scheduleThemeUpdate();});
document.addEventListener('wordclock:night-status',e=>{nightPreview=e.detail;});
// Defer initialization until all deferred scripts have executed.
document.addEventListener('DOMContentLoaded',()=>{
    const v=window.WordclockStudioVisual, el=document.querySelector('.home-preview');
    const setTime=v.matrixPreview(el,-1,-1);
    let previous='', lastKey='', start=performance.now();
    function draw(now){
        if(!document.hidden&&!document.getElementById('homeView').hidden){
            const key=homeTime.join(':');if(key!==previous){setTime(...homeTime);previous=key;}
            const effectKey=[frontColor.value,rearColor.value,harmony.value,...Object.values(animationControls).flatMap(c=>[c.effect.value,c.speed.value])].join(':');
            if(effectKey!==lastKey){start=now;lastKey=effectKey;}
            let g=initialized?Number(brightness.value):0;
            if(nightPreview?.active)g=nightPreview.mode===2?0:Math.min(g,nightPreview.brightness);
            const screenLevel=n=>n>0?0.45+0.55*Math.pow(n,0.4):0;
            const scale=(c,n)=>v.css(c.map(x=>x*screenLevel(n)));
            const f=v.rgb(frontColor.value),r=v.rgb(harmony.value==='manual'?rearColor.value:effectiveRear);
            const fc=v.effect(f,{...animationControls.front,start},now);
            const rc=Array.from({length:20},(_,i)=>v.effect(r,{...animationControls.rear,start},now,i));
            // Screen contrast is perceptual, not a linear LED power measurement.
            const frontLevel=g/255*Number(frontBrightness)/100;
            const peak=Math.max(...fc);
            const perceptual=frontLevel>0&&peak>0 ? screenLevel(frontLevel) : 0;
            const normalization=peak>0 ? Math.max(1,235/Math.max(...f,1)) : 1;
            const displayFront=fc.map(x=>x*normalization*perceptual);
            el.style.setProperty('--front',v.css(displayFront));
            const rearScale=nightPreview?.active&&nightPreview.mode===1?0:g/255*Number(rearBrightness)/100;
            const groups=Array.from({length:4},(_,side)=>{const a=rc.slice(side*5,side*5+5);return scale([0,1,2].map(k=>a.reduce((sum,c)=>sum+c[k],0)/5),rearScale);});
            el.style.boxShadow=`0 12px 24px 4px ${groups[0]}, -12px 0 24px 4px ${groups[1]}, 0 -12px 24px 4px ${groups[2]}, 12px 0 24px 4px ${groups[3]}`;
            el.classList.toggle('rear-off',rearScale===0);
            el.classList.toggle('front-off',g===0||Number(frontBrightness)===0);
        }
        setTimeout(()=>requestAnimationFrame(draw),50);
    }
    requestAnimationFrame(draw);updateStatus();initialize();
});

const transitionEnabled = document.getElementById("transitionEnabled");
const transitionDuration = document.getElementById("transitionDuration");
const transitionValue = document.getElementById("transitionValue");
const transitionReset = document.getElementById("transitionReset");
const transitionMessage = document.getElementById("transitionMessage");
let transitionBusy = true;
function updateTransitionUI() {
    transitionEnabled.disabled = transitionBusy;
    transitionDuration.disabled = transitionBusy || !transitionEnabled.checked;
    transitionReset.disabled = transitionBusy;
    transitionValue.textContent = transitionEnabled.checked ? (Number(transitionDuration.value)/1000).toLocaleString("fr-CH", {minimumFractionDigits:1, maximumFractionDigits:1}) + " s" : "Instantané";
}
function showTransition(data) {
    transitionEnabled.checked = data.duration > 0;
    if (data.duration > 0) transitionDuration.value = data.duration;
}
async function loadTransitions() {
    try { showTransition(await request("/api/transitions")); transitionBusy = false; transitionMessage.textContent = ""; }
    catch (e) { transitionMessage.textContent = "Réglages indisponibles, nouvelle tentative…"; setTimeout(loadTransitions, 3000); }
    updateTransitionUI();
}
async function saveTransitions() {
    if (transitionBusy) return;
    transitionBusy = true; updateTransitionUI();
    try {
        showTransition(await request("/api/transitions", {method:"POST", body:new URLSearchParams({duration:transitionEnabled.checked ? transitionDuration.value : "0"})}));
        transitionMessage.textContent = "Réglage appliqué";
    } catch (e) {
        transitionMessage.textContent = "Échec de la modification";
        try { showTransition(await request("/api/transitions")); } catch (ignored) { setTimeout(loadTransitions, 3000); return; }
    }
    transitionBusy = false; updateTransitionUI();
}
transitionEnabled.addEventListener("change", saveTransitions);
transitionDuration.addEventListener("input", updateTransitionUI);
transitionDuration.addEventListener("change", saveTransitions);
transitionReset.addEventListener("click", () => { transitionEnabled.checked = true; transitionDuration.value = 1500; saveTransitions(); });
loadTransitions();

for (const c of Object.values(animationControls)) {
    c.effect.addEventListener("change", scheduleThemeUpdate);
    c.speed.addEventListener("input", scheduleThemeUpdate);
}

async function refreshThemeLibrary() {
    themes = await request("/api/themes?version=3");
    renderThemeLibrary();
}

// Browser-local appearance, independent of device settings.
const appearanceToggle=document.getElementById('appearanceToggle');
function setAppearance(dark){document.documentElement.dataset.appearance=dark?'dark':'light';appearanceToggle.textContent=dark?'Mode clair':'Mode sombre';appearanceToggle.setAttribute('aria-pressed',String(dark));}
let darkAppearance=false;try{darkAppearance=localStorage.getItem('wordclock-appearance')==='dark';}catch(e){}
setAppearance(darkAppearance);
appearanceToggle.addEventListener('click',()=>{darkAppearance=!darkAppearance;setAppearance(darkAppearance);try{localStorage.setItem('wordclock-appearance',darkAppearance?'dark':'light');}catch(e){}});

// Device-wide physical face color. Preview only; never changes theme data.
(()=>{
 const fields=document.getElementById('faceFields'), picker=document.getElementById('facePicker'), msg=document.getElementById('faceMessage'), presets=document.getElementById('facePresets');
 const sample=document.getElementById('faceSample');
 const colors=[['Noir','#151515'],['Blanc','#F5F5F5'],['Anthracite','#454545'],['Gris','#909090'],['Rouge','#A13835'],['Bleu','#304D70'],['Vert','#536B49'],['Terracotta','#B46B50']];
 let saved='#151515',busy=false;
 function paint(el,c){const rgb=[1,3,5].map(i=>parseInt(c.slice(i,i+2),16));const l=rgb[0]*.299+rgb[1]*.587+rgb[2]*.114;const v=rgb.map(n=>Math.round(n*.78+(l>135?0:255)*.22));el.style.setProperty('--face-color',c);el.style.setProperty('--face-unlit',`rgb(${v.join(',')})`);}
 function draft(c){picker.value=c;paint(sample,c);for(const b of presets.children)b.setAttribute('aria-pressed',String(b.dataset.color.toLowerCase()===c.toLowerCase()));}
 function apply(c){saved=c;paint(document.documentElement,c);draft(c);}
 for(const [name,c] of colors){const b=document.createElement('button');b.type='button';b.dataset.color=c;const sw=document.createElement('span');sw.className='face-swatch';sw.style.background=c;b.append(sw,document.createTextNode(name));b.addEventListener('click',()=>{draft(c);saveFace();});presets.append(b);}
 picker.addEventListener('input',()=>draft(picker.value));
 async function load(){try{const data=await request('/api/appearance');if(!/^#[0-9a-f]{6}$/i.test(data.faceColor))throw Error('Invalid color');apply(data.faceColor);fields.disabled=false;msg.textContent='';}catch(e){msg.textContent='Apparence indisponible. Nouvelle tentative…';setTimeout(load,3000);}}
 picker.addEventListener('change',saveFace);
 async function saveFace(){if(busy)return;busy=true;fields.disabled=true;try{const data=await request('/api/appearance',{method:'POST',body:new URLSearchParams({faceColor:picker.value})});apply(data.faceColor);msg.textContent='Couleur appliquée';}catch(e){draft(saved);msg.textContent='Échec de la modification';}finally{busy=false;fields.disabled=false;}}
 function init(){
    try { window.WordclockStudioVisual.matrixPreview(sample); }
    catch (error) { console.error('Face preview initialization failed', error); }
    load();
 }
 // Deferred scripts execute while readyState is interactive, before DOMContentLoaded.
 document.addEventListener('DOMContentLoaded', init, {once:true});
})();


(() => {
    document.addEventListener("DOMContentLoaded", () => {
        const el = id => document.getElementById(id);
        const change = el("wifiChange"), forget = el("wifiForget"), message = el("wifiMessage");
        let busy = false;
        function enable(value) { change.disabled = forget.disabled = !value; }
        async function refresh() {
            if (busy) return;
            try {
                const data = await request("/api/wifi");
                el("wifiSsid").textContent = data.ssid || "—";
                el("wifiIp").textContent = data.ip;
                el("wifiHost").textContent = data.connected ? data.hostname : "—";
                el("wifiState").textContent = data.connected ? "Connectée" : data.offlineAP ? "Accès direct (sans routeur)" : "Déconnectée";
                enable(data.connected || data.offlineAP);
            } catch (_) { el("wifiState").textContent = "Indisponible"; enable(false); }
        }
        async function act(action) {
            if (busy || !confirm(action === "forget" ? "Effacer les identifiants Wi-Fi et redémarrer ? Les thèmes sont conservés." : "Ouvrir le portail Wi-Fi ? Cette interface sera temporairement inaccessible.")) return;
            busy = true; enable(false);
            message.textContent = "Authentification administrateur requise…";
            try {
                await request("/api/wifi", {method:"POST", headers:{"X-Wordclock-Action":"wifi"}, body:new URLSearchParams({action})});
                message.textContent = "Commande acceptée. Connecte-toi à Wordclock-Setup (mot de passe : Wordclock123), puis ouvre http://192.168.4.1. Après configuration, recharge cette page.";
            } catch (err) {
                message.textContent = "Commande non confirmée : " + err.message + ". Vérifie si le portail est apparu avant de réessayer.";
                busy = false; refresh();
            }
        }
        change.addEventListener("click", () => act("change"));
        forget.addEventListener("click", () => act("forget"));
        el("settingsTab").addEventListener("click", refresh);
        refresh();
    });
})();

// Local access and browser shortcut guidance (works on plain HTTP).
document.addEventListener('DOMContentLoaded', () => {
    const el = id => document.getElementById(id);
    const host = el('accessHost'), ip = el('accessIp'), message = el('accessMessage');
    if (!host || !ip || !message) return;
    async function copy(field) {
        if (!field.value) return;
        try {
            if (navigator.clipboard && window.isSecureContext) await navigator.clipboard.writeText(field.value);
            else { field.focus(); field.select(); field.setSelectionRange(0, field.value.length); if (!document.execCommand('copy')) throw new Error('copy'); }
            message.textContent = 'Adresse copiée.';
        } catch (_) { field.focus(); field.select(); message.textContent = 'Adresse sélectionnée : copiez-la manuellement.'; }
    }
    el('copyHost').addEventListener('click', () => copy(host));
    el('copyIp').addEventListener('click', () => copy(ip));
    async function refreshAccess() {
        try {
            const data = await request('/api/wifi');
            ip.value = (data.connected || data.offlineAP) && /^\d{1,3}(\.\d{1,3}){3}$/.test(data.ip) ? 'http://' + data.ip + '/' : '';
        } catch (_) { ip.value = ''; }
        el('copyIp').disabled = !ip.value;
    }
    const platform = el('accessPlatform');
    const help = {android: 'Dans Chrome, ouvrez le menu ⋮ puis Ajouter à l’écran d’accueil (ou Créer un raccourci), et confirmez.',ios: 'Dans Safari, ouvrez Partager, puis Sur l’écran d’accueil et confirmez.',windows: 'Dans Chrome, recherchez Créer un raccourci dans le menu. Sinon, sur le bureau Windows, clic droit > Nouveau > Raccourci et saisissez http://wordclock.local/.'};
    platform.value = /Android/i.test(navigator.userAgent) ? 'android' : /iPhone|iPad|iPod/i.test(navigator.userAgent) || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1) ? 'ios' : 'windows';
    function updateHelp() { el('accessHelp').textContent = help[platform.value]; }
    platform.addEventListener('change', updateHelp); updateHelp();
    el('settingsTab').addEventListener('click', refreshAccess);
    refreshAccess();
});


// Clock control: UTC epoch for phone sync; manual date interpreted in the clock timezone.
document.addEventListener('DOMContentLoaded', () => {
    const el = id => document.getElementById(id);
    const phone = el('clockPhone'), manual = el('clockManual'), apply = el('clockApply');
    const source = el('clockSource'), message = el('clockMessage');
    let busy = false;
    function show(data, fill) {
        source.textContent = !data.valid ? 'Heure non réglée' : 'Source : ' + ({ntp:'Internet (NTP)',manual:'Réglage local',unknown:'Non déterminée'}[data.source] || 'Non déterminée');
        if (fill && document.activeElement !== manual) manual.value = data.local || '';
    }
    function lock(value) { phone.disabled = manual.disabled = apply.disabled = value; }
    async function refresh() {
        if (busy) return;
        try { show(await request('/api/clock'), true); lock(false); }
        catch (_) { source.textContent = 'Réglage de l’heure indisponible'; lock(true); }
    }
    async function save(fields) {
        if (busy) return;
        busy = true; lock(true);
        try {
            show(await request('/api/clock',{method:'POST',headers:{'X-Wordclock-Action':'clock'},body:new URLSearchParams(fields)}),true);
            message.textContent = 'Heure synchronisée.';
        } catch (e) { message.textContent = 'Réglage non confirmé : '+e.message; }
        finally { busy = false; lock(false); }
    }
    phone.addEventListener('click', () => save({epoch:String(Math.floor(Date.now()/1000))}));
    apply.addEventListener('click', () => {
        const m = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})(?::(\d{2}))?$/.exec(manual.value);
        if (!m || !manual.checkValidity()) { message.textContent = 'Date ou heure invalide.'; return; }
        const [_,year,month,day,hour,minute,second='00'] = m;
        save({year,month,day,hour,minute,second});
    });
    el('settingsTab').addEventListener('click', refresh);
    refresh();
});

// Load only when settings are opened, not during initial HTTP startup.
(() => {
    const el=id=>document.getElementById(id), fields=el('effectsFields'), message=el('effectsMessage');
    let busy=false, loaded=false, dirty=false;
    const values=()=>({enabled:el('effectsEnabled').checked?1:0,hourlyColor:el('effectsHourly').checked?1:0,
        transition:el('effectsTransition').value,chime:el('effectsChime').value,spatial:el('effectsSpatial').value,spatialSpeed:el('effectsSpatialSpeed').value,
        duration:el('effectsDuration').value,intensity:el('effectsIntensity').value});
    const labels=()=>{
        el('effectsDurationValue').textContent=(Number(el('effectsDuration').value)/1000).toLocaleString('fr-CH')+' s';
        el('effectsIntensityValue').textContent=el('effectsIntensity').value+' %';
        el('effectsSpatialSpeedValue').textContent=el('effectsSpatialSpeed').value+' %';
    };
    const show=a=>{
        el('effectsEnabled').checked=a.enabled;el('effectsHourly').checked=a.hourlyColor;
        for(const k of ['transition','chime','spatial','spatialSpeed','duration','intensity'])el('effects'+k[0].toUpperCase()+k.slice(1)).value=a[k];
        labels();dirty=false;loaded=true;
    };
    async function run(work) {
        if(busy)return;busy=true;fields.disabled=true;
        try{await work();}catch(e){message.textContent='Impossible de terminer : '+e.message;}
        finally{busy=false;fields.disabled=!loaded;}
    }
    function post(path,body){return request(path,{method:'POST',headers:{'X-Wordclock-Action':'animations'},body:new URLSearchParams(body)});}
    el('settingsTab').addEventListener('click',()=>{if(!dirty)run(async()=>{show(await request('/api/animations'));message.textContent='';});});
    for(const key of ['Enabled','Hourly','Transition','Chime','Spatial','SpatialSpeed','Duration','Intensity'])el('effects'+key).addEventListener('input',()=>{dirty=true;labels();message.textContent='Modifications non enregistrées.';});
    el('effectsSave').addEventListener('click',()=>run(async()=>{show(await post('/api/animations',values()));message.textContent='Réglages appliqués.';}));
    el('effectsTest').addEventListener('click',()=>{
        if(dirty){message.textContent='Enregistre les réglages avant le test.';return;}
        run(async()=>{await post('/api/animations/test',{kind:el('effectsTestKind').value});message.textContent='Animation lancée sur l’horloge.';});
    });
})();
