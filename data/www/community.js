(() => {
    'use strict';
    const byId = id => document.getElementById(id);
    // Community module: browser-only, no device writes.
    let communityRequest=0;
    async function loadCommunity(){
        const seq=++communityRequest, status=byId('communityStatus'), list=byId('communityList');
        const reload=byId('communityReload');
        reload.disabled=true; list.replaceChildren(); status.textContent='Chargement du catalogue…';
        const controller=new AbortController(); const timer=setTimeout(()=>controller.abort(),10000);
        try {
            const response=await fetch('https://ccachin.github.io/wordclock-themes/catalog.json',{signal:controller.signal,cache:'no-store',credentials:'omit'});
            if(!response.ok)throw new Error('HTTP '+response.status);
            const raw=await response.text(); if(raw.length>262144)throw new Error('Catalogue trop volumineux');
            const data=JSON.parse(raw);
            if(data.format!=='wordclock-community-catalog'||data.version!==1||!Array.isArray(data.themes)||data.themes.length>500)throw new Error('Catalogue incompatible');
            const fragment=document.createDocumentFragment();
            for(const t of data.themes){
                if(!t||typeof t.name!=='string'||typeof t.author!=='string'||typeof t.description!=='string'||typeof t.front!=='string'||typeof t.rear!=='string'||!/^#[0-9a-f]{6}$/i.test(t.front)||!/^#[0-9a-f]{6}$/i.test(t.rear))throw new Error('Entrée de catalogue invalide');
                const card=document.createElement('article');card.className='community-card';
                const colors=document.createElement('div');colors.className='community-colors';
                for(const color of [t.front,t.rear]){const dot=document.createElement('span');dot.style.backgroundColor=color;colors.appendChild(dot);}
                const title=document.createElement('strong');title.textContent=t.name;
                const author=document.createElement('small');author.textContent=t.author;
                const desc=document.createElement('p');desc.textContent=t.description;
                card.append(colors,title,author,desc);fragment.appendChild(card);
            }
            if(seq===communityRequest){list.replaceChildren(fragment);status.textContent=data.themes.length+' thème(s) disponible(s).';}
        }catch(e){if(seq===communityRequest)status.textContent='Catalogue indisponible : '+(e.name==='AbortError'?'délai dépassé':e.message)+'. Réessaie plus tard.';}
        finally{clearTimeout(timer);if(seq===communityRequest)reload.disabled=false;}
    }
    byId('communityReload').addEventListener('click',loadCommunity);
    document.addEventListener('wordclock:community-open', loadCommunity);
})();
