"""Protocol 2 robustness tests, campaign 21.A. Never activates a verified image."""
import argparse
import base64
import getpass
import hashlib
import json
import secrets
import time
from pathlib import Path
from urllib.error import HTTPError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--base', default='http://192.168.1.87')
p.add_argument('--image', required=True, type=Path)
a = p.parse_args()
image = a.image.read_bytes()
sig = a.image.with_suffix('.sig').read_bytes()
if len(sig) != 256 or not image or image[0] != 0xe9:
    p.error('Invalid image or signature format')
def basic(password):
    return 'Basic ' + base64.b64encode(('admin:' + password).encode()).decode()
auth = basic(getpass.getpass('Mot de passe administrateur : '))
wrong = basic(secrets.token_hex(24))
params = dict(size=len(image), sha256=hashlib.sha256(image).hexdigest(), confirm='INSTALL', signature=sig.hex())
base = a.base.rstrip('/') + '/api/'

def req(path, query=None, method='POST', body=None, expected=200, credential=auth, action='firmware-update'):
    headers = {'Content-Type': 'text/plain'}
    if credential is not None: headers['Authorization'] = credential
    if action is not None: headers['X-Wordclock-Action'] = action
    url = base + path + ('?' + urlencode(query) if query else '')
    try:
        with urlopen(Request(url, data=body, method=method, headers=headers), timeout=30) as f:
            code, raw = f.status, f.read()
    except HTTPError as e:
        code, raw = e.code, e.read()
    try: result = json.loads(raw)
    except (ValueError, UnicodeError): result = {}
    if code != expected:
        raise RuntimeError(f'{path}: HTTP {code}, expected {expected}; error={result.get("error", result.get("reason", "non-JSON or unspecified"))}')
    return result

def fw(path='', *args, **kwargs):
    return req('system/firmware' + ('/' + path if path else ''), *args, **kwargs)

def check(condition, message):
    if not condition: raise RuntimeError(message)

def authorize():
    for attempt in range(12):
        try: return fw('authorize', params)
        except RuntimeError as exc:
            if 'HTTP 503' not in str(exc) or 'Insufficient verification memory' not in str(exc) or attempt == 11: raise
            print('Memory recovery: retry in 5 seconds', flush=True)
            time.sleep(5)

def start(ticket):
    s = fw('start', dict(token=ticket, confirm='INSTALL'))
    check(s['offset'] == 0 and s['chunkSize'] == 512, 'Unexpected session parameters')
    return s['token'], s['chunkSize']

def status(t, expected=200): return fw('status', dict(token=t), method='GET', expected=expected)
def cancel(t): return fw('cancel', dict(token=t))
def block(t, offset, data, expected=200): return fw('chunk', dict(token=t, offset=offset), method='PUT', body=data, expected=expected)

def snapshot():
    return {k: req(k, method='GET', credential=None, action=None) for k in ('device','appearance','themes','night','transitions')}

def system(): return req('system', method='GET', credential=None, action=None)

before = system()
settings = snapshot()
print('Initial:', before['buildId'], before['uptime'], flush=True)
def unchanged():
    after = system()
    for k in ('buildId','sketchBytes','webSlot','webSlotGeneration','storageUsed','resetReason'):
        check(after[k] == before[k], 'Unexpected change: ' + k)
    check(after['uptime'] >= before['uptime'], 'Unexpected restart')
    check(snapshot() == settings, 'Configuration changed')
    print('System:', after['uptime'], after['freeHeap'], after['maxFreeBlock'], flush=True)

def passed(label): print('PASS:', label, flush=True)

active = None
try:
    pre = fw(method='GET')
    check(pre['ready'] and pre['protocol'] == 2, 'Firmware not ready or incompatible')
    fw(method='GET', credential=None, expected=401)
    fw(method='GET', credential=wrong, expected=401)
    fw(method='GET', action=None, expected=403)
    passed('21.1 administrator authentication and action header')
    ticket = authorize()
    check(ticket['expiresIn'] == 60, 'Unexpected ticket lifetime')
    print('21.2 waiting 65 seconds for ticket expiry', flush=True)
    time.sleep(65)
    fw('start', dict(token=ticket['token'], confirm='INSTALL'), expected=403)
    passed('21.2 expired authorization rejected')
    ticket = authorize()['token']
    active, chunk = start(ticket)
    check(block(active,0,image[:chunk])['offset'] == chunk, 'First block not accepted')
    fw('start', dict(token=ticket, confirm='INSTALL'), expected=409)
    fw('authorize', params, expected=409)
    fw(method='GET', expected=409)
    req('system/web-update/preflight', action='web-update', expected=409)
    req('system/web-update/start', action='web-update', expected=409)
    passed('21.6 concurrent firmware and web update requests rejected')
    fw('activate', dict(token=active, confirm='INSTALL'), expected=409)
    check(status(active)['verified'] is False, 'Session unexpectedly verified')
    passed('21.5 premature activation rejected')
    fw('status', dict(token=active), method='GET', credential=wrong, expected=401)
    status(secrets.token_hex(16), expected=403)
    block(active,chunk,image[chunk:chunk+1],expected=409)
    check(status(active)['offset']==chunk, 'Invalid block advanced offset')
    passed('Invalid session credentials and incomplete block rejected')
    cancel(active)
    status(active, expected=403)
    active = None
    fw('start', dict(token=ticket, confirm='INSTALL'), expected=403)
    passed('21.4 consumed ticket and cancelled session rejected')
    active, chunk = start(authorize()['token'])
    block(active,0,image[:chunk])
    print('21.3 waiting 65 seconds for session expiry', flush=True)
    time.sleep(65)
    status(active, expected=403)
    active = None
    check(fw(method='GET')['ready'], 'Updater not released after timeout')
    passed('21.3 abandoned session expired and updater released')
    active, chunk = start(authorize()['token'])
    block(active,0,image[:chunk])
    fw('verify',dict(token=active),expected=409)
    status(active,expected=403)
    active = None
    check(fw(method='GET')['ready'], 'Updater not released after incomplete verification')
    passed('Incomplete image verification rejected and cleaned up')
    active, chunk = start(authorize()['token'])
    block(active,0,image[:chunk])
    cancel(active)
    active = None
    passed('New session possible after interruptions')
    unchanged()
    print('PASS: campaign 21.A; no firmware installation requested or scheduled', flush=True)
finally:
    if active is not None:
        try: cancel(active)
        except Exception as exc: print('Cleanup:', exc, flush=True)
