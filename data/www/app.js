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

async function request(url, options = {}) {

    const response = await fetch(url, {

        cache: "no-store",

        ...options

    });

    if (!response.ok) {

        throw new Error(
            "HTTP " + response.status
        );
    }

    return response.json();
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

async function updateStatus() {

    try {

        const data =
            await request("/api/status");

        timeElement.textContent =
            data.timeValid
                ? data.time
                : "--:--:--";

        statusElement.textContent =
            "Connectée";

        statusElement.style.color =
            "var(--success)";

    } catch (error) {

        statusElement.textContent =
            "Connexion perdue";

        statusElement.style.color =
            "var(--error)";

        timeElement.textContent =
            "--:--:--";

    } finally {

        setTimeout(updateStatus, 1000);
    }
}


// ======================================================
// User controls
// ======================================================

function setControlsEnabled(enabled) {

    frontColor.disabled = !enabled;

    rearColor.disabled = !enabled;

    brightness.disabled = !enabled;

    harmony.disabled = !enabled;
    for (const c of Object.values(animationControls)) { c.effect.disabled = !enabled; c.speed.disabled = !enabled; }

    themeList
        .querySelectorAll(".theme-card")
        .forEach(button => {

            button.disabled = !enabled;
        });
}

function updateBrightnessLabel() {

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

    if (harmony.value === "manual") {

        selected = themes.find(theme =>

            theme.front.toLowerCase() ===
            frontColor.value.toLowerCase() &&

            theme.rear.toLowerCase() ===
            rearColor.value.toLowerCase() &&
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

        preview.style.setProperty(
            "--front",
            theme.front
        );

        preview.style.setProperty(
            "--rear",
            theme.rear
        );

        const previewText =
            document.createElement("span");

        previewText.textContent =
            "IL EST\nHUIT\nHEURES";

        preview.appendChild(previewText);

        const name =
            document.createElement("span");

        name.className = "theme-name";

        name.textContent = theme.name;

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

        themeList.appendChild(button);
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
            await request("/api/themes");

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

        return;
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

updateStatus();

initialize();

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
