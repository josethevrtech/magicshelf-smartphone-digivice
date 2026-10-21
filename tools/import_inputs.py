#!/usr/bin/env python3
"""Import the user's locally supplied game/model; never download or publish them."""
import argparse,hashlib,json,os,shutil,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--game',type=Path,required=True,help='Folder containing DigitalPartner.exe and data.win')
p.add_argument('--model',type=Path,required=True,help='Exported assets folder containing device.mesh and body-*.rgba')
a=p.parse_args()
for file in (a.game/'DigitalPartner.exe',a.game/'data.win',a.model/'device.mesh'):
    if not file.is_file():raise SystemExit('Missing required input: '+str(file))
target=Path.home()/'.local/share/magicshelf/creation-inputs/org.magicshelf.digital-partner'
if target.exists():raise SystemExit('Inputs already imported. Move that directory aside before replacing inputs.')
target.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(dir=target.parent) as temp:
    stage=Path(temp)/'inputs';(stage/'game').mkdir(parents=True);(stage/'assets').mkdir()
    files={a.game/'DigitalPartner.exe',a.game/'data.win'}
    for glob in ('*.dat','*.dll','*.ini','*.TTF'):files.update(a.game.glob(glob))
    for file in files:shutil.copy2(file,stage/'game'/file.name)
    assets={a.model/'device.mesh'}
    for glob in ('body-*.rgba','*.pattern','model-info.json'):assets.update(a.model.glob(glob))
    for file in assets:shutil.copy2(file,stage/'assets'/file.name)
    if not list((stage/'assets').glob('body-*.rgba')):raise SystemExit('Model has no material textures')
    index={str(file.relative_to(stage)):hashlib.file_digest(file.open('rb'),'sha256').hexdigest() for file in stage.rglob('*') if file.is_file()}
    (stage/'inputs.json').write_text(json.dumps({'source':'User supplied local files','files':index},indent=2)+'\n')
    os.replace(stage,target)
print('Imported game and model into',target)
