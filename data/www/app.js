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

// Confirm deferred persistence without continuous polling.
async function confirmStored(element, label) {
    const waiting=label+' — enregistrement en cours…';
    element.textContent=waiting;
    for(let i=0;i<6;i++) {
        await new Promise(resolve=>setTimeout(resolve,700));
        if(element.textContent!==waiting)return;
        try {
            const state=await request('/api/system');
            if(element.textContent!==waiting)return;
            if(state.saveFailed){element.textContent=label+' — sauvegarde en échec.';return;}
            if(!state.savePending){element.textContent=label+' — enregistré.';return;}
        } catch(e) { /* retry bounded read, never repeat write */ }
    }
    if(element.textContent===waiting)element.textContent=label+' — sauvegarde non confirmée.';
}

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

            button.setAttribute("aria-pressed",String(isActive));
            button.querySelector(".theme-state").textContent=isActive?"Sélection manuelle":"Activer";
        });
}


// ======================================================
// Theme library
// ======================================================

function renderThemeLibrary() {

    themeList.replaceChildren();

    for (const theme of themes.filter(t=>!t.id.startsWith("custom-"))) {

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
        const effectNames={solid:'Fixe',breathing:'Respiration', 'color-cycle':'Cycle de couleurs',rainbow:'Arc-en-ciel',chase:'Chenillard'};
        preview.dataset.rearEffect=theme.rearEffect||'solid';
        preview.dataset.frontEffect=theme.frontEffect||'solid';
        preview.style.setProperty('--rear-period',`${30-0.29*(theme.rearSpeed??50)}s`);
        preview.style.setProperty('--front-period',`${30-0.29*(theme.frontSpeed??50)}s`);
        const effectLabel=document.createElement('span');effectLabel.className='theme-effect-label';effectLabel.textContent=theme.rearEffect==='solid'&&theme.frontEffect==='solid'?'Éclairage fixe':`Avant : ${effectNames[theme.frontEffect]||'Fixe'} · Arrière : ${effectNames[theme.rearEffect]||'Fixe'}`;


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
            colors, effectLabel
        );
        const state=document.createElement("span");state.className="theme-state";button.append(state);

        button.addEventListener(
            "click",
            () => applyTheme(theme.id)
        );

        const tile=document.createElement('div');tile.className='library-tile';
        tile.append(button);themeList.appendChild(tile);
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
            await request("/api/themes?version=3&factoryOnly=1");

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

                showMessage("Réglages appliqués"); void confirmStored(message,"Réglages appliqués");
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
    showMessage("Modification en cours — aperçu local");
    updateAnimationControls();

    updateBrightnessLabel();

    updateHarmonyControls();

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

        showMessage("Thème appliqué"); void confirmStored(message,"Thème appliqué");
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
        void confirmStored(transitionMessage,"Transition appliquée");
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
    themes = await request("/api/themes?version=3&factoryOnly=1");
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
 async function saveFace(){if(busy)return;busy=true;fields.disabled=true;try{const data=await request('/api/appearance',{method:'POST',body:new URLSearchParams({faceColor:picker.value})});apply(data.faceColor);void confirmStored(msg,'Couleur appliquée');}catch(e){draft(saved);msg.textContent='Échec de la modification';}finally{busy=false;fields.disabled=false;}}
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
    let busy=false, loaded=false, dirty=false, saved=null;
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
        labels();dirty=false;loaded=true;saved={...a};el("effectsState").textContent=a.enabled?"Animations activées dans la configuration":"Animations désactivées";el("effectsSave").disabled=true;el("effectsCancel").disabled=true;
    };
    async function run(work) {
        if(busy)return;busy=true;fields.disabled=true;
        try{await work();}catch(e){message.textContent='Impossible de terminer : '+e.message;}
        finally{busy=false;fields.disabled=!loaded;}
    }
    function post(path,body){return request(path,{method:'POST',headers:{'X-Wordclock-Action':'animations'},body:new URLSearchParams(body)});}
    document.addEventListener('wordclock:settings-section',e=>{if(e.detail==='animations'&&!dirty)run(async()=>{show(await request('/api/animations'));message.textContent='';});});
    for(const key of ['Enabled','Hourly','Transition','Chime','Spatial','SpatialSpeed','Duration','Intensity'])el('effects'+key).addEventListener('input',()=>{dirty=true;labels();el('effectsSave').disabled=false;el('effectsCancel').disabled=false;el('effectsState').textContent='Brouillon — non enregistré';message.textContent='Modifications non enregistrées.';});
    el('effectsSave').addEventListener('click',()=>run(async()=>{show(await post('/api/animations',values()));void confirmStored(message,'Réglages appliqués');}));
    el('effectsCancel').addEventListener('click',()=>{if(saved){show(saved);message.textContent='Modifications annulées.';}});
    el('effectsTest').addEventListener('click',()=>{
        if(dirty){message.textContent='Enregistre les réglages avant le test.';return;}
        run(async()=>{await post('/api/animations/test',{kind:el('effectsTestKind').value});message.textContent='Test temporaire accepté par l’horloge ; retour automatique à l’heure.';});
    });
})();

// Automation configuration is fetched only when Settings is opened.
(() => {
    const el=id=>document.getElementById(id), fields=el('rulesFields'), msg=el('rulesMessage');
    const themes=['Warm White','Ocean','Sunset','Forest','Aurora','Ocean Pulse','Aurora Flow','Rainbow','Ember'];
    const effects=['Fondu','Balayage','Matrix','Apparition progressive','Explosion lumineuse','Coucou'];
    const keys=['start','end','days','month','day','action','value'];
    let rules=null,selected=null,dirty=false,busy=false;
    const list=el('rulesList'), add=el('rulesAdd');
    const option=(text,value)=>{const o=document.createElement('option');o.textContent=text;o.value=value;return o;};
    ['Dim','Lun','Mar','Mer','Jeu','Ven','Sam'].forEach((name,i)=>{const label=document.createElement('label'), input=document.createElement('input');input.type='checkbox';input.id='ruleDay'+i;label.append(input,document.createTextNode(name));el('ruleDays').append(label);});
    function valid(r){if(!r||!keys.every(k=>Number.isInteger(r[k])))return false;const {start,end,days,month,day,action,value}=r;return start>=0&&start<1440&&end>=0&&end<1440&&days>=1&&days<=127&&month>=0&&month<=12&&day>=0&&day<=31&&(!month||!day||day<=[0,31,29,31,30,31,30,31,31,30,31,30,31][month])&&value>=0&&value<=255&&(action===0||(action===1&&value<9&&start!==end)||(action===2&&value<=100&&start!==end)||(action===3&&value>=1&&value<=6&&start===end));}
    function validate(list){if(!Array.isArray(list)||list.length!==16||!list.every(valid))throw Error('Configuration invalide');return list.map(r=>Object.fromEntries(keys.map(k=>[k,r[k]])));}
    function decode(hex){if(typeof hex!=='string'||!/^([0-9a-f]{18}){16}$/i.test(hex))throw Error('Réponse invalide');const bytes=hex.match(/../g).map(x=>parseInt(x,16));return validate(Array.from({length:16},(_,i)=>{const b=bytes.slice(i*9,i*9+9);return {start:b[0]+256*b[1],end:b[2]+256*b[3],days:b[4],month:b[5],day:b[6],action:b[7],value:b[8]};}));}
    function encode(list){return validate(list).flatMap(r=>[r.start&255,r.start>>8,r.end&255,r.end>>8,r.days,r.month,r.day,r.action,r.value]).map(b=>b.toString(16).padStart(2,'0')).join('');}
    function time(n){return String(Math.floor(n/60)).padStart(2,'0')+':'+String(n%60).padStart(2,'0');}
    function minute(id){const v=el(id).value;if(!/^\d{2}:\d{2}$/.test(v))throw Error('Heure invalide');const [h,m]=v.split(':').map(Number);return h*60+m;}
    function actions(value){const action=Number(el('ruleAction').value);el('ruleEndRow').hidden=action===3;el('ruleValueRow').hidden=action===2;el('ruleCapRow').hidden=action!==2;el('ruleValue').replaceChildren(...(action===3?effects:themes).map((x,i)=>option(x,i+(action===3?1:0))));if(value!==undefined)el('ruleValue').value=value;}
    const empty=()=>({start:1080,end:1320,days:127,month:0,day:0,action:0,value:0});
    function daysText(days){return ({127:'Tous les jours',62:'En semaine',65:'Le week-end'})[days]||['Dim','Lun','Mar','Mer','Jeu','Ven','Sam'].filter((_,i)=>days&(1<<i)).join(', ');}
    function showList(){list.replaceChildren();if(!rules)return;let count=0;rules.forEach((r,i)=>{if(!r.action)return;count++;const card=document.createElement('article');card.className='rule-card';const title=document.createElement('strong');title.textContent=r.action===1?themes[r.value]:r.action===2?'Luminosité maximale : '+r.value+' %':effects[r.value-1];const detail=document.createElement('p');detail.className='muted';detail.textContent=(r.action===3?time(r.start):time(r.start)+' – '+time(r.end))+' · '+daysText(r.days)+(r.month?' · Mois '+r.month:'')+(r.day?' · Jour '+r.day:'');const buttons=document.createElement('div');buttons.className='rule-buttons';for(const [name,fn] of [['Modifier',()=>edit(i)],['Supprimer',()=>{if(confirm('Supprimer cette programmation ?'))run(()=>persist(rules.map((x,j)=>j===i?empty():x)));}]]){const b=document.createElement('button');b.type='button';b.className='action-button';b.textContent=name;b.disabled=busy||selected!==null;b.addEventListener('click',fn);buttons.append(b);}card.append(title,detail,buttons);list.append(card);});if(!count){const p=document.createElement('p');p.className='muted';p.textContent='Aucune programmation. Ajoutez par exemple une ambiance du soir.';list.append(p);}add.disabled=busy||selected!==null||rules.every(r=>r.action);}
    function edit(i){if(busy||selected!==null)return;selected=i;const r=rules[i].action?rules[i]:{...empty(),action:1};el('ruleLegend').textContent=rules[i].action?'Modifier la programmation':'Nouvelle programmation';el('ruleAction').value=r.action;el('ruleStart').value=time(r.start);el('ruleEnd').value=time(r.end);el('ruleMonth').value=r.month;el('ruleDay').value=r.day;el('ruleCap').value=r.action===2?r.value:50;for(let j=0;j<7;j++)el('ruleDay'+j).checked=!!(r.days&(1<<j));el('rulePreset').value=[127,62,65].includes(r.days)?String(r.days):'custom';el('ruleDays').hidden=el('rulePreset').value!=='custom';el('ruleDateMode').value=r.month||r.day?'custom':'any';el('ruleDateFields').hidden=el('ruleDateMode').value==='any';actions(r.value);fields.hidden=false;fields.disabled=false;dirty=true;showList();msg.textContent='Brouillon — non enregistré.';}
    function close(){selected=null;dirty=false;fields.hidden=true;showList();}
    function capture(){const action=Number(el('ruleAction').value),start=minute('ruleStart');let days=0;for(let i=0;i<7;i++)if(el('ruleDay'+i).checked)days|=1<<i;const any=el('ruleDateMode').value==='any';const r={start,end:action===3?start:minute('ruleEnd'),days,month:any?0:Number(el('ruleMonth').value),day:any?0:Number(el('ruleDay').value),action,value:action===2?Number(el('ruleCap').value):Number(el('ruleValue').value)};if(!valid(r))throw Error('Vérifiez les jours, les dates et les heures (début et fin différents).');return r;}
    async function run(work){if(busy)return;busy=true;fields.disabled=true;showList();try{await work();}catch(e){msg.textContent=e.message;}finally{busy=false;fields.disabled=!rules||selected===null;showList();}}
    async function persist(next){const wire=encode(next);msg.textContent='Enregistrement en cours…';const r=await request('/api/automations',{method:'POST',headers:{'X-Wordclock-Action':'automations'},body:new URLSearchParams({rules:wire})});const saved=decode(r.rules);if(encode(saved)!==wire)throw Error('Enregistrement non confirmé.');rules=saved;close();msg.textContent='Programmations enregistrées.';}
    document.addEventListener('wordclock:settings-section',e=>{if(e.detail==='rules'&&!dirty)run(async()=>{const r=await request('/api/automations');if(r.version!==1)throw Error('Version incompatible');rules=decode(r.rules);close();msg.textContent='';});});
    add.addEventListener('click',()=>{if(rules)edit(rules.findIndex(r=>!r.action));});
    el('ruleAction').addEventListener('change',()=>{actions();if(Number(el('ruleAction').value)!==3&&el('ruleStart').value&&el('ruleStart').value===el('ruleEnd').value)el('ruleEnd').value=time((minute('ruleStart')+60)%1440);});
    el('rulePreset').addEventListener('change',()=>{const v=el('rulePreset').value;el('ruleDays').hidden=v!=='custom';if(v!=='custom')for(let i=0;i<7;i++)el('ruleDay'+i).checked=!!(Number(v)&(1<<i));});
    el('ruleDateMode').addEventListener('change',()=>{el('ruleDateFields').hidden=el('ruleDateMode').value==='any';});
    el('rulesSave').addEventListener('click',()=>run(async()=>{const r=capture();await persist(rules.map((x,i)=>i===selected?r:x));}));
    el('rulesCancel').addEventListener('click',()=>{if(!busy){close();msg.textContent='Modifications annulées.';}});

})();

