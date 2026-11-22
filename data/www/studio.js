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
    libraryTab.addEventListener('click', () => { if (working || !confirmDiscard()) return; disableLive(); resetDraft(); showStudio(false); });
    studioTab.addEventListener('click', () => { showStudio(true); refreshSources(); });
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
    const rgb = hex => [1,3,5].map(i => parseInt(hex.slice(i,i+2),16));
    const css = c => '#' + c.map(x => Math.max(0,Math.min(255,Math.round(x))).toString(16).padStart(2,'0')).join('');
    function hsv(c) {
        const [r,g,b] = c.map(x=>x/255), v=Math.max(r,g,b), d=v-Math.min(r,g,b);
        let h=0;
        if(d) h=60*(v===r?(g-b)/d:v===g?(b-r)/d+2:(r-g)/d+4);
        return [(h+360)%360,v?d/v:0,v];
    }
    function fromHsv(h,s,v) {
        h=((h%360)+360)%360;
        const c=v*s,x=c*(1-Math.abs(h/60%2-1)),m=v-c;
        return [[c,x,0],[x,c,0],[0,c,x],[0,x,c],[x,0,c],[c,0,x]][Math.floor(h/60)].map(n=>Math.round((n+m)*255));
    }
    function rotate(c,degrees) { const [h,s,v]=hsv(c); return fromHsv(h+degrees,s,v); }
    const harmony=byId('studioHarmony');
    const segments=[];
    const svg=rear.querySelector('svg');
    // Five trapezoids on each side, bottom, left, top, right.
    for(let side=0;side<4;side++) {
        for(let i=0;i<5;i++) {
            const path=document.createElementNS('http://www.w3.org/2000/svg','path');
            const a=i*20,b=(i+1)*20,c=10+i*16,d=10+(i+1)*16;
            path.setAttribute('d',`M ${100-a} 100 L ${100-b} 100 L ${100-d} 90 L ${100-c} 90 Z`);
            path.setAttribute('transform',`rotate(${side*90} 50 50)`);
            svg.appendChild(path); segments.push(path);
        }
    }
    const controls=['Front','Rear'].map(face=>({effect:byId(`studio${face}Effect`),speed:byId(`studio${face}Speed`),row:byId(`studio${face}SpeedRow`),value:byId(`studio${face}SpeedValue`),period:byId(`studio${face}Period`),start:performance.now()}));
    const period=c=>30000-290*Number(c.speed.value);
    function sync(c) {
        c.row.hidden=c.effect.value==='solid';
        c.value.textContent=c.speed.value+' %';
        c.period.textContent=`Période : ${(period(c)/1000).toFixed(1)} s`;
    }
    function effect(base,c,now,i=0) {
        const t=((now-c.start)%period(c))/period(c);
        const phase=Math.floor(t*256)/256;
        let level=255;
        switch(c.effect.value) {
            case 'solid': return base;
            case 'breathing': level=40+215*(1+Math.cos(phase*2*Math.PI))/2; break;
            case 'color-cycle': return rotate(base,t*360);
            case 'rainbow': return fromHsv((Math.floor(i*256/20)/256-phase)*360,1,Math.max(...base)/255);
            case 'chase': level=12+243*Math.pow((1+Math.cos((Math.floor(i*256/20)/256-phase)*2*Math.PI))/2,4); break;
        }
        return base.map(n=>n?Math.min(255,Math.floor(n*Math.floor(level)/256)+1):0);
    }
    function bases() {
        const f=rgb(byId('studioFrontColor').value), r=rgb(byId('studioRearColor').value);
        const offsets={identical:0,complementary:180,'analogous-plus':30,'analogous-minus':-30,triadic:120};
        return [f,harmony.value==='manual'?r:rotate(f,offsets[harmony.value])];
    }
    function render(now) {
        const [f,r]=bases();
        front.style.setProperty('--studio-front',css(effect(f,controls[0],now)));
        segments.forEach((p,i)=>{const col=css(effect(r,controls[1],now,i));p.setAttribute('fill',col);p.setAttribute('stroke',col);p.style.filter=`drop-shadow(0 0 2px ${col})`;});
    }
    function updatePreview() {
        const manual=harmony.value==='manual';
        byId('studioManualRear').hidden=!manual;
        byId('studioAutoRear').hidden=manual;
        byId('studioEffectiveRear').style.backgroundColor=css(bases()[1]);
        render(performance.now());
    }
    controls.forEach(c=>{for(const el of [c.effect,c.speed]) el.addEventListener('input',()=>{c.start=performance.now();sync(c);updatePreview();});sync(c);});
    for(const id of ['studioFrontColor','studioRearColor','studioHarmony']) byId(id).addEventListener('input',updatePreview);
    const liveButton = byId('studioLive'), liveStatus = byId('studioLiveStatus');
    let live = false, busy = false, pending = false, stopping = false, token = '', lastSent = 0;
    function displayLive(message) {
        liveButton.setAttribute('aria-pressed', String(live));
        liveButton.textContent = live ? 'Désactiver l’aperçu physique' : 'Activer l’aperçu physique';
        liveButton.disabled = stopping;
        liveStatus.textContent = message;
    }
    function payload(action) {
        const p = new URLSearchParams({action, token});
        if(action === 'set') for(const [key,id] of Object.entries({front:'FrontColor',rear:'RearColor',harmony:'Harmony',frontEffect:'FrontEffect',rearEffect:'RearEffect',frontSpeed:'FrontSpeed',rearSpeed:'RearSpeed'})) p.set(key,byId('studio'+id).value);
        return p;
    }
    async function requestPreview(action) {
        const controller = new AbortController();
        const timeout = setTimeout(()=>controller.abort(),3000);
        try {
            const response = await fetch('/api/preview',{method:'POST',body:payload(action),signal:controller.signal,cache:'no-store'});
            if(!response.ok) throw new Error(response.status===409?'Aperçu utilisé ailleurs ou indisponible':`HTTP ${response.status}`);
            if(!(await response.json()).ok) throw new Error('Réponse invalide');
        } finally { clearTimeout(timeout); }
    }
    async function flush() {
        if(busy) return;
        busy=true;
        try {
            if(live && (pending || performance.now()-lastSent>=2000) && performance.now()-lastSent>=100) {
                pending=false;lastSent=performance.now();
                await requestPreview('set');
                if(live) displayLive('Aperçu physique actif (luminosité et mode nuit conservés).');
            }
        } catch(e) {
            live=false;stopping=true;displayLive('Connexion interrompue : restauration automatique sous 10 s après la dernière requête reçue.');
        } finally {
            if(stopping) {
                try {await requestPreview('stop');displayLive('Ambiance précédente restaurée.');}
                catch(e){displayLive('Restauration automatique sous 10 s après la dernière requête reçue.');}
                stopping=false;liveButton.disabled=false;
            }
            busy=false;
        }
    }
    function disableLive() {
        if(!live) return;
        live=false;pending=false;stopping=true;displayLive('Restauration en cours…');flush();
    }
    // Stop must also run when no set is pending.
    liveButton.addEventListener('click',()=>{
        if(live){disableLive();return;}
        if(stopping||busy)return;
        token=Array.from(crypto.getRandomValues(new Uint8Array(16)),b=>b.toString(16).padStart(2,'0')).join('');
        live=true;pending=true;lastSent=-1000;displayLive('Connexion…');flush();
    });
    studio.addEventListener('input',()=>{if(live)pending=true;});
    setInterval(()=>{if(live||stopping)flush();},100);
    document.addEventListener('visibilitychange',()=>{if(document.hidden)disableLive();});
    window.addEventListener('pagehide',()=>{if(live||stopping)navigator.sendBeacon('/api/preview',payload('stop'));live=false;});
    // Skip drawing while the editor is hidden.
    let last=0;
    function frame(now) { if(!studio.hidden && now-last>=40) {render(now);last=now;} requestAnimationFrame(frame); }
    // Theme persistence is independent from the preview session.
    const fields = {name:'Name',author:'Author',description:'Description',front:'FrontColor',rear:'RearColor',harmony:'Harmony',frontEffect:'FrontEffect',rearEffect:'RearEffect',frontSpeed:'FrontSpeed',rearSpeed:'RearSpeed'};
    const source=byId('studioSource'), status=byId('studioSaveStatus');
    let editing='', working=false, catalog=[], catalogReady=false;
    function draft() {
        const data={version:2};
        for(const [key,suffix] of Object.entries(fields)) {
            const value=byId('studio'+suffix).value;
            data[key]=key.endsWith('Speed')?Number(value):value;
        }
        return data;
    }
    const defaults=draft(); let baseline=JSON.stringify(defaults);
    const dirty=()=>JSON.stringify(draft())!==baseline;
    function confirmDiscard(){return !dirty()||window.confirm('Abandonner les modifications non enregistrées ?');}
    function buttons(){
        const personal=source.value.startsWith('custom-');
        for(const id of ['studioNew','studioSave','studioCancel','studioSource','studioDelete']) byId(id).disabled=working;
        byId('studioEdit').disabled=working||!catalogReady||!personal;
        byId('studioCopy').disabled=working||!catalogReady||!source.value;
        byId('studioDelete').disabled=working||!catalogReady||!personal;
        byId('studioMode').textContent=(editing?'Modification d’un thème personnel':'Nouveau thème')+(dirty()?' — non enregistré':'');
        for(const suffix of Object.values(fields)) byId('studio'+suffix).disabled=working;
    }
    function loadDraft(data,id=''){
        editing=id;
        for(const [key,suffix] of Object.entries(fields)) byId('studio'+suffix).value=data[key]??defaults[key];
        controls.forEach(c=>{c.start=performance.now();sync(c);});updatePreview();
        baseline=JSON.stringify(draft());if(live)pending=true;buttons();
    }
    function resetDraft(){loadDraft(defaults);source.value='';status.textContent='';buttons();}
    async function refreshSources(){
        catalogReady=false;buttons();
        try {
            await refreshThemeLibrary();catalog=themes.slice();
            const selected=source.value;source.replaceChildren(new Option('Choisir un thème',''));
            for(const t of catalog) source.add(new Option(t.name+(t.id.startsWith('custom-')?' (Perso)':' (Usine)'),t.id));
            source.value=catalog.some(t=>t.id===selected)?selected:'';catalogReady=true;buttons();return true;
        } catch(e){status.textContent='Bibliothèque indisponible : '+e.message;buttons();return false;}
    }
    async function api(method,id,data){
        const response=await fetch('/api/themes/custom'+(id?'?id='+encodeURIComponent(id):''),{method,cache:'no-store',...(data?{headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}:{})});
        const result=await response.json();if(!response.ok)throw new Error(result.error||'HTTP '+response.status);return result;
    }
    function validate(data){
        data.name=data.name.trim();data.author=data.author.trim();data.description=data.description.trim();
        if(!data.name)throw new Error('Indiquez un nom.');
        for(const [key,max] of [['name',48],['author',48],['description',200]]) {
            if(new TextEncoder().encode(data[key]).length>max)throw new Error('Texte trop long : '+key+' (limite '+max+' octets UTF-8).');
            if(/[\x00-\x1f\x7f]/.test(data[key]))throw new Error('Les caractères de contrôle et retours à la ligne ne sont pas acceptés.');
        }
    }
    async function operation(fn){if(working)return;working=true;buttons();status.textContent='';try{await fn();}catch(e){status.textContent='Erreur : '+e.message;}finally{working=false;buttons();}}
    source.addEventListener('change',buttons);
    studio.addEventListener('input',buttons);
    byId('studioNew').addEventListener('click',()=>{if(confirmDiscard()){disableLive();resetDraft();}});
    byId('studioCancel').addEventListener('click',()=>{if(confirmDiscard()){disableLive();resetDraft();}});
    for(const copy of [false,true]) byId(copy?'studioCopy':'studioEdit').addEventListener('click',()=>{
        if(!confirmDiscard())return;
        const t=catalog.find(t=>t.id===source.value);if(!t||(!copy&&!t.id.startsWith('custom-')))return;
        disableLive();loadDraft(copy?{...t,name:''}:t,copy?'':t.id);status.textContent=copy?'Donnez un nom à la copie.':'';
    });
    byId('studioSave').addEventListener('click',()=>operation(async()=>{
        const data=draft();validate(data);if(editing)data.id=editing;
        let saved;try{saved=await api(editing?'PUT':'POST',editing,data);}catch(e){throw new Error(e.message+' — vérifiez la bibliothèque avant de réessayer.');}
        if(!saved.id||!saved.id.startsWith('custom-'))throw new Error('Réponse inattendue : vérifiez la bibliothèque avant de réessayer.');
        disableLive();loadDraft(saved,saved.id);
        const refreshed=await refreshSources();if(refreshed)source.value=saved.id;
        while(busy||stopping) await new Promise(resolve=>setTimeout(resolve,50));
        const applied=await applyTheme(saved.id);
        status.textContent='Thème enregistré. '+(applied?'Thème appliqué.':'Application non confirmée : réessayez depuis la bibliothèque.')+(refreshed?'':' Actualisation de la bibliothèque impossible.');
    }));
    byId('studioDelete').addEventListener('click',()=>{
        const id=source.value, target=catalog.find(t=>t.id===id);
        if(!catalogReady||!target||!id.startsWith('custom-'))return;
        if(!window.confirm('Supprimer définitivement « '+target.name+' » ?'))return;
        if(editing===id&&!confirmDiscard())return;
        operation(async()=>{const result=await api('DELETE',id);if(!result.ok)throw new Error('Suppression non confirmée');if(editing===id){disableLive();resetDraft();}const refreshed=await refreshSources();status.textContent=refreshed?'Thème supprimé.':'Thème supprimé, actualisation impossible.';});
    });
    window.addEventListener('beforeunload',e=>{if(dirty()||working){e.preventDefault();e.returnValue='';}});
    buttons();
    updatePreview();requestAnimationFrame(frame);
})();
