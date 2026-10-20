#!/usr/bin/env python3
"""Adapted from neverball-nxdk ed3fd13f; fetch the pinned source dependencies used by Makefile.nxdk."""
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'build/deps'
root.mkdir(parents=True, exist_ok=True)
for name, url, revision in [
    ('ogg', 'https://github.com/xiph/ogg.git', 'e1774cd77f471443541596e09078e78fdc342e4f'),
    ('vorbis', 'https://github.com/xiph/vorbis.git', '0657aee69dec8508a0011f47f3b69d7538e9d262'),
]:
    target = root / name
    if not target.exists():
        subprocess.run(['git', 'clone', url, str(target)], check=True)
    actual = subprocess.check_output(['git', '-C', str(target), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != revision:
        if subprocess.check_output(['git', '-C', str(target), 'status', '--porcelain'], text=True).strip():
            raise RuntimeError(f'{name} has local changes; refusing to switch revisions')
        subprocess.run(['git', '-C', str(target), 'fetch', url, revision], check=True)
        subprocess.run(['git', '-C', str(target), 'checkout', '--detach', revision], check=True)
    print(name, revision)
