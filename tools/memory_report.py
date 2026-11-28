"""Report static DRAM sections and largest DRAM symbols from the current ESP8266 ELF."""
import argparse
from pathlib import Path
import re
import subprocess
import os

ROOT=Path(__file__).resolve().parents[1]
BASELINE=58472

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf',type=Path,default=ROOT/'.pio/build/d1/firmware.elf')
    p.add_argument('--bin',type=Path,default=Path(os.environ.get('PLATFORMIO_CORE_DIR',Path.home()/'.platformio'))/'packages/toolchain-xtensa/bin')
    a=p.parse_args()
    def run(name,*args):
        exe=a.bin/('xtensa-lx106-elf-'+name+('.exe' if os.name=='nt' else ''))
        return subprocess.check_output([str(exe),*args,str(a.elf)],text=True,errors='replace')
    sections=run('size','-A')
    vals={m[0]:int(m[1]) for m in re.findall(r'^\s*(\.\S+)\s+(\d+)\s+\d+\s*$',sections,re.M)}
    if not all(k in vals for k in ('.data','.rodata','.bss')): raise SystemExit('Required sections not found')
    used=sum(vals[k] for k in ('.data','.rodata','.bss'))
    for k in ('.data','.rodata','.bss','.noinit'): print(f'{k:8} {vals.get(k,0):6} bytes')
    print(f'Static RAM (PlatformIO convention): {used}; baseline {BASELINE}; delta {used-BASELINE:+d}')
    print('Largest DRAM symbols (aliases can overlap):')
    symbols=[]
    for row in run('nm','-S','--size-sort','--radix=d','-C').splitlines():
        m=re.match(r'^(\d+)\s+(\d+)\s+([A-Za-z])\s+(.+)$',row)
        if not m: continue
        addr,n=int(m[1]),int(m[2])
        if 0x3ffe8000<=addr<0x40000000 and n: symbols.append((n,m[4]))
    for n,name in sorted(symbols,reverse=True)[:25]: print(f'{n:6}  {name}')

if __name__=='__main__': main()
