(() => {
    'use strict';
    const byId = id => document.getElementById(id);
    const libraryTab = byId('libraryTab');
    const studioTab = byId('studioTab');
    const library = byId('libraryView');
    const studio = byId('studioView');
    function showStudio(active) {
        library.hidden = active;
        studio.hidden = !active;
        libraryTab.setAttribute('aria-pressed', String(!active));
        studioTab.setAttribute('aria-pressed', String(active));
    }
    libraryTab.addEventListener('click', () => showStudio(false));
    studioTab.addEventListener('click', () => showStudio(true));
    const matrix = ['ILAESTBDEUX','QUATREDCINQ','SIXNEUFONZE','UNEHSEPTDIX','HUITFTROISZ','UMIDIHEURES','MINUITSETFA','MOINSOVINGT','DIXTLESDEMI','QUARTUCINQO','WGBSDLO****'];
    // Static example: IL EST HUIT HEURES. No device writes.
    const lit = new Set([0,1,3,4,5,44,45,46,47,60,61,62,63,64,65]);
    const front = byId('studioFrontPreview');
    const rear = byId('studioRearPreview');
    Array.from(matrix.join('')).forEach((letter, index) => {
        const cell = document.createElement('span');
        cell.textContent = letter;
        if (lit.has(index)) cell.classList.add('lit');
        front.appendChild(cell);
    });
    function updatePreview() {
        front.style.setProperty('--studio-front', byId('studioFrontColor').value);
        rear.style.setProperty('--studio-rear', byId('studioRearColor').value);
    }
    byId('studioFrontColor').addEventListener('input', updatePreview);
    byId('studioRearColor').addEventListener('input', updatePreview);
    updatePreview();
})();
