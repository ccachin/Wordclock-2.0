(() => {
    'use strict';
    const byId = id => document.getElementById(id);
    const libraryTab = byId('libraryTab');
    const studioTab = byId('studioTab');
    const library = byId('libraryView');
    const studio = byId('studioView');
    const communityTab = byId('communityTab'), community = byId('communityView');
    function showStudio(active, online=false, destination="themes") {
        const page=active||online?"themes":destination;
        byId("homeView").hidden=page!=="home";
        byId("settingsView").hidden=page!=="settings";
        byId("themesNav").hidden=page!=="themes";
        for(const [id,target] of [["homeTab","home"],["themesTab","themes"],["settingsTab","settings"]])byId(id).setAttribute("aria-pressed",String(page===target));
        if(!online){communityPreview=null; document.dispatchEvent(new Event("wordclock:community-close"));}
        library.hidden = page!=="themes" || online || active;
        community.hidden = !online;
        communityTab.setAttribute('aria-pressed', String(online));
        studio.hidden = !active;
        libraryTab.setAttribute('aria-pressed', String(page==="themes" && !online && !active));
        studioTab.setAttribute('aria-pressed', String(active));
    }
    libraryTab.addEventListener('click', () => { if (working || !confirmDiscard()) return; previewWanted=false; disableLive(); resetDraft(); showStudio(false); });
    for(const [id,destination] of [['homeTab','home'],['settingsTab','settings'],['themesTab','themes']])byId(id).addEventListener('click',()=>{
        if(working||byId(id).getAttribute('aria-pressed')==='true'||!confirmDiscard())return;
        previewWanted=false; disableLive(); resetDraft(); showStudio(false,false,destination);
    });
    studioTab.addEventListener('click', () => { if(working||!studio.hidden)return; previewWanted=false; disableLive(); showStudio(true); operation(loadActive); });
    communityTab.addEventListener('click', () => {
        if(working || !community.hidden || !confirmDiscard())return;
        previewWanted=false; disableLive(); resetDraft(); showStudio(false,true); document.dispatchEvent(new Event('wordclock:community-open'));
    });
    const {controls,sync,render,updatePreview}=window.WordclockStudioVisual;
    const {validate,validateImport,uniqueName,fingerprint,planImport,prepareSave}=window.WordclockThemeRules;
    const liveStatus = byId('studioLiveStatus');
    let communityPreview=null;
    let live = false, busy = false, pending = false, stopping = false, token = '', lastSent = 0;
    function displayLive(message) {
        liveStatus.textContent = message;
        if(!community.hidden)byId('communityPreviewStatus').textContent=message;
    }
    function payload(action) {
        const p = new URLSearchParams({action, token});
        if(action === 'set' && !community.hidden && communityPreview){for(const key of ['front','rear','harmony','frontEffect','rearEffect','frontSpeed','rearSpeed','frontBrightness','rearBrightness'])p.set(key,communityPreview[key]);return p;}
        if(action === 'set') for(const [key,id] of Object.entries({front:'FrontColor',rear:'RearColor',harmony:'Harmony',frontEffect:'FrontEffect',rearEffect:'RearEffect',frontSpeed:'FrontSpeed',rearSpeed:'RearSpeed',frontBrightness:'FrontBrightness',rearBrightness:'RearBrightness'})) p.set(key,byId('studio'+id).value);
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
                if(live) displayLive('');
            }
        } catch(e) {
            retryAt=performance.now()+5000;live=false;stopping=true;displayLive('Connexion interrompue : restauration automatique sous 10 s après la dernière requête reçue.');
        } finally {
            if(stopping) {
                try {await requestPreview('stop');displayLive('Ambiance précédente restaurée.');}
                catch(e){displayLive('Restauration automatique sous 10 s après la dernière requête reçue.');}
                stopping=false;
            }
            busy=false;
        }
    }
    function disableLive() {
        if(!live) return;
        live=false;pending=false;stopping=true;displayLive('Restauration en cours…');flush();
    }
    let previewWanted=false, retryAt=0;
    function startLive(){
        if(!previewWanted||(studio.hidden&&(community.hidden||!communityPreview))||document.hidden||working||live||busy||stopping||performance.now()<retryAt)return;
        token=Array.from(crypto.getRandomValues(new Uint8Array(16)),b=>b.toString(16).padStart(2,'0')).join('');
        live=true;pending=true;lastSent=-1000;displayLive('Connexion…');flush();
    }
    studio.addEventListener('input',()=>{if(live)pending=true;});
    setInterval(()=>{if(live||stopping)flush();else startLive();},100);
    document.addEventListener('visibilitychange',()=>{if(document.hidden)disableLive();else startLive();});
    window.addEventListener('pagehide',()=>{if(live||stopping)navigator.sendBeacon('/api/preview',payload('stop'));live=false;});
    // Skip drawing while the editor is hidden.
    let last=0;
    function frame(now) { if(!studio.hidden && now-last>=40) {render(now);last=now;} requestAnimationFrame(frame); }
    // Theme persistence is independent from the preview session.
    const fields = {name:'Name',author:'Author',description:'Description',front:'FrontColor',rear:'RearColor',harmony:'Harmony',frontEffect:'FrontEffect',rearEffect:'RearEffect',frontSpeed:'FrontSpeed',rearSpeed:'RearSpeed',frontBrightness:'FrontBrightness',rearBrightness:'RearBrightness'};
    const source=byId('studioSource'), status=byId('studioSaveStatus');
    let shareId='', shareData=null;
    let lastSource='', imported=false, fromFile=false, editing='', working=false, catalog=[], catalogReady=false;
    function draft() {
        const data={version:3};
        for(const [key,suffix] of Object.entries(fields)) {
            const value=byId('studio'+suffix).value;
            data[key]=(key.endsWith('Speed')||key.endsWith('Brightness'))?Number(value):value;
        }
        return data;
    }
    const defaults=draft(); let baseline=JSON.stringify(defaults);
    const dirty=()=>imported||JSON.stringify(draft())!==baseline;
    function confirmDiscard(){return !dirty()||window.confirm('Abandonner les modifications non enregistrées ?');}
    function buttons(){
        if(shareId!==source.value||studio.hidden)closeShare();
        lastSource=source.value;
        const personal=source.value.startsWith('custom-');
        for(const id of ['studioNew','studioSave','studioCancel','studioSource','studioDelete','studioImport']) byId(id).disabled=working;
        byId('studioExportAll').disabled=working||!catalogReady||!catalog.some(t=>t.id.startsWith('custom-'));
        byId('studioImportAll').disabled=working||!catalogReady;
        byId('studioCopy').disabled=working||!catalogReady||!source.value;
        byId('studioExport').disabled=working||!catalogReady||!source.value;
        byId('studioShare').disabled=working||!catalogReady||!personal;
        byId('studioShare').title=personal?'Partager la version enregistrée':'Sélectionnez un thème personnel enregistré';
        byId('studioMore').disabled=working;
        byId('studioShareSubmit').disabled=working||!shareData||!byId('studioShareConsent').checked;
        byId('studioDelete').disabled=working||!catalogReady||!personal;
        byId('studioMode').textContent=(editing?'Modification d’un thème personnel':'Nouveau thème')+(dirty()?' — non enregistré':'');
        for(const suffix of Object.values(fields)) byId('studio'+suffix).disabled=working;
    }
    function loadDraft(data,id=''){
        closeShare();setMore(false);
        imported=false;fromFile=false;editing=id;
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
        const response=await fetch('/api/themes/custom?version=3'+(id?'&id='+encodeURIComponent(id):''),{method,cache:'no-store',...(data?{headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}:{})});
        const result=await response.json();if(!response.ok)throw new Error(result.error||'HTTP '+response.status);return result;
    }
    async function operation(fn){if(working)return;working=true;buttons();status.textContent='';try{await fn();}catch(e){status.textContent='Erreur : '+e.message;}finally{working=false;buttons();}}
    function selectTheme(t){
        loadDraft(t,t.id.startsWith('custom-')?t.id:'');source.value=t.id;buttons();
    }
    async function loadActive(){
        previewWanted=false;disableLive();
        const data=await request('/api/theme');
        if(!await refreshSources())throw new Error('Bibliothèque indisponible.');
        const matches=catalog.filter(t=>(t.harmony||'manual')===(data.harmony||'manual')&&['front','rear'].every(f=>t[f].toLowerCase()===data[f].toLowerCase()&&(t[f+'Effect']||'solid')===(data[f+'Effect']||'solid')&&(t[f+'Speed']??50)===(data[f+'Speed']??50)&&(t[f+'Brightness']??100)===(data[f+'Brightness']??100)));
        if(matches.length)selectTheme(matches[0]);else {loadDraft({...defaults,...data});source.value='';buttons();}
        previewWanted=true;
    }
    source.addEventListener('change',()=>{
        const id=source.value,t=catalog.find(t=>t.id===id);
        if(!confirmDiscard()){source.value=lastSource;buttons();return;}
        if(t)selectTheme(t);else resetDraft();
        lastSource=source.value;previewWanted=true;buttons();
    });
    studio.addEventListener('input',buttons);
    byId('studioNew').addEventListener('click',()=>{if(confirmDiscard()){disableLive();resetDraft();previewWanted=true;}});
    byId('studioCancel').addEventListener('click',()=>{if(confirmDiscard())operation(loadActive);});
    function setMore(open){
        byId('studioMorePanel').hidden=!open;
        byId('studioMore').setAttribute('aria-expanded',String(open));
    }
    byId('studioMore').addEventListener('click',()=>setMore(byId('studioMorePanel').hidden));
    byId('studioCopy').addEventListener('click',()=>{
        if(!confirmDiscard())return;
        const t=catalog.find(t=>t.id===source.value);if(!t)return;
        disableLive();loadDraft({...t,name:uniqueName(t.name,catalog)});imported=true;previewWanted=true;buttons();status.textContent='Copie prête à enregistrer.';
    });
    byId('studioSave').addEventListener('click',()=>operation(async()=>{
        const data=draft();validate(data);
        if(!await refreshSources())throw new Error('Impossible de vérifier les doublons.');
        const pool=catalog.filter(t=>t.id!==editing);
        const originalName=data.name;
        const planned=prepareSave(data,pool,fromFile&&!editing);
        if(!planned){status.textContent='Thème identique déjà présent : enregistrement ignoré.';return;}
        data.name=planned.name;
        const renamed=data.name!==originalName;
        if(editing)data.id=editing;
        let saved;try{saved=await api(editing?'PUT':'POST',editing,data);}catch(e){throw new Error(e.message+' — vérifiez la bibliothèque avant de réessayer.');}
        if(!saved.id||!saved.id.startsWith('custom-'))throw new Error('Réponse inattendue : vérifiez la bibliothèque avant de réessayer.');
        disableLive();loadDraft(saved,saved.id);
        const refreshed=await refreshSources();if(refreshed)source.value=saved.id;
        while(busy||stopping) await new Promise(resolve=>setTimeout(resolve,50));
        const applied=await applyTheme(saved.id);
        if(!applied)previewWanted=false;
        status.textContent=(renamed?'Nom ajusté : '+saved.name+'. ':'')+'Thème enregistré. '+(applied?'Thème appliqué.':'Application non confirmée : réessayez depuis la bibliothèque.')+(refreshed?'':' Actualisation de la bibliothèque impossible.');
    }));
    // Validate portable theme files before touching the current draft.
    const importFile=byId('studioImportFile');
    byId('studioImport').addEventListener('click',()=>{if(!working){importFile.value='';importFile.click();}});
    importFile.addEventListener('change',()=>{
        const file=importFile.files[0];importFile.value='';if(!file)return;
        operation(async()=>{
            if(file.size>8192) throw new Error('Fichier trop volumineux (8 Ko maximum).');
            let data;
            try {data=JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(await file.arrayBuffer()));}
            catch(e){throw new Error('Fichier JSON ou encodage UTF-8 invalide.');}
            const theme=validateImport(data);
            if(!await refreshSources())throw new Error('Bibliothèque indisponible.');
            const planned=planImport(theme,catalog);
            if(!planned){status.textContent='Thème identique déjà présent : import ignoré.';return;}
            if(!confirmDiscard())return;
            disableLive();loadDraft(planned);source.value='';imported=true;fromFile=true;previewWanted=true;
            status.textContent='Thème importé dans le Studio. Cliquez sur Enregistrer et appliquer pour le conserver.';
        });
    });
    function shareTheme(t){const v=validateImport(t);validate(v);return v;}
    function shareBrightness(t){return ' — Avant : '+t.frontBrightness+' % · Arrière : '+t.rearBrightness+' %';}
    function closeShare(){
        shareId='';shareData=null;byId('studioSharePanel').hidden=true;
        byId('studioShareConsent').checked=false;
    }
    byId('studioShare').addEventListener('click',()=>operation(async()=>{
        closeShare();setMore(false);const id=source.value;
        if(!id.startsWith('custom-'))throw new Error('Choisissez un thème personnel enregistré.');
        const t=await api('GET',id);
        if(t.id!==id)throw new Error('Thème indisponible.');
        const data=shareTheme(t);validate(data);
        shareId=id;shareData=data;
        byId('studioShareInfo').textContent='Version enregistrée : '+data.name+' — '+(data.author||'Auteur non renseigné')+(data.description?' — '+data.description:'')+shareBrightness(data);
        byId('studioSharePanel').hidden=false;
    }));
    byId('studioShareConsent').addEventListener('change',buttons);
    byId('studioShareCancel').addEventListener('click',()=>{closeShare();buttons();});
    byId('studioShareSubmit').addEventListener('click',()=>{
        if(working||!shareData||shareId!==source.value||!byId('studioShareConsent').checked)return;
        const json=JSON.stringify(shareData,null,2).replace(/`/g,'\\u0060');
        const body='Licence : CC0 1.0 — Je confirme être autorisé à soumettre ce thème.\n\n```json\n'+json+'\n```';
        const url=new URL('https://github.com/ccachin/wordclock-themes/issues/new');
        url.searchParams.set('title','[Theme] '+shareData.name);url.searchParams.set('body',body);
        window.open(url.href,'_blank','noopener,noreferrer');
        status.textContent='Terminez la soumission sur GitHub. Aucune publication automatique.';
        closeShare();buttons();
    });
    // Export the saved source, never the draft or global clock settings.
    byId('studioExport').addEventListener('click',()=>operation(async()=>{
        const id=source.value;
        if(!catalogReady||!id)throw new Error('Choisissez un thème à exporter.');
        const t=id.startsWith('custom-')?await api('GET',id):catalog.find(t=>t.id===id);
        if(!t||t.id!==id)throw new Error('Thème indisponible.');
        const data={...validateImport(t),id:t.id};
        const slug=t.name.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-+|-+$/g,'')||id;
        const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)+'\n'],{type:'application/json'}));
        const link=document.createElement('a');link.href=url;link.download=slug+'.json';
        try{document.body.appendChild(link);link.click();status.textContent='Export demandé : '+link.download;}
        finally{link.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);}
    }));
    // Collections are processed one theme at a time, never sent as a bulk body.
    const collectionFile=byId('studioCollectionFile');
    byId('studioExportAll').addEventListener('click',()=>operation(async()=>{
        const ids=catalog.filter(t=>t.id.startsWith('custom-')).map(t=>t.id);
        if(!catalogReady||!ids.length)throw new Error('Aucun thème personnel à exporter.');
        const entries=[];
        for(const id of ids){
            status.textContent=`Export : ${entries.length}/${ids.length}`;
            const t=await api('GET',id);
            if(t.id!==id)throw new Error('Identifiant inattendu : '+id);
            entries.push({...validateImport(t),id});
        }
        const data={format:'wordclock-theme-collection',version:1,themes:entries};
        const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)+'\n'],{type:'application/json'}));
        const link=document.createElement('a');link.href=url;link.download='wordclock-themes-'+new Date().toISOString().slice(0,10)+'.json';
        try{document.body.appendChild(link);link.click();status.textContent=`Export demandé : ${entries.length} thème(s).`;}
        finally{link.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);}
    }));
    byId('studioImportAll').addEventListener('click',()=>{if(!working){collectionFile.value='';collectionFile.click();}});
    collectionFile.addEventListener('change',()=>{
        const file=collectionFile.files[0];collectionFile.value='';if(!file)return;
        operation(async()=>{
            if(file.size>256*1024)throw new Error('Collection trop volumineuse (256 Ko maximum).');
            let data;
            try{data=JSON.parse(new TextDecoder('utf-8',{fatal:true}).decode(await file.arrayBuffer()));}
            catch(e){throw new Error('Fichier JSON ou encodage UTF-8 invalide.');}
            if(!data||data.format!=='wordclock-theme-collection'||data.version!==1||!Array.isArray(data.themes)||!data.themes.length||data.themes.length>30)throw new Error('Collection version 1 requise (1 à 30 thèmes).');
            const incoming=data.themes.map((t,i)=>{try{return validateImport(t);}catch(e){throw new Error(`Thème ${i+1} : ${e.message}`);}});
            if(!await refreshSources())throw new Error('Impossible de vérifier la capacité disponible.');
            const pool=catalog.slice(),entries=[];let skipped=0,renamed=0;
            for(const theme of incoming){
                const planned=planImport(theme,pool);
                if(!planned){skipped++;continue;}
                if(planned.name!==theme.name)renamed++;
                entries.push(planned);pool.push(planned);
            }
            if(!entries.length){status.textContent=`${skipped} thème(s) identique(s) ignoré(s).`;return;}
            const available=30-catalog.filter(t=>t.id.startsWith('custom-')).length;
            if(entries.length>available)throw new Error(`Places disponibles : ${available}, thèmes à importer : ${entries.length}.`);
            if(!window.confirm(`Importer ${entries.length} thème(s) comme nouvelles créations, sans les appliquer ? (${skipped} ignoré(s), ${renamed} renommé(s))\n\n`+entries.map(t=>t.name).join('\n'))) {status.textContent='Import annulé.';return;}
            let count=0, failure='';
            for(const entry of entries){
                status.textContent=`Import : ${count}/${entries.length}`;
                try{const saved=await api('POST','',entry);if(!saved.id||!saved.id.startsWith('custom-'))throw new Error('Création non confirmée');count++;}
                catch(e){failure=e.message;break;}
            }
            const refreshed=await refreshSources();
            status.textContent=`${count}/${entries.length} thème(s) importé(s). ${skipped} identique(s) ignoré(s), ${renamed} renommage(s) prévu(s).`+(failure?' Arrêt : '+failure+'. Vérifiez la bibliothèque avant de réessayer.':'')+(refreshed?'':' Actualisation impossible.');
        });
    });
    byId('studioDelete').addEventListener('click',()=>{
        const id=source.value, target=catalog.find(t=>t.id===id);
        if(!catalogReady||!target||!id.startsWith('custom-'))return;
        if(!window.confirm('Supprimer définitivement « '+target.name+' » ?'))return;
        if(editing===id&&!confirmDiscard())return;
        operation(async()=>{const result=await api('DELETE',id);if(!result.ok)throw new Error('Suppression non confirmée');if(editing===id){disableLive();resetDraft();}const refreshed=await refreshSources();status.textContent=refreshed?'Thème supprimé.':'Thème supprimé, actualisation impossible.';});
    });
    // Library actions operate on persisted themes, never on a Studio draft.
    const libDialog=byId('libraryDialog'), libActions=byId('libraryActions'), libMsg=byId('libraryDialogMessage');
    let libShare=null, libBusy=false;
    function libLock(value){libBusy=value;for(const el of libActions.querySelectorAll('button'))el.disabled=value;byId('libraryClose').disabled=value;}
    function libResult(message){byId('libraryMessage').textContent=message;}
    function libClose(){libDialog.close();}
    async function libRun(fn){if(libBusy||working)return;libLock(true);libMsg.textContent='';try{await fn();}catch(e){libMsg.textContent='Erreur : '+e.message;}finally{libLock(false);}}
    async function libEdit(id,copy=false){
        if(!confirmDiscard())return;
        if(!await refreshSources())throw new Error('Bibliothèque indisponible.');
        const t=catalog.find(t=>t.id===id);if(!t)throw new Error('Thème indisponible.');
        previewWanted=false;disableLive();showStudio(true);
        if(copy||!id.startsWith('custom-')){loadDraft({...t,name:uniqueName(t.name,catalog)});source.value='';imported=true;buttons();}else selectTheme(t);
        previewWanted=true;libClose();
    }
    byId('libraryCreate').addEventListener('click',()=>{if(working||!confirmDiscard())return;previewWanted=false;disableLive();resetDraft();showStudio(true);previewWanted=true;operation(async()=>{await refreshSources();});});
    byId('libraryClose').addEventListener('click',libClose);
    libDialog.addEventListener('cancel',e=>{if(libBusy)e.preventDefault();});
    libDialog.addEventListener('close',()=>{libShare=null;byId('libraryShare').hidden=true;byId('libraryConsent').checked=false;byId('librarySubmit').disabled=true;});
    byId('libraryConsent').addEventListener('change',()=>{byId('librarySubmit').disabled=!libShare||!byId('libraryConsent').checked;});
    byId('librarySubmit').addEventListener('click',()=>{if(!libShare||!byId('libraryConsent').checked||libBusy)return;
        const json=JSON.stringify(libShare,null,2).replace(/`/g,'\\u0060');
        const url=new URL('https://github.com/ccachin/wordclock-themes/issues/new');
        url.searchParams.set('title','[Theme] '+libShare.name);url.searchParams.set('body','Licence : CC0 1.0 — Je confirme être autorisé à soumettre ce thème.\n\n```json\n'+json+'\n```');
        window.open(url.href,'_blank','noopener,noreferrer');libResult('Terminez la soumission sur GitHub. Aucune publication automatique.');libClose();
    });
    window.wordclockLibrary={open(id){
        if(working||libBusy||libDialog.open)return;
        const t=themes.find(t=>t.id===id);if(!t)return;const personal=id.startsWith('custom-');
        byId('libraryTitle').textContent=t.name;libActions.replaceChildren();libMsg.textContent='';libShare=null;byId('libraryShare').hidden=true;
        function action(label,fn,danger=false){const b=document.createElement('button');b.type='button';b.className='library-action'+(danger?' danger':'');b.textContent=label;b.addEventListener('click',()=>libRun(fn));libActions.appendChild(b);}
        async function saved(){const v=personal?await api('GET',id):themes.find(x=>x.id===id);if(!v||v.id!==id)throw new Error('Thème indisponible.');return v;}
        action('Appliquer',async()=>{if(!await applyTheme(id))throw new Error('Application non confirmée.');libResult('Thème appliqué.');libClose();});
        action(personal?'Modifier dans le Studio':'Personnaliser une copie',()=>libEdit(id));
        action('Dupliquer',()=>libEdit(id,true));
        if(personal)action('Partager avec la communauté',async()=>{libShare=shareTheme(await saved());validate(libShare);byId('libraryShareInfo').textContent='Version enregistrée : '+libShare.name+' — '+(libShare.author||'Auteur non renseigné')+(libShare.description?' — '+libShare.description:'')+shareBrightness(libShare);byId('libraryConsent').checked=false;byId('librarySubmit').disabled=true;byId('libraryShare').hidden=false;});
        action('Exporter',async()=>{const v=await saved();const data={...validateImport(v),id:v.id};const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)+'\n'],{type:'application/json'}));const link=document.createElement('a');link.href=url;link.download=id+'.json';try{document.body.appendChild(link);link.click();libResult('Export demandé.');libClose();}finally{link.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);}});
        if(personal)action('Supprimer',async()=>{if(!window.confirm('Supprimer définitivement « '+t.name+' » ?'))return;const result=await api('DELETE',id);if(!result.ok)throw new Error('Suppression non confirmée.');const refreshed=await refreshSources();libResult(refreshed?'Thème supprimé.':'Thème supprimé, actualisation impossible.');libClose();},true);
        libDialog.showModal();
    }};
    // Shared preview transport: one token and heartbeat for both views.
    window.wordclockCommunity = {
        validate: validateImport,
        async inspect(raw){
            const theme=validateImport(raw);
            await refreshThemeLibrary();
            const match=themes.find(t=>fingerprint(t)===fingerprint(theme));
            return {theme,existing:match||null};
        },
        preview(raw){
            if(community.hidden||working)return;
            communityPreview=validateImport(raw);previewWanted=true;pending=true;startLive();
        },
        clear(){previewWanted=false;disableLive();communityPreview=null;},
        async add(raw){
            if(working||community.hidden)throw new Error('Opération indisponible.');
            working=true;buttons();
            try{
                const theme=validateImport(raw);
                if(!await refreshSources())throw new Error('Bibliothèque indisponible.');
                const match=catalog.find(t=>fingerprint(t)===fingerprint(theme));
                const planned=match?null:planImport(theme,catalog);
                if(!match&&catalog.filter(t=>t.id.startsWith('custom-')).length>=30)throw new Error('Bibliothèque pleine (30 thèmes personnels).');
                previewWanted=false;disableLive();while(busy||stopping)await new Promise(r=>setTimeout(r,50));
                const saved=match||await api('POST','',planned);
                if(!saved.id)throw new Error('Réponse inattendue : vérifiez la bibliothèque.');
                const refreshed=await refreshSources();
                const applied=await applyTheme(saved.id);
                return (match?'Déjà présent : ':'Enregistré : ')+saved.name+(applied?' — appliqué.':' — application non confirmée.')+(refreshed?'':' Bibliothèque non actualisée.');
            }finally{working=false;buttons();}
        },
        async edit(raw){
            if(working||community.hidden)throw new Error('Opération indisponible.');
            working=true;buttons();
            try{
                const theme=validateImport(raw);
                if(!await refreshSources())throw new Error('Bibliothèque indisponible.');
                previewWanted=false;disableLive();
                showStudio(true);loadDraft({...theme,name:uniqueName(theme.name,catalog)});source.value='';imported=true;fromFile=true;previewWanted=true;
                status.textContent='Thème communautaire chargé dans le Studio.';
            }finally{working=false;buttons();}
        }
    };
    window.addEventListener('beforeunload',e=>{if(dirty()||working){e.preventDefault();e.returnValue='';}});
    buttons();
    updatePreview();requestAnimationFrame(frame);
})();
