// Local navigation. Full theme editor remains in studio-legacy/.
(() => {
    'use strict';
    const el=id=>document.getElementById(id);
    for(const [id,view] of [['homeTab','homeView'],['themesTab','libraryView'],['settingsTab','settingsView']]) {
        el(id).addEventListener('click',()=>{
            for(const v of ['homeView','libraryView','settingsView'])el(v).hidden=v!==view;
            for(const tab of ['homeTab','themesTab','settingsTab'])el(tab).setAttribute('aria-pressed',String(tab===id));
        });
    }
})();
