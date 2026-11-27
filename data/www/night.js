(() => {
    'use strict';
    const el = id => document.getElementById(id);
    const fields = el('nightFields'), enabled = el('nightEnabled'), start = el('nightStart'), end = el('nightEnd'), mode = el('nightMode'), brightness = el('nightBrightness');
    const defaults = {enabled:false,startMinute:1350,endMinute:420,brightness:13,mode:0};
    let ready = false, busy = false, dirty = false, timer;
    const toTime = n => `${String(Math.floor(n/60)).padStart(2,'0')}:${String(n%60).padStart(2,'0')}`;
    const toMinute = s => { const [h,m] = s.split(':').map(Number); return h*60+m; };
    function message(text, error=false) { el('nightMessage').textContent=text; el('nightMessage').classList.toggle('error',error); }
    function preview() {
        el('nightBrightnessValue').textContent = `${(Number(brightness.value)*100/255).toFixed(1).replace('.',',')} %`;
        el('nightBrightnessRow').hidden = mode.value === '2';
        el('nightHint').textContent = start.value === end.value ? 'Horaires identiques : le mode nuit ne s’activera pas.' : 'La luminosité nocturne est un plafond. Les horaires utilisent l’heure locale de l’horloge.';
    }
    function fill(n) { enabled.checked=n.enabled; start.value=toTime(n.startMinute); end.value=toTime(n.endMinute); mode.value=String(n.mode); brightness.value=String(n.brightness); preview(); }
    function status(n) { document.dispatchEvent(new CustomEvent("wordclock:night-status",{detail:n})); const e=el('nightStatus'); e.textContent=n.active?'Actif':n.enabled?'Programmé':'Désactivé'; e.classList.toggle('active',n.active); }
    async function request(options={}) { return window.WordclockLocal.run(async()=>{ const r=await fetch('/api/night',{cache:'no-store',...options}); if(!r.ok) throw Error(`HTTP ${r.status}`); return await r.json(); }); }
    async function poll() {
        if (!busy && !window.WordclockLocal.busy && !document.hidden) {
            busy=true;
            try { const n=await request(); status(n); if(!dirty) fill(n); if(!ready){ready=true;fields.disabled=false;message('');} }
            catch(e){el('nightStatus').textContent='Indisponible';el('nightStatus').classList.remove('active'); if(!ready) message('Mode nuit inaccessible : '+e.message,true);}
            finally {busy=false;}
        }
        setTimeout(poll,30000);
    }
    function schedule() { dirty=true;preview();clearTimeout(timer);message('Enregistrement en attente…');timer=setTimeout(save,500); }
    el('nightForm').addEventListener('input',schedule);
    el('nightForm').addEventListener('change',schedule);
    el('nightReset').addEventListener('click',()=>{fill(defaults);schedule();});
    el('nightForm').addEventListener('submit',e=>{e.preventDefault();schedule();});
    async function save(){
        if(!ready || !dirty || fields.disabled) return;
        if(!el('nightForm').checkValidity()){message('Veuillez renseigner des horaires valides.',true);return;}
        // A status poll may be in progress; let it finish before writing.
        while(busy) { await new Promise(resolve=>setTimeout(resolve,50)); }
        if(fields.disabled || !dirty) return;
        clearTimeout(timer);
        const body=new URLSearchParams({enabled:enabled.checked?'1':'0',startMinute:String(toMinute(start.value)),endMinute:String(toMinute(end.value)),brightness:brightness.value,mode:mode.value});
        busy=true;fields.disabled=true;message("Envoi en cours…");
        try { const n=await request({method:'POST',body});fill(n);status(n);dirty=false;void confirmStored(el('nightMessage'),'Paramètres appliqués'); }
        catch(e){dirty=true;message('Échec de l’enregistrement : '+e.message,true);}
        finally {busy=false;fields.disabled=false;}
    }
    poll();
})();
