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