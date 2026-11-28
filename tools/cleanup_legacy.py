"""Dry-run by default. Remove only unchanged, audited legacy headers from Git."""
import argparse
import hashlib
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
FILES={'include/displayPixels.h': '1fd9c9c12678518f3d5f524a17b217efc05b572e400d951c38a77eeabf6266a5', 'include/handleJSON.h': 'cd8ee88f661c73e8dc6ebe0d8038bedeafd2259a2402288aeab2fc8e6c5c8993', 'include/timeManager.h': 'c29d254b2f2dca61bef0fe33164440f3815184fe8574e9e83a0771ab162ff47c', 'include/README': 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'}

def git(*args,**kwargs):
    return subprocess.run(['git','-C',str(ROOT),*args],**kwargs)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true',help='Stage removal of the four verified files')
    a=parser.parse_args()
    for name,digest in FILES.items():
        p=ROOT/name
        if not p.is_file() or hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=digest:
            raise SystemExit('STOP: absent or changed since audit: '+name)
        if git('ls-files','--error-unmatch','--',name,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode:
            raise SystemExit('STOP: not tracked: '+name)
        if git('diff','--quiet','HEAD','--',name).returncode:
            raise SystemExit('STOP: local changes: '+name)
    # Search tracked and nonignored untracked files, except the legacy set and this tool/documentation.
    paths=['.']+[':!'+x for x in FILES]+[':!tools/cleanup_legacy.py',':!docs/lot-optimization.md']
    for name in list(FILES)[:3]:
        r=git('grep','--untracked','-n','-I','-F','-e',Path(name).name,'--',*paths,capture_output=True,text=True)
        if r.returncode!=1:
            print(r.stdout,r.stderr)
            raise SystemExit('STOP: reference found or search failed: '+name)
    for name in FILES: print('SAFE CANDIDATE:',name)
    if a.apply:
        git('rm','--',*FILES,check=True)
    else:
        print('Dry run only. Use --apply to stage the verified deletions.')

if __name__=='__main__': main()
