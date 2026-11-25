(() => {
'use strict';
const el = id => document.getElementById(id);
const zones = [['Europe/Zurich','Suisse — Zurich'],['Europe/Paris','France — Paris'],['Europe/Berlin','Allemagne — Berlin'],['Europe/London','Royaume-Uni — Londres'],['Europe/Lisbon','Portugal — Lisbonne'],['Europe/Athens','Grèce — Athènes'],['Etc/UTC','UTC'],['America/New_York','États-Unis — New York'],['America/Toronto','Canada — Toronto'],['America/Los_Angeles','États-Unis — Los Angeles'],['Asia/Tokyo','Japon — Tokyo']];
const colors = [['Noir','#151515'],['Blanc','#F5F5F5'],['Anthracite','#454545'],['Gris','#909090'],['Rouge','#A13835'],['Bleu','#304D70'],['Vert','#536B49'],['Terracotta','#B46B50']];
const sleep = ms => new Promise(resolve=>setTimeout(resolve,ms));
async function api(path, data) {
 return window.WordclockLocal.run(async()=>{
 const controller = new AbortController(); const timeout=setTimeout(()=>controller.abort(),5000);
 try { const response=await fetch(path,{cache:'no-store',signal:controller.signal,...(data?{method:'POST',body:new URLSearchParams(data)}:{})}); if(!response.ok) throw Error('HTTP '+response.status); return await response.json(); } finally {clearTimeout(timeout);}
 });
}
let installPrompt=null;
window.addEventListener('beforeinstallprompt',event=>{event.preventDefault();installPrompt=event;});
window.addEventListener('appinstalled',()=>{installPrompt=null;const d=el('shortcutDialog');if(d&&d.open)d.close();});
document.addEventListener('DOMContentLoaded',()=>{
 const dialog=el('setupDialog'), name=el('setupName'), zone=el('setupZone'), color=el('setupColor'), message=el('setupMessage'), next=el('setupNext'), back=el('setupBack'), skip=el('setupSkip'), open=el('setupOpen');
 let step=0,busy=false,original=null;
 const shortcut=el('shortcutDialog');
 function offerShortcut(){
  const mobile=/Android|iPhone|iPad|iPod/i.test(navigator.userAgent)||(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);
  if((!mobile&&!installPrompt)||navigator.standalone||matchMedia('(display-mode: standalone)').matches)return;
  el('shortcutAdd').hidden=false;el('shortcutHelp').hidden=true;el('shortcutAddressHelp').hidden=true;el('shortcutClose').textContent='Plus tard';
  if(!shortcut.open)shortcut.showModal();
 }
 el('shortcutAdd').onclick=async()=>{
  if(installPrompt){const prompt=installPrompt;installPrompt=null;try{await prompt.prompt();const choice=await prompt.userChoice;if(choice.outcome==='accepted'){shortcut.close();return;}return;}catch(error){/* Native prompt unavailable: show manual instructions. */}}
  const ios=/iPhone|iPad|iPod/i.test(navigator.userAgent)||(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);
  el('shortcutHelp').textContent=ios?'Dans Safari, touchez Partager, puis Sur l’écran d’accueil.':/Android/i.test(navigator.userAgent)?'Dans Chrome, ouvrez le menu ⋮, puis Ajouter à l’écran d’accueil (ou Créer un raccourci).':'Utilisez le menu de votre navigateur pour installer cette page ou créer un raccourci, si cette option est disponible.';
  el('shortcutHelp').hidden=false;
  const note=el('shortcutAddressHelp');note.textContent=location.hostname==='wordclock.local'?'Si vous êtes dans la fenêtre de connexion Wi-Fi, ouvrez cette page dans votre navigateur habituel.':'Vous utilisez actuellement une adresse IP qui peut changer. Pour un raccourci durable, essayez l’adresse locale dans Réglages > Accéder à mon horloge avant de créer le raccourci.';note.hidden=false;
  el('shortcutAdd').hidden=true;el('shortcutClose').textContent='Terminer';
 };
 el('shortcutClose').onclick=()=>shortcut.close();
 el('accessInstall').onclick=()=>{if(navigator.standalone||matchMedia('(display-mode: standalone)').matches){el('accessMessage').textContent='Vous utilisez déjà le mode application.';return;}el('shortcutAdd').hidden=false;el('shortcutHelp').hidden=true;el('shortcutAddressHelp').hidden=true;el('shortcutClose').textContent='Plus tard';if(!shortcut.open)shortcut.showModal();};
 const handoff=new URLSearchParams(location.search).get('shortcut')==='1';
 if(handoff){const u=new URL(location.href);u.searchParams.delete('shortcut');history.replaceState(null,'',u.pathname+u.search+u.hash);}

 const deviceName=el('deviceName'), deviceZone=el('deviceZone'), deviceStatus=el('deviceSummary');
 let deviceTimer=null, deviceBusy=false, devicePending={}, deviceRevision=0;
 for(const [value,label] of zones){zone.add(new Option(label,value));deviceZone.add(new Option(label,value));}
 function validName(value){return value.trim()&&new TextEncoder().encode(value.trim()).length<=48&&!/[\x00-\x1f\x7f]/.test(value);}
 function queueDevice(key,value,delay){
  deviceRevision++;devicePending[key]=value;clearTimeout(deviceTimer);deviceStatus.textContent='Enregistrement…';open.disabled=true;
  deviceTimer=setTimeout(()=>{deviceTimer=null;saveDevice();},delay);
 }
 async function saveDevice(){
  if(deviceBusy||deviceTimer!==null)return;if(!Object.keys(devicePending).length){open.disabled=false;return;}
  const changes=devicePending, revision=deviceRevision;devicePending={};deviceBusy=true;
  try{await api('/api/device',changes);await waitSaved(changes);if(revision===deviceRevision)deviceStatus.textContent='Enregistré';}
  catch(e){if(revision===deviceRevision)deviceStatus.textContent='Enregistrement non confirmé. Modifiez à nouveau le réglage pour réessayer.';}
  finally{deviceBusy=false;if(Object.keys(devicePending).length&&deviceTimer===null)saveDevice();else if(deviceTimer===null)open.disabled=false;}
 }
 deviceName.addEventListener('input',()=>{const value=deviceName.value.trim();if(!validName(deviceName.value)){deviceRevision++;delete devicePending.name;deviceStatus.textContent='Saisissez un nom valide de 48 octets maximum.';return;}queueDevice('name',value,700);});
 deviceZone.addEventListener('change',()=>queueDevice('timezone',deviceZone.value,0));
 for(const [label,value] of colors){const b=document.createElement('button');b.type='button';b.textContent=label;b.dataset.color=value;const sw=document.createElement('span');sw.className='face-swatch';sw.style.background=value;b.prepend(sw);b.onclick=()=>{color.value=value;paint();};el('setupPresets').append(b);}
 window.WordclockStudioVisual.matrixPreview(el('setupFace'));
 function paint(){const c=color.value;const rgb=[1,3,5].map(i=>parseInt(c.slice(i,i+2),16));const light=rgb[0]*.299+rgb[1]*.587+rgb[2]*.114;el('setupFace').style.setProperty('--face-color',c);el('setupFace').style.setProperty('--face-unlit',`rgb(${rgb.map(n=>Math.round(n*.78+(light>135?0:255)*.22)).join(',')})`);for(const b of el('setupPresets').children)b.setAttribute('aria-pressed',String(b.dataset.color.toLowerCase()===c));}
 color.oninput=paint;
 function render(){el('setupTitle').textContent=['Bienvenue','À la bonne heure','Votre façade','Vérifier votre configuration'][step];el('setupProgress').textContent=`Étape ${step+1} sur 4`;el('setupBar').value=step+1;for(const section of dialog.querySelectorAll('[data-setup-step]'))section.hidden=Number(section.dataset.setupStep)!==step;back.hidden=step===0;next.textContent=step===3?'Enregistrer et terminer':'Continuer';skip.textContent=original&&original.setupComplete?'Annuler':'Passer la configuration';el('setupSummary').textContent=`${name.value.trim()} — ${zone.options[zone.selectedIndex].text} — ${color.value}`;}
 function lock(value){busy=value;for(const control of dialog.querySelectorAll('button,input,select'))control.disabled=value;}
 function valid(){return name.value.trim()&&new TextEncoder().encode(name.value.trim()).length<=48&&!/[\x00-\x1f\x7f]/.test(name.value);}
 async function waitSaved(expected,face){for(let i=0;i<20;i++){await sleep(1000);const d=await api('/api/device');if(d.saveFailed)throw Error('Sauvegarde non confirmée');if(!d.savePending){if(Object.keys(expected).some(k=>d[k]!==expected[k]))throw Error('Paramètres modifiés ailleurs');if(face&&(await api('/api/appearance')).faceColor.toLowerCase()!==face.toLowerCase())throw Error('Couleur non confirmée');return d;}}throw Error('Délai de sauvegarde dépassé');}
 async function finish(ignore){if(busy)return;lock(true);message.textContent='Enregistrement…';try{if(!ignore){const expected={name:name.value.trim(),timezone:zone.value};await api('/api/device',expected);await api('/api/appearance',{faceColor:color.value});await waitSaved(expected,color.value);}await api('/api/device',{setupComplete:'true'});await waitSaved({setupComplete:true});const first=!original.setupComplete;const u=new URL(location.href);if(first||handoff)u.searchParams.set("shortcut","1");location.replace(u.href);}catch(e){message.textContent='Enregistrement non confirmé. Réessayez. Des modifications peuvent déjà avoir été appliquées. ('+e.message+')';lock(false);}}
 next.onclick=()=>{if(busy)return;if(step===0&&!valid()){message.textContent='Saisissez un nom valide de 48 octets maximum.';return;}message.textContent='';if(step===3){finish(false);return;}step++;render();};
 back.onclick=()=>{if(!busy&&step>0){step--;message.textContent='';render();}};
 skip.onclick=()=>{if(busy)return;if(original.setupComplete){dialog.close();return;}finish(true);};
 dialog.addEventListener('cancel',e=>{e.preventDefault();});
 async function launch(d){const a=await api('/api/appearance');if(!/^#[0-9a-f]{6}$/i.test(a.faceColor))throw Error('Couleur invalide');original=d;name.value=d.name;zone.value=d.timezone;if(!d.setupComplete){let guess=Intl.DateTimeFormat().resolvedOptions().timeZone;if(guess==='UTC')guess='Etc/UTC';if(zones.some(z=>z[0]===guess))zone.value=guess;}color.value=a.faceColor;paint();step=0;message.textContent='';render();dialog.showModal();}
 async function refresh(auto=false){if(deviceBusy||deviceTimer!==null||Object.keys(devicePending).length||document.activeElement===deviceName)return;const revision=deviceRevision;try{const d=await api('/api/device');if(revision!==deviceRevision)return;deviceName.value=d.name;deviceZone.value=d.timezone;deviceName.disabled=false;deviceZone.disabled=false;deviceStatus.textContent='';open.disabled=false;if(auto&&!d.setupComplete)await launch(d);else if(auto&&handoff)offerShortcut();}catch(e){deviceStatus.textContent='Configuration indisponible. Réessayez.';open.disabled=false;}}
 open.onclick=async()=>{open.disabled=true;try{await launch(await api('/api/device'));}catch(e){el('deviceSummary').textContent='Configuration indisponible. Réessayez.';}finally{open.disabled=false;}};
 refresh(true);el('settingsTab').addEventListener('click',()=>refresh());
},{once:true});
})();
