"""Check a packaged release and optionally stress-test its live HTTP interface (read only)."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import gzip
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from urllib.parse import urlsplit
from urllib.request import Request, urlopen
import zipfile
from prepare_release import verify, ROOT, versions

class Assets(HTMLParser):
    def __init__(self): super().__init__(); self.paths=[]
    def handle_starttag(self, tag, attrs):
        a=dict(attrs)
        if tag=='script' and a.get('src','').startswith('/'): self.paths.append(a['src'])
        if tag=='link' and a.get('href','').startswith('/'): self.paths.append(a['href'])

def links(html):
    p=Assets(); p.feed(html.decode('utf-8')); return p.paths

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--release',type=Path,default=ROOT/'dist'/versions()['RELEASE_VERSION'])
    p.add_argument('--host',help='Local HTTP origin, e.g. http://192.168.1.87')
    p.add_argument('--repeat',type=int,default=20,choices=range(1,51),metavar='1-50')
    a=p.parse_args(); meta=verify(a.release)
    with zipfile.ZipFile(a.release/'web.zip') as z: files={n:z.read(n) for n in z.namelist()}
    paths=links(files['index.html']); assert len(paths)==4,paths
    assert sum(urlsplit(x).path=='/app.js' for x in paths)==1
    for name,companion in [('app.js','community.js'),('style.css','night.js')]:
        assert gzip.decompress(files[companion])==files[name],name
    node=shutil.which('node')
    if node:
        for f in (ROOT/'data/www').glob('*.js'): subprocess.run([node,'--check',str(f)],check=True)
        subprocess.run([node,'--check'],input=files['app.js'],check=True)
        print('PASS JavaScript syntax')
    else: print('SKIP JavaScript syntax: Node.js absent')
    print('PASS release integrity, manifest, linked assets and gzip equivalents')
    compiler=shutil.which('g++') or shutil.which('clang++')
    if compiler:
        with tempfile.TemporaryDirectory() as tmp:
            exe=Path(tmp)/'effects-test.exe'
            subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),str(ROOT/'test/effects_native/test.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
    else: print('SKIP native effects tests: host C++ compiler absent')
    if compiler:
        with tempfile.TemporaryDirectory() as tmp:
            exe=Path(tmp)/'rules-test.exe'
            subprocess.run([compiler,'-std=c++11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),str(ROOT/'test/rules_native/test.cpp'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
    else: print('SKIP native automation tests: host C++ compiler absent')
    if not a.host:return
    host=a.host.rstrip('/'); u=urlsplit(host)
    if u.scheme!='http' or not u.hostname or u.path or u.query or u.fragment or u.username: p.error('Use a local HTTP origin')
    def get(path, encoding='gzip'):
        req=Request(host+path,headers={'Accept-Encoding':encoding,'Cache-Control':'no-cache','Connection':'close'})
        with urlopen(req,timeout=15) as r:
            data=r.read(); compressed=r.headers.get('Content-Encoding')=='gzip'
            return (gzip.decompress(data) if compressed else data),compressed
    def api(path):return json.loads(get(path)[0])
    before=api('/api/system'); assert before['webVersion']==meta['webVersion'],before
    if meta['firmwareVersion']: assert before['firmware']==meta['firmwareVersion'],before
    minheap=before['freeHeap']; minblock=before['maxFreeBlock']; prev=before['uptime']; failures=[]
    endpoints=['/api/status','/api/theme','/api/themes?version=3','/api/device','/api/appearance','/api/night','/api/transitions','/api/clock','/api/wifi','/api/animations','/api/automations']
    def asset(path):
        try: data,compressed=get(path)
        except Exception as e: raise RuntimeError(f'{path}: {e}') from e
        name=urlsplit(path).path[1:]; assert data==files[name],f'Asset mismatch: {name}'
        if name in ('app.js','style.css'): assert compressed,f'Gzip missing: {name}'
    for i in range(a.repeat):
        try:
            html,_=get('/'); assert html==files['index.html'],'HTML mismatch'
            with ThreadPoolExecutor(max_workers=4) as ex: list(ex.map(asset,paths))
            for ep in endpoints:
                try: result=api(ep)
                except Exception as e: raise RuntimeError(f'{ep}: {e}') from e
                if ep=='/api/clock':
                    assert isinstance(result['valid'],bool) and result['source'] in ('unknown','manual','ntp'),result
                    assert (bool(re.fullmatch(r'\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}',result['local'])) if result['valid'] else result['local']==''),result
                if ep=='/api/wifi': assert isinstance(result['offlineAP'],bool),result
                if ep=='/api/automations':
                    assert result['version']==1 and re.fullmatch(r'[0-9a-f]{288}',result['rules']),result
                if ep=='/api/animations':
                    assert isinstance(result['enabled'],bool) and isinstance(result['hourlyColor'],bool),result
                    for key,lo,hi in [('transition',0,11),('chime',0,2),('spatial',0,2),('spatialSpeed',0,100),('intensity',0,100),('duration',1000,5000)]:
                        assert type(result[key]) is int and lo<=result[key]<=hi,result
                    assert result['duration']%100==0 and result['transition']!=6,result
            state=api('/api/system'); assert state['uptime']>=prev,'Unexpected restart'; assert state['buildId']==before['buildId'],'Build changed'; assert state['webSlotGeneration']==before['webSlotGeneration'],'Slot changed'
            prev=state['uptime'];minheap=min(minheap,state['freeHeap']);minblock=min(minblock,state['maxFreeBlock']);print(f'PASS {i+1}/{a.repeat} RAM={state["freeHeap"]} max={state["maxFreeBlock"]}')
        except Exception as e: failures.append((i+1,str(e)));print(f'FAIL {i+1}/{a.repeat}: {e}')
    for path in paths:
        name=urlsplit(path).path[1:]
        if name in ('app.js','style.css'):
            data,compressed=get(path,'identity'); assert not compressed and data==files[name],f'Plain fallback: {name}'
    print(f'HTTP: {a.repeat-len(failures)}/{a.repeat}; sampled minimum heap={minheap}, largest block={minblock}')
    if failures: raise SystemExit(1)

if __name__=='__main__': main()