// Settings navigation, no additional API traffic for local previews.
document.addEventListener('DOMContentLoaded',()=>{
    const el=id=>document.getElementById(id), buttons=[...document.querySelectorAll('[data-settings]')];
    let section='general';
    function open(key){section=key;for(const b of buttons){const active=b.dataset.settings===key;b.setAttribute('aria-pressed',String(active));el('settings-'+b.dataset.settings).hidden=!active;}document.dispatchEvent(new CustomEvent('wordclock:settings-section',{detail:key}));}
    buttons.forEach(b=>b.addEventListener('click',()=>open(b.dataset.settings)));
    el('settingsTab').addEventListener('click',()=>open(section));
    const v=window.WordclockStudioVisual, preview=el('effectPreview'), setTime=v.matrixPreview(preview,8,25), cells=[...preview.querySelectorAll('text')];
    let start=performance.now(), last=0, lastFrame='';
    const reset=()=>{start=performance.now();lastFrame='';};
    document.addEventListener('wordclock:settings-section',e=>{if(e.detail==='animations')reset();});
    el('effectReplay').addEventListener('click',reset);
    el('effectsFields').addEventListener('input',reset);
    function draw(now){
        if(!document.hidden&&!el('settingsView').hidden&&section==='animations'&&now-last>33){
            last=now;const select=el('effectsTransition'),kind=Number(select.value),duration=Number(el('effectsDuration').value)||2500, strength=Number(el('effectsIntensity').value)/100;
            const t=Math.min(1,Math.max(0,(now-start)/duration)),p=t*255, wave=(d,w)=>Math.max(0,1-d/w), ease=x=>x*x*(3-2*x);
            const frame=[kind,Math.round(p),strength,frontColor.value].join(':');if(frame===lastFrame){requestAnimationFrame(draw);return;}lastFrame=frame;
            setTime(8,25);preview.style.setProperty('--front',frontColor.value);
            el('effectPreviewName').textContent=select.selectedOptions[0]?.textContent+' — simulation locale, sans action sur l’horloge';
            const envelope=ease(Math.min(1,t*8,(1-t)*8));
            cells.forEach((c,i)=>{const x=i%11,y=Math.floor(i/11),r=Math.abs(x-5)+Math.abs(y-5),hash=(i*73+19)%117;let a=0,reveal=1;
                if(i<117&&t<1){switch(kind){case 1:reveal=ease(t);break;case 2:a=wave(Math.abs(x*24-p),48);break;case 3:{const h=(Math.floor(p*13/256)+x*7)%14;a=h>=y&&h-y<3?(3-h+y)/3:0;break;}case 4:reveal=ease(Math.max(0,Math.min(1,(t*141-i)/24)));break;case 5:a=wave(Math.abs(r*24-p),48);break;case 7:reveal=ease(Math.max(0,Math.min(1,(t*141-hash)/24)));break;case 8:a=wave(Math.abs((x+y)*12-p),42);break;case 9:a=wave(Math.abs(x*24-(p<128?p*2:(255-p)*2)),34);break;case 10:a=wave(Math.abs(hash*17%256-p),18);break;case 11:a=wave(Math.abs(((x-5)**2+(y-5)**2)*5-p),48);break;}}
                const lit=c.classList.contains('lit');c.style.fill=lit?frontColor.value:a>0?frontColor.value:'';c.style.opacity=lit?String(1-(1-reveal)*strength*.75):a>0?String(a*envelope*strength):'';
            });
        }
        requestAnimationFrame(draw);
    }
    requestAnimationFrame(draw);
});

