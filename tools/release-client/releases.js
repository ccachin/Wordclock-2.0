// Browser discovery module, intentionally not yet included in ESP web assets.
export const BASE = 'https://ccachin.github.io/Wordclock-2.0/';
const versionPattern = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-dev(?:\.(0|[1-9]\d*))?)?$/;
export function compare(a,b) {
  const x=versionPattern.exec(a), y=versionPattern.exec(b);
  if (!x || !y) throw Error('Invalid version');
  for(let i=1;i<=3;i++) { const d=Number(x[i])-Number(y[i]); if(d) return Math.sign(d); }
  const xd=a.includes('-dev'), yd=b.includes('-dev');
  return xd===yd ? (xd?Math.sign(Number(x[4]||0)-Number(y[4]||0)):0) : xd?-1:1;
}
async function json(url) {
  const r=await fetch(url,{cache:'no-store',credentials:'omit'});
  if(!r.ok) throw Error(`HTTP ${r.status}`);
  return r.json();
}
export async function discover(installed, channel='stable') {
  if(!['stable','development'].includes(channel)) throw Error('Invalid channel');
  const catalog=await json(BASE+'catalog.json');
  if(catalog.format!=='wordclock-release-catalog'||catalog.version!==1||!Array.isArray(catalog.releases)) throw Error('Invalid catalog');
  const candidates=catalog.releases.filter(r=>r.channel===channel && versionPattern.test(r.version) && r.manifest===`releases/${r.version}/release.json`).sort((a,b)=>compare(b.version,a.version));
  if(!candidates.length) return null;
  const entry=candidates[0], manifest=await json(BASE+entry.manifest);
  if(manifest.format!=='wordclock-release'||manifest.manifestVersion!==1||manifest.version!==entry.version||manifest.channel!==channel||!versionPattern.test(manifest.webVersion)) throw Error('Invalid manifest');
  const compatible=manifest.compatibility?.board==='d1' && manifest.compatibility?.webApiVersion===installed.webApiVersion;
  const web=manifest.artifacts?.web, wm=manifest.artifacts?.webManifest;
  const valid=(a,name)=>a?.path===name && Number.isSafeInteger(a.size) && a.size>0 && /^[a-f0-9]{64}$/.test(a.sha256);
  if(!valid(web,'web.zip')||!valid(wm,'web-manifest.json')) throw Error('Invalid web descriptors');
  return {manifest,compatible,webUpdate:compatible && (installed.webVersionState!=='manifest'||!installed.webVersion||compare(manifest.webVersion,installed.webVersion)>0),firmwareUpdate:false};
}
