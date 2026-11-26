// Local theme library only. Full editor is preserved in studio-legacy/.
(() => {
    'use strict';
    const el=id=>document.getElementById(id);
    const {validateImport,planImport}=window.WordclockThemeRules;
    const dialog=el('libraryDialog'), actions=el('libraryActions');
    let busy=false;
    const message=text=>{el('libraryMessage').textContent=text; if(dialog.open)el('libraryDialogMessage').textContent=text;};
    for(const [id,view] of [['homeTab','homeView'],['themesTab','libraryView'],['settingsTab','settingsView']]) {
        el(id).addEventListener('click',()=>{
            for(const v of ['homeView','libraryView','settingsView'])el(v).hidden=v!==view;
            for(const tab of ['homeTab','themesTab','settingsTab'])el(tab).setAttribute('aria-pressed',String(tab===id));
        });
    }
    function api(method,id='',data){
        return request('/api/themes/custom?version=3'+(id?'&id='+encodeURIComponent(id):''),{method,...(data?{headers:{'Content-Type':'application/json'},body:JSON.stringify(data)}:{})});
    }
    async function run(fn){
        if(busy||window.WordclockLocal.busy)return;
        busy=true;
        const buttons=[el('libraryImport'),el('libraryBackup'),el('libraryClose'),...actions.querySelectorAll('button')];
        buttons.forEach(b=>b.disabled=true);
        try{await fn();}catch(e){message('Erreur : '+e.message);}finally{busy=false;buttons.forEach(b=>b.disabled=false);}
    }
    function download(data,name){
        const url=URL.createObjectURL(new Blob([JSON.stringify(data,null,2)+'\n'],{type:'application/json'}));
        const a=document.createElement('a');a.href=url;a.download=name;document.body.append(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),30000);
    }
    async function saved(t){return t.id.startsWith('custom-')?api('GET',t.id):t;}
    window.wordclockLibrary={open(id){
        if(busy||dialog.open)return;
        const t=themes.find(x=>x.id===id);if(!t)return;
        el('libraryTitle').textContent=t.name;el('libraryDialogMessage').textContent='';actions.replaceChildren();
        function action(label,fn){const b=document.createElement('button');b.type='button';b.className='library-action';b.textContent=label;b.addEventListener('click',()=>run(fn));actions.append(b);}
        action('Appliquer',async()=>{if(!await applyTheme(id))throw Error('Application non confirmée');dialog.close();});
        action('Exporter',async()=>{download({...validateImport(await saved(t)),id},id+'.json');dialog.close();});
        if(id.startsWith('custom-'))action('Supprimer',async()=>{if(!confirm('Supprimer « '+t.name+' » ?'))return;const r=await api('DELETE',id);if(!r.ok)throw Error('Suppression non confirmée');await refreshThemeLibrary();message('Thème supprimé.');dialog.close();});
        dialog.showModal();
    }};
    el('libraryClose').addEventListener('click',()=>{if(!busy)dialog.close();});
    dialog.addEventListener('cancel',e=>{if(busy)e.preventDefault();});
    const file=el('libraryFile');
    el('libraryImport').addEventListener('click',()=>{file.value='';file.click();});
    file.addEventListener('change',()=>{const f=file.files[0];file.value='';if(!f)return;run(async()=>{
        if(f.size>256*1024)throw Error('Fichier trop volumineux');
        const raw=JSON.parse(await f.text());
        const entries=raw.format==='wordclock-theme-collection'&&raw.version===1?raw.themes:[raw];
        if(!Array.isArray(entries)||!entries.length||entries.length>30)throw Error('Collection invalide');
        const incoming=entries.map(validateImport);await refreshThemeLibrary();
        const pool=themes.slice(), planned=[];
        for(const t of incoming){const next=planImport(t,pool);if(next){planned.push(next);pool.push(next);}}
        if(themes.filter(t=>t.id.startsWith('custom-')).length+planned.length>30)throw Error('Bibliothèque pleine');
        if(!planned.length){message('Thèmes déjà présents.');return;}
        if(!confirm('Importer '+planned.length+' thème(s) ?'))return;
        let count=0;
        try{for(const t of planned){const r=await api('POST','',t);if(!r.id?.startsWith('custom-'))throw Error('Création non confirmée');count++;}}
        catch(e){throw Error(count+'/'+planned.length+' importé(s) : '+e.message+' — actualisez avant de réessayer.');}
        finally{try{await refreshThemeLibrary();}catch{}}
        message(count+'/'+planned.length+' thème(s) importé(s).');
    });});
    el('libraryBackup').addEventListener('click',()=>run(async()=>{
        await refreshThemeLibrary();const entries=[];
        for(const t of themes.filter(t=>t.id.startsWith('custom-')))entries.push({...validateImport(await saved(t)),id:t.id});
        download({format:'wordclock-theme-collection',version:1,themes:entries},'wordclock-themes.json');
    }));
})();
