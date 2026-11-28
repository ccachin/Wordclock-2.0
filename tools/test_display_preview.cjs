// Exercise the actual home preview block with a small mocked DOM, no network.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const source=fs.readFileSync(path.resolve(__dirname,'../data/www/app.js'),'utf8');
const start=source.indexOf('// Defer initialization until all deferred scripts have executed.');
const end=source.indexOf('const transitionEnabled',start);
assert(start>=0&&end>start);
const node=()=>{const classes=new Set();return {style:{},dataset:{},hidden:false,textContent:'',classList:{toggle:(k,v)=>v?classes.add(k):classes.delete(k),add:k=>classes.add(k),remove:(...keys)=>keys.forEach(k=>classes.delete(k)),contains:k=>classes.has(k)}};};
const cells=Array.from({length:121},node),el=node();el.querySelectorAll=()=>cells;
const ids=Object.fromEntries(['homeView','homeDisplayMode','homePreviewNote','homeWeatherStatus','homeWeatherPanel'].map(k=>[k,node()]));
const document={hidden:false,querySelector:()=>el,getElementById:k=>ids[k],addEventListener:(name,fn)=>{assert.equal(name,'DOMContentLoaded');fn();}};
const v={matrixPreview:()=>{},rgb:h=>[1,3,5].map(i=>parseInt(h.slice(i,i+2),16)),css:c=>'#'+c.map(x=>Math.round(x).toString(16).padStart(2,'0')).join('')};
const local={busy:false};let timers=[],response,calls=0;
const window={WordclockStudioVisual:v,WordclockLocal:local};
const ctx={document,window,AbortSignal,request:async(p)=>{assert.equal(p,'/api/display');calls++;if(response instanceof Error)throw response;return response;},setTimeout:(fn,delay)=>timers.push({fn,delay}),updateStatus:()=>{},initialize:()=>{}};
vm.runInNewContext(source.slice(start,end),ctx);
async function step(){assert.equal(timers.length,1);await timers.shift().fn();assert.equal(timers.length,1);}
(async()=>{
 const pixels='ff0000'+'000000'.repeat(120)+'0000ff'.repeat(20);
 response={mode:1,weather:1,zone:0,temperature:35,animated:1,capped:0,pixels};
 await step();assert.equal(ids.homeDisplayMode.textContent,'Mode météo actif');assert.match(ids.homePreviewNote.textContent,/arrière/);assert.equal(cells[0].style.fill,'#ff0000');assert.equal(cells[1].classList.contains('lit'),false);assert.match(el.style.boxShadow,/#0000ff/);assert.equal(timers[0].delay,600);
 response={...response,mode:2,weather:4,animated:0,pixels:'000000'.repeat(141)};await step();assert.match(ids.homeWeatherStatus.textContent,/pause pendant le mode nuit/);assert(el.classList.contains('rear-off'));assert.equal(timers[0].delay,1500);
 response={...response,mode:3,capped:1};await step();assert.match(ids.homePreviewNote.textContent,/Luminosité limitée/);assert.match(ids.homeWeatherStatus.textContent,/pause pendant la programmation/);
 for(const [mode,weather] of [[0,0],[0,2],[0,3],[4,4],[5,4],[6,4]]){response={...response,mode,weather,capped:0};await step();assert.notEqual(ids.homeDisplayMode.textContent,'Aperçu indisponible');}
 const previous=calls;document.hidden=true;await step();document.hidden=false;local.busy=true;await step();local.busy=false;ids.homeView.hidden=true;await step();ids.homeView.hidden=false;assert.equal(calls,previous);
 response={...response,pixels:'broken'};await step();await step();assert.equal(ids.homeDisplayMode.textContent,'Aperçu indisponible');assert.equal(timers[0].delay,3000);
 response={mode:1,weather:1,zone:2,temperature:35,animated:1,capped:0,pixels};await step();assert.match(ids.homePreviewNote.textContent,/deux faces/);assert.equal(ids.homeWeatherPanel.dataset.liveWeather,'true');
 console.log('PASS home display states, RGB, weather priority messages, polling suspension and recovery');
})().catch(e=>{console.error(e);process.exitCode=1;});
