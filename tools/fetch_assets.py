#!/usr/bin/env python3
"""Fetch the permission-cleared, pinned build inputs without any player data."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile
import tempfile
import urllib.request

root=Path(__file__).resolve().parents[1]
info=json.loads((root/'asset-source.json').read_text())
target=root/'build/authorized-inputs'
if (target/'game/DigitalPartner.exe').is_file() and (target/'assets/device.mesh').is_file():
    raise SystemExit(0)
target.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(dir=target.parent) as temp:
    temp=Path(temp);archive=temp/'assets.tar.gz'
    with urllib.request.urlopen(info['url'],timeout=60) as source,archive.open('xb') as output:
        count=0;digest=hashlib.sha256()
        while chunk:=source.read(1024*1024):
            count+=len(chunk)
            if count>info['size_bytes']:raise RuntimeError('Asset download exceeds its declared size')
            output.write(chunk);digest.update(chunk)
    if count!=info['size_bytes'] or digest.hexdigest()!=info['sha256']:
        raise RuntimeError('Asset download verification failed')
    staged=temp/'inputs'
    with tarfile.open(archive) as tar:
        members=tar.getmembers()
        if len(members)>100 or sum(m.size for m in members)>1024*1024*1024:raise RuntimeError('Invalid input archive')
        for item in members:
            p=PurePosixPath(item.name)
            if p.is_absolute() or '..' in p.parts or p.parts[0] not in ('game','assets') or not item.isfile():raise RuntimeError('Unsafe input archive')
        tar.extractall(staged,filter='data')
    if target.exists():raise RuntimeError('Existing inputs are incomplete. Rebuild in a fresh build directory.')
    staged.rename(target)
