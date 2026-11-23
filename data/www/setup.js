(() => {
'use strict';
const el = id => document.getElementById(id);
const zones = [['Europe/Zurich','Suisse — Zurich'],['Europe/Paris','France — Paris'],['Europe/Berlin','Allemagne — Berlin'],['Europe/London','Royaume-Uni — Londres'],['Europe/Lisbon','Portugal — Lisbonne'],['Europe/Athens','Grèce — Athènes'],['Etc/UTC','UTC'],['America/New_York','États-Unis — New York'],['America/Toronto','Canada — Toronto'],['America/Los_Angeles','États-Unis — Los Angeles'],['Asia/Tokyo','Japon — Tokyo']];
const colors = [['Noir','#151515'],['Blanc','#F5F5F5'],['Anthracite','#454545'],['Gris','#909090'],['Rouge','#A13835'],['Bleu','#304D70'],['Vert','#536B49'],['Terracotta','#B46B50']];
const sleep = ms => new Promise(resolve=>setTimeout(resolve,ms));
async function api(path, data) {
 const controller = new AbortController(); const timeout=setTimeout(()=>controller.abort(),5000);
 try { const response=await fetch(path,{cache:'no-store',signal:controller.signal,...(data?{method:'POST',body:new URLSearchParams(data)}:{})}); if(!response.ok) throw Error('HTTP '+response.status); return await response.json(); } finally {clearTimeout(timeout);}
}
document.addEventListener('DOMContentLoaded',()=>{
 const dialog=el('setupDialog'), name=el('setupName'), zone=el('setupZone'), color=el('setupColor'), message=el('setupMessage'), next=el('setupNext'), back=el('setupBack'), skip=el('setupSkip'), open=el('setupOpen');
 let step=0,busy=false,original=null;
 for(const [value,label] of zones) zone.add(new Option(label,value));
 for(const [label,value] of colors){const b=document.createElement('button');b.type='button';b.textContent=label;b.dataset.color=value;const sw=document.createElement('span');sw.className='face-swatch';sw.style.background=value;b.prepend(sw);b.onclick=()=>{color.value=value;paint();};el('setupPresets').append(b);}
 window.WordclockStudioVisual.matrixPreview(el('setupFace'));
 function paint(){const c=color.value;const rgb=[1,3,5].map(i=>parseInt(c.slice(i,i+2),16));const light=rgb[0]*.299+rgb[1]*.587+rgb[2]*.114;el('setupFace').style.setProperty('--face-color',c);el('setupFace').style.setProperty('--face-unlit',`rgb(${rgb.map(n=>Math.round(n*.78+(light>135?0:255)*.22)).join(',')})`);for(const b of el('setupPresets').children)b.setAttribute('aria-pressed',String(b.dataset.color.toLowerCase()===c));}
 color.oninput=paint;
 function render(){el('setupTitle').textContent=['Bienvenue','À la bonne heure','Votre façade','Vérifier votre configuration'][step];el('setupProgress').textContent=`Étape ${step+1} sur 4`;el('setupBar').value=step+1;for(const section of dialog.querySelectorAll('[data-setup-step]'))section.hidden=Number(section.dataset.setupStep)!==step;back.hidden=step===0;next.textContent=step===3?'Enregistrer et terminer':'Continuer';skip.textContent=original&&original.setupComplete?'Annuler':'Passer la configuration';el('setupSummary').textContent=`${name.value.trim()} — ${zone.options[zone.selectedIndex].text} — ${color.value}`;}
 function lock(value){busy=value;for(const control of dialog.querySelectorAll('button,input,select'))control.disabled=value;}
 function valid(){return name.value.trim()&&new TextEncoder().encode(name.value.trim()).length<=48&&!/[\x00-\x1f\x7f]/.test(name.value);}
 async function waitSaved(expected,face){for(let i=0;i<20;i++){await sleep(1000);const d=await api('/api/device');if(d.saveFailed)throw Error('Sauvegarde non confirmée');if(!d.savePending){if(Object.keys(expected).some(k=>d[k]!==expected[k]))throw Error('Paramètres modifiés ailleurs');if(face&&(await api('/api/appearance')).faceColor.toLowerCase()!==face.toLowerCase())throw Error('Couleur non confirmée');return d;}}throw Error('Délai de sauvegarde dépassé');}
 async function finish(ignore){if(busy)return;lock(true);message.textContent='Enregistrement…';try{if(!ignore){const expected={name:name.value.trim(),timezone:zone.value};await api('/api/device',expected);await api('/api/appearance',{faceColor:color.value});await waitSaved(expected,color.value);}await api('/api/device',{setupComplete:'true'});await waitSaved({setupComplete:true});location.reload();}catch(e){message.textContent='Enregistrement non confirmé. Réessayez. Des modifications peuvent déjà avoir été appliquées. ('+e.message+')';lock(false);}}
 next.onclick=()=>{if(busy)return;if(step===0&&!valid()){message.textContent='Saisissez un nom valide de 48 octets maximum.';return;}message.textContent='';if(step===3){finish(false);return;}step++;render();};
 back.onclick=()=>{if(!busy&&step>0){step--;message.textContent='';render();}};
 skip.onclick=()=>{if(busy)return;if(original.setupComplete){dialog.close();return;}finish(true);};
 dialog.addEventListener('cancel',e=>{e.preventDefault();});
 async function launch(d){const a=await api('/api/appearance');if(!/^#[0-9a-f]{6}$/i.test(a.faceColor))throw Error('Couleur invalide');original=d;name.value=d.name;zone.value=d.timezone;if(!d.setupComplete){let guess=Intl.DateTimeFormat().resolvedOptions().timeZone;if(guess==='UTC')guess='Etc/UTC';if(zones.some(z=>z[0]===guess))zone.value=guess;}color.value=a.faceColor;paint();step=0;message.textContent='';render();dialog.showModal();}
 async function refresh(auto=false){try{const d=await api('/api/device');el('deviceSummary').textContent=d.name+' — '+d.timezone;open.disabled=false;if(auto&&!d.setupComplete)await launch(d);}catch(e){el('deviceSummary').textContent='Configuration indisponible. Réessayez.';open.disabled=false;}}
 open.onclick=async()=>{open.disabled=true;try{await launch(await api('/api/device'));}catch(e){el('deviceSummary').textContent='Configuration indisponible. Réessayez.';}finally{open.disabled=false;}};
 refresh(true);el('settingsTab').addEventListener('click',()=>refresh());
},{once:true});
})();
