
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

function updateControls(data) {

    frontColor.value =
        data.front.toLowerCase();

    rearColor.value =
        data.rear.toLowerCase();

    brightness.value =
        data.brightness;

    updateBrightnessLabel();

    updateActiveTheme();
}


// ======================================================
// Active theme detection
// ======================================================

function updateActiveTheme() {

    const selected = themes.find(theme =>

        theme.front.toLowerCase() ===
        frontColor.value.toLowerCase() &&

        theme.rear.toLowerCase() ===
        rearColor.value.toLowerCase()
    );

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

        preview.className =
            "theme-preview";

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
// Load initial settings
// ======================================================

async function initialize() {

    setControlsEnabled(false);

    try {

        const data =
            await request("/api/theme");

        updateControls(data);

        themes =
            await request("/api/themes");

        initialized = true;

        renderThemeLibrary();

        setControlsEnabled(true);

        showMessage("");

    } catch (error) {

        showMessage(
            "Impossible de charger les paramètres.",
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

    const params = new URLSearchParams({

        front: frontColor.value,

        rear: rearColor.value,

        brightness: brightness.value
    });

    currentRequest = request(
        "/api/theme",
        {
            method: "POST",
            body: params
        }
    )

        .then(() => {

            showMessage(
                "Réglages appliqués"
            );
        })

        .catch(error => {

            showMessage(
                "Impossible d'appliquer les réglages.",
                true
            );
        })

        .finally(() => {

            currentRequest = null;

            if (pending && !applyingTheme) {

                sendTheme();
            }
        });
}

function scheduleThemeUpdate() {

    updateBrightnessLabel();

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

    clearTimeout(themeTimer);

    pending = false;

    setControlsEnabled(false);

    // Wait for an existing manual update.

    if (currentRequest) {

        await currentRequest;
    }

    try {

        const params =
            new URLSearchParams({ id });

        // Preserve the latest brightness selected
        // in the interface, including pending changes.

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
                body: params
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


// ======================================================
// Start
// ======================================================

updateStatus();

initialize();