// Weather settings, loaded only when the weather section is selected.
(()=>{const el=id=>document.getElementById(id),fields=el('weatherFields'),msg=el('weatherMessage');let saved=null,dirty=false,busy=false;const keys=['Lat','Lon','Enabled','Forecast','Zone','Intensity'];let places=[],chosen=null,searchId=0;const city=el('weatherCity'),results=el('weatherResults'),searchStatus=el('weatherSearchStatus');const labelPlace=p=>[p.name,p.admin1,p.country].filter(Boolean).join(' — ');async function searchCity(){const q=city.value.trim(),id=++searchId;results.hidden=true;places=[];if(q.length<2){searchStatus.textContent='Saisissez au moins deux caractères.';return;}searchStatus.textContent='Recherche…';try{const u=new URL('https://geocoding-api.open-meteo.com/v1/search');u.search=new URLSearchParams({name:q,count:10,language:'fr',format:'json'});const r=await fetch(u,{signal:AbortSignal.timeout(10000),credentials:'omit',referrerPolicy:'no-referrer'});if(!r.ok)throw Error('HTTP '+r.status);const d=await r.json();if(id!==searchId)return;places=(d.results||[]).filter(p=>typeof p.name==='string'&&Number.isFinite(p.latitude)&&Number.isFinite(p.longitude)&&Math.abs(p.latitude)<=90&&Math.abs(p.longitude)<=180);results.replaceChildren(new Option('Choisir une localité…',''));places.forEach((p,i)=>results.add(new Option(labelPlace(p),String(i))));results.hidden=!places.length;searchStatus.textContent=places.length?'Sélectionnez votre localité.':'Aucun résultat. Essayez une ville proche.';}catch(e){if(id===searchId)searchStatus.textContent='Recherche indisponible. Vous pouvez saisir les coordonnées manuellement.';}}el('weatherSearch').addEventListener('click',searchCity);city.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();searchCity();}});city.addEventListener('input',()=>{++searchId;results.hidden=true;searchStatus.textContent='';});results.addEventListener('change',()=>{if(results.value==='')return;const p=places[Number(results.value)];if(!p)return;el('weatherLat').value=p.latitude.toFixed(5);el('weatherLon').value=p.longitude.toFixed(5);chosen={name:labelPlace(p),lat:Math.round(p.latitude*100000),lon:Math.round(p.longitude*100000)};city.value=chosen.name;results.hidden=true;searchStatus.textContent='Coordonnées remplies. Enregistrez pour appliquer.';dirty=true;msg.textContent='Modifications non enregistrées.';});const front=['#ffe8ae','#e2e8f0','#a5dcff','#ded1ff','#ffffff','#dddeda','#d5dfff'],rear=['#ed9b29','#64748b','#2266df','#7036ba','#8dddf7','#909b9e','#293d86'];document.addEventListener('DOMContentLoaded',()=>{window.WordclockStudioVisual.matrixPreview(el('weatherPreview'),8,25);preview();},{once:true});function preview(){const p=Number(el('weatherDemo').value),v=el('weatherPreview'),z=Number(el('weatherZone').value),a=Number(el('weatherIntensity').value);const visual=window.WordclockStudioVisual;const f=frontColor.value||'#ffe8ae',r=(harmony.value==='manual'?rearColor.value:effectiveRear)||'#64748b';const blend=(base,hex,amount)=>visual.css(visual.rgb(base).map((n,i)=>n+(visual.rgb(hex)[i]-n)*amount/100));v.style.setProperty('--front',blend(f,front[p],z===0?0:a));v.style.setProperty('--weather-rear',blend(r,rear[p],z===1?0:a));v.dataset.motion=String(z!==1&&a>0&&p>=2&&p<=5);el('weatherLevel').textContent=a+' %';}function status(d){const labels={disabled:'Mode désactivé',waiting:'En attente de données',ok:'Données reçues',memory:'Mémoire insuffisante',network:'Connexion indisponible',response:'Réponse invalide',connect:'Connexion impossible',send:'Envoi incomplet',header:'En-tête incomplet', 'header-limit':'En-tête trop long','http-code':'Code HTTP inattendu','body-limit':'Réponse trop volumineuse',encoding:'Encodage inattendu','body-timeout':'Réception interrompue','body-empty':'Réponse vide',json:'JSON invalide','json-empty':'Réponse JSON vide','json-incomplete':'Réponse JSON incomplète','json-memory':'Mémoire de décodage insuffisante','json-depth':'JSON trop imbriqué',fields:'Champs manquants',timestamp:'Date météo incohérente',forecast:'Prévision indisponible',range:'Valeurs invalides',expired:'Données expirées'};el('weatherStatus').textContent=(d.valid&&d.status!=='ok'?'Dernières données valides conservées — '+(labels[d.status]||'État inconnu'):(labels[d.status]||'État inconnu'))+(d.valid?' · '+(d.code>=95?'Orage':[71,73,75,77,85,86].includes(d.code)?'Neige':[45,48].includes(d.code)?'Brouillard':d.code>=51?'Pluie':d.code>=2?'Nuageux':d.day?'Dégagé':'Nuit dégagée')+' · '+(d.temperature/10).toFixed(1)+' °C (actuel) · il y a '+Math.floor(d.age/60)+' min':' · thème habituel');}function show(d){saved=d;status(d);++searchId;results.hidden=true;searchStatus.textContent='';chosen=null;city.value='';try{const p=JSON.parse(localStorage.getItem('wordclock.weather.place')||'null');if(p&&p.lat===d.lat&&p.lon===d.lon&&typeof p.name==='string'){chosen=p;city.value=p.name;}}catch(e){}el('weatherLat').value=d.lat/100000;el('weatherLon').value=d.lon/100000;el('weatherEnabled').checked=!!d.enabled;for(const k of ['Forecast','Zone','Intensity'])el('weather'+k).value=d[k.toLowerCase()];dirty=false;preview();}async function run(fn){if(busy)return;busy=true;fields.disabled=true;try{await fn();}catch(e){msg.textContent='Action non confirmée : '+e.message;}finally{busy=false;fields.disabled=!saved;}}const post=body=>request('/api/weather',{method:'POST',headers:{'X-Wordclock-Action':'weather'},body:new URLSearchParams(body)});function load(){run(async()=>{show(await request('/api/weather'));msg.textContent='';});}document.addEventListener('wordclock:settings-section',e=>{if(e.detail==='weather'&&!dirty)load();});for(const k of keys)el('weather'+k).addEventListener('input',()=>{dirty=true;preview();msg.textContent='Modifications non enregistrées.';});el('weatherDemo').addEventListener('change',preview);el('weatherCancel').addEventListener('click',()=>{if(saved)show(saved);msg.textContent='Modifications annulées.';});el('weatherSave').addEventListener('click',()=>run(async()=>{const lat=el('weatherLat'),lon=el('weatherLon');if(!lat.value||!lon.value||!lat.checkValidity()||!lon.checkValidity())throw Error('Coordonnées invalides');const data={lat:Math.round(Number(lat.value)*100000),lon:Math.round(Number(lon.value)*100000),enabled:el('weatherEnabled').checked?1:0,forecast:Number(el('weatherForecast').value),zone:Number(el('weatherZone').value),intensity:Number(el('weatherIntensity').value)};const d=await post(data);if(!Object.keys(data).every(k=>d[k]===data[k]))throw Error('Sauvegarde non confirmée');try{if(chosen&&chosen.lat===data.lat&&chosen.lon===data.lon)localStorage.setItem('wordclock.weather.place',JSON.stringify(chosen));else localStorage.removeItem('wordclock.weather.place');}catch(e){}show(d);msg.textContent='Réglages enregistrés.';}));el('weatherRefresh').addEventListener('click',()=>run(async()=>{if(dirty)throw Error('Enregistrez ou annulez les modifications');if(!saved.enabled)throw Error('Activez le mode météo');status(await post({refresh:1}));msg.textContent='Actualisation demandée. Le résultat sera affiché automatiquement.';}));setInterval(()=>{if(!busy&&!document.hidden&&!el('settingsView').hidden&&!el('settings-weather').hidden)run(async()=>{const d=await request('/api/weather');status(d);if(!dirty)saved=d;});},15000);})();
