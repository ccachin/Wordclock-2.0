(() => {
    'use strict';
    const byId=id=>document.getElementById(id), bridge=window.wordclockCommunity;
    const base='https://ccachin.github.io/wordclock-themes/';
    let entries=[], selected=null, current=null, generation=0, selection=0, locked=false;
    const visible=()=>!byId('communityView').hidden;
    const status=message=>byId('communityActionStatus').textContent=message;
    function controls(){
        byId('communityAdd').disabled=locked||!current;
        byId('communityEdit').disabled=locked||!current;
        byId('communityReload').disabled=locked;
    }
    async function json(path,limit){
        const controller=new AbortController(), timer=setTimeout(()=>controller.abort(),10000);
        try{
            const r=await fetch(base+path,{signal:controller.signal,cache:'no-store',credentials:'omit',redirect:'error'});
            if(!r.ok)throw new Error('HTTP '+r.status);
            const raw=await r.arrayBuffer();if(raw.byteLength>limit)throw new Error('Fichier trop volumineux');
            return JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(raw));
        }finally{clearTimeout(timer);}
    }
    function render(){
        const q=byId('communitySearch').value.trim().toLowerCase();
        const list=byId('communityList');list.replaceChildren();
        const filtered=entries.filter(t=>(t.name+' '+t.author+' '+t.description).toLowerCase().includes(q));
        for(const t of filtered){
            const card=document.createElement('button');card.type='button';card.className='theme-card community-card';card.setAttribute('aria-pressed',String(t===selected));card.disabled=locked;
            const preview=document.createElement('div');preview.className='theme-preview';const v=window.WordclockStudioVisual;
            const level=n=>n>0?0.45+0.55*Math.pow(n/100,0.4):0;
            preview.style.setProperty('--front',t.frontBrightness===0?'var(--face-unlit,#393939)':v.css(v.rgb(t.front).map(n=>n*level(t.frontBrightness))));
            preview.style.setProperty('--rear',v.css(v.rgb(t.rear).map(n=>n*level(t.rearBrightness))));
            if(t.rearBrightness===0)preview.style.boxShadow='none';
            window.WordclockStudioVisual.matrixPreview(preview);
            const title=document.createElement('strong');title.textContent=t.name;
            const author=document.createElement('small');author.textContent=t.author;
            const colors=document.createElement('div');colors.className='theme-colors';for(const c of [t.front,t.rear]){const dot=document.createElement('span');dot.className='color-dot';dot.style.backgroundColor=c;colors.append(dot);}
            card.append(preview,title,author,colors);card.addEventListener('click',()=>choose(t));list.append(card);
        }
        byId('communityStatus').textContent=filtered.length+' / '+entries.length+' thème(s).';
    }
    function reset(){selection++;selected=null;current=null;bridge.clear();byId('communityDetails').hidden=true;byId('communityPreviewStatus').textContent='';status('');controls();}
    async function load(){
        if(locked)return;const seq=++generation;reset();entries=[];render();byId('communityReload').disabled=true;byId('communityStatus').textContent='Chargement du catalogue…';
        try{
            const data=await json('catalog.json',262144);
            if(data.format!=='wordclock-community-catalog'||data.version!==1||!Array.isArray(data.themes)||data.themes.length>500)throw new Error('Catalogue incompatible');
            const validated=data.themes.map(t=>{
                if(!t||!['name','author','description','front','rear','id','path'].every(k=>typeof t[k]==='string')||!/^themes\/[a-z0-9-]+\.json$/.test(t.path)||!/^#[0-9a-f]{6}$/i.test(t.front)||!/^#[0-9a-f]{6}$/i.test(t.rear))throw new Error('Entrée de catalogue invalide');
                const levels={};for(const key of ['frontBrightness','rearBrightness']){const n=t[key]===undefined?100:t[key];if(!Number.isInteger(n)||n<0||n>100)throw new Error('Luminosité du catalogue invalide');levels[key]=n;}
                return {...t,...levels};
            });
            if(seq!==generation||!visible())return;entries=validated;render();
        }catch(e){if(seq===generation&&visible())byId('communityStatus').textContent='Catalogue indisponible : '+e.message;}
        finally{if(seq===generation)controls();}
    }
    async function choose(t){
        if(locked)return;const seq=++selection;selected=t;current=null;bridge.clear();render();controls();byId('communityDetails').hidden=true;status('Chargement du thème…');
        try{
            const raw=await json(t.path,8192);if(raw.id!==t.id)throw new Error('Identifiant incohérent');
            const theme=bridge.validate(raw);
            if(seq!==selection||!visible())return;
            current=theme;byId('communityName').textContent=theme.name;byId('communityDescription').textContent=theme.description;byId('communityBrightness').textContent='Avant : '+theme.frontBrightness+' % · Arrière : '+theme.rearBrightness+' %';byId('communityExisting').textContent='';byId('communityDetails').hidden=false;status('');bridge.preview(theme);controls();
            try{const info=await bridge.inspect(theme);if(seq===selection&&visible())byId('communityExisting').textContent=info.existing?'Déjà dans ma bibliothèque':'';}catch(e){if(seq===selection&&visible())byId('communityExisting').textContent='Bibliothèque non vérifiée.';}
        }catch(e){if(seq===selection&&visible())status('Thème indisponible : '+e.message);}
    }
    async function action(edit){
        if(!current||locked)return;locked=true;controls();render();status('Traitement…');
        try{const result=await (edit?bridge.edit(current):bridge.add(current));if(visible()){status(result||'');byId('communityExisting').textContent='Dans ma bibliothèque';}}
        catch(e){if(visible())status('Erreur : '+e.message+' — vérifiez la bibliothèque avant de réessayer.');}
        finally{locked=false;controls();render();}
    }
    byId('communitySearch').addEventListener('input',render);
    byId('communityReload').addEventListener('click',load);
    byId('communityAdd').addEventListener('click',()=>action(false));
    byId('communityEdit').addEventListener('click',()=>action(true));
    document.addEventListener('wordclock:community-open',load);
    document.addEventListener('wordclock:community-close',()=>{generation++;selection++;current=null;selected=null;byId('communityDetails').hidden=true;controls();});
    controls();
})();
