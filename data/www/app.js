async function updateStatus() {

    const statusElement =
        document.getElementById("status");

    const timeElement =
        document.getElementById("time");

    try {

        const response = await fetch("/api/status", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error("HTTP error");
        }

        const data = await response.json();

        timeElement.textContent =
            data.timeValid ? data.time : "--:--:--";

        statusElement.textContent = "Connectée";
        statusElement.style.color = "#70c596";

    } catch (error) {

        statusElement.textContent = "Connexion perdue";
        statusElement.style.color = "#e47b7b";

    } finally {

        setTimeout(updateStatus, 1000);

    }
}

updateStatus();

const frontColor =
    document.getElementById("frontColor");

const rearColor =
    document.getElementById("rearColor");

const brightness =
    document.getElementById("brightness");

const brightnessValue =
    document.getElementById("brightnessValue");

let themeTimer = null;
let sending = false;
let pending = false;

frontColor.disabled = true;
rearColor.disabled = true;
brightness.disabled = true;

function updateBrightnessLabel() {

    brightnessValue.textContent =
        Math.round(brightness.value / 255 * 100)
        + " %";
}

async function loadTheme() {

    try {

        const response = await fetch("/api/theme", {
            cache: "no-store"
        });

        if (!response.ok) {
            throw new Error("HTTP error");
        }

        const data = await response.json();

        frontColor.value = data.front;
        rearColor.value = data.rear;
        brightness.value = data.brightness;

        updateBrightnessLabel();

        frontColor.disabled = false;
        rearColor.disabled = false;
        brightness.disabled = false;

    } catch (error) {

        console.error("Cannot load theme", error);
    }
}

async function sendTheme() {

    if (sending) {
        pending = true;
        return;
    }

    sending = true;

    const params = new URLSearchParams({
        front: frontColor.value,
        rear: rearColor.value,
        brightness: brightness.value
    });

    try {

        const response = await fetch("/api/theme", {
            method: "POST",
            body: params
        });

        if (!response.ok) {
            throw new Error("HTTP error");
        }

    } catch (error) {

        console.error("Cannot update theme", error);

    } finally {

        sending = false;

        if (pending) {
            pending = false;
            sendTheme();
        }
    }
}

function scheduleThemeUpdate() {

    updateBrightnessLabel();

    clearTimeout(themeTimer);

    themeTimer = setTimeout(sendTheme, 100);
}

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

loadTheme();