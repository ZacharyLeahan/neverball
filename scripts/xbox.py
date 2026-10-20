#!/usr/bin/env python3
"""Deploy, verify, and launch a Neverball candidate on the physical Xbox."""
import argparse
import hashlib
import json
import os
import socket
import time
from datetime import datetime, timezone
from ftplib import FTP, error_perm, error_temp
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('action', choices=['status', 'wait', 'deploy', 'update', 'launch', 'logs'])
p.add_argument('--host', default='192.168.1.254')
p.add_argument('--directory', default='/F/neverball-upstream')
p.add_argument('--quick', action='store_true',
               help='Launch: recheck the XBE and changed files; reuse the prior verified asset install')
a = p.parse_args()
if a.directory.rstrip('/').lower() == '/f/neverball-nxdk':
    p.error('The known-good older port is protected; use a separate directory')
evidence = ROOT / 'build/hardware' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')
evidence.mkdir(parents=True, exist_ok=True)
if a.action == 'wait':
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((a.host, 21), timeout=1) as probe:
                probe.settimeout(1)
                if probe.recv(1024).startswith(b'220'):
                    print('Dashboard FTP ready', flush=True)
                    break
        except OSError:
            pass
        time.sleep(2)
    else:
        raise SystemExit('Dashboard FTP did not return within 60 seconds')
with FTP(a.host, timeout=15) as ftp:
    ftp.login(os.getenv('XBOX_FTP_USER', 'xbox'), os.getenv('XBOX_FTP_PASSWORD', 'xbox'))
    if a.action in {'status', 'wait'}:
        print(ftp.getwelcome(), ftp.nlst())
    elif a.action in {'deploy', 'update'}:
        current = None
        def mkdirs(path):
            global current
            path = path.replace('/.', '')
            if current == path:
                return
            try:
                ftp.cwd(path)
                current = path
                return
            except error_perm:
                pass
            ftp.cwd('/')
            for part in path.strip('/').split('/'):
                try:
                    ftp.cwd(part)
                except error_perm:
                    ftp.mkd(part)
                    ftp.cwd(part)
            current = path

        mkdirs(a.directory)
        # Retain evidence before replacing an earlier candidate.
        for name in ['default.xbe', 'log.txt', 'stderr.txt', 'watchdog.log']:
            try:
                data = bytearray()
                ftp.retrbinary('RETR ' + name, data.extend)
                (evidence / ('prior-' + name)).write_bytes(data)
            except error_perm:
                pass
        installed = {}
        changed = []
        baseline = {}
        if a.action == 'update':
            for record_path in sorted((ROOT / 'build/hardware').glob('*/deploy.json')):
                record = json.loads(record_path.read_text())
                if record['host'] == a.host and record['directory'] == a.directory:
                    baseline = record['files']
            if not baseline:
                raise RuntimeError('Run a full verified deploy before incremental updates')
        # Editor source files are unnecessary at runtime; some names (e.g.
        # grow+shrink.wings) are also rejected by the Xbox FTP/FATX layer.
        files = sorted(f for f in (ROOT / 'bin').rglob('*')
                       if f.is_file() and f.suffix.lower() not in {'.map', '.wings', '.obj'})
        if not (ROOT / 'bin/default.xbe').is_file():
            raise RuntimeError('Build bin/default.xbe before deployment')
        marker = 'Neverball/no-audio'
        if marker in baseline and not (ROOT / 'bin' / marker).exists():
            # Explicitly restore audio when the diagnostic marker is removed.
            ftp.delete(a.directory + '/' + marker)
        for index, file in enumerate(files, 1):
            rel = file.relative_to(ROOT / 'bin').as_posix()
            expected = hashlib.sha256(file.read_bytes()).hexdigest()
            if baseline.get(rel) == expected:
                installed[rel] = expected
                continue
            changed.append(rel)
            mkdirs(a.directory + '/' + str(Path(rel).parent))
            # Resume interrupted uploads without rewriting verified files.
            previous = hashlib.sha256()
            try:
                ftp.retrbinary('RETR ' + file.name, previous.update, 65536)
            except error_perm:
                pass
            if previous.hexdigest() != expected:
                try:
                    with file.open('rb') as source:
                        ftp.storbinary('STOR ' + file.name, source, 65536)
                except (error_perm, error_temp) as error:
                    raise RuntimeError('Upload failed for ' + rel) from error
            actual = previous
            if previous.hexdigest() != expected:
                actual = hashlib.sha256()
                ftp.retrbinary('RETR ' + file.name, actual.update, 65536)
            if actual.hexdigest() != expected:
                raise RuntimeError('Readback mismatch: ' + rel)
            installed[rel] = expected
            if index % 100 == 0:
                print(f'Verified {index}/{len(files)} files', flush=True)
        record = {'host': a.host, 'directory': a.directory, 'files': installed,
                  'mode': a.action, 'reused_verified_assets': bool(baseline),
                  'changed_files': changed}
        (evidence / 'deploy.json').write_text(json.dumps(record, indent=2) + '\n')
        print('Verified deployment:', evidence, flush=True)
    elif a.action == 'launch':
        records = sorted((ROOT / 'build/hardware').glob('*/deploy.json'))
        matching = [json.loads(r.read_text()) for r in records]
        matching = [r for r in matching if r['host'] == a.host and r['directory'] == a.directory]
        if not matching:
            raise RuntimeError('Deploy and verify the candidate before launch')
        for rel, expected in matching[-1]['files'].items():
            if a.quick and rel != 'default.xbe' and rel not in matching[-1].get('changed_files', []):
                continue
            actual = hashlib.sha256()
            ftp.retrbinary('RETR ' + a.directory + '/' + rel, actual.update, 65536)
            if actual.hexdigest() != expected:
                raise RuntimeError('Installed file changed: ' + rel)
        path = a.directory.strip('/').replace('/', '\\')
        path = path[0] + ':' + path[1:] + '\\default.xbe'
        print('Launching', path, flush=True)
        try:
            print(ftp.sendcmd('SITE RunXBE ' + path))
        except (EOFError, socket.timeout, ConnectionError) as error:
            print(type(error).__name__, 'during launch; inspect KVM for outcome')
    else:
        ftp.cwd(a.directory)
        for name in ['log.txt', 'stderr.txt', 'watchdog.log']:
            try:
                with (evidence / name).open('wb') as out:
                    ftp.retrbinary('RETR ' + name, out.write)
            except error_perm as error:
                print(name, error)
        print(evidence)
