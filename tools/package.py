#!/usr/bin/env python3
import json,os,pathlib,platform,shutil,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
sdk=pathlib.Path(os.environ.get('MAGICSHELF_SDK_DIR',root/'vendor/magicshelf'))
subprocess.run([sys.executable,str(root/'tests/test_release_metadata.py')],check=True)
creation=json.loads((root/'creation.json').read_text())
repository=creation['repository'].removeprefix('https://github.com/')
meta=json.loads((root/'module.json').read_text());meta['architecture']=platform.machine()
stage=root/'build/package'
if stage.exists():shutil.rmtree(stage)
(stage/'bin').mkdir(parents=True)
shutil.copy2(root/'build/runtime/digitalpartner-vr',stage/'bin/digitalpartner-vr')
for name in ('start','control','cleanup'):
    shutil.copy2(root/'runtime'/(name+'.py'),stage/'bin'/name)
    (stage/'bin'/name).chmod(0o755)
shutil.copytree(root/'backend',stage/'backend',ignore=shutil.ignore_patterns('__pycache__'))
# Game and model are bundled with permission confirmed by the project owner.
inputs=pathlib.Path(os.environ.get('MAGICSHELF_DIGIVICE_INPUTS',root/'build/authorized-inputs'))
if not (inputs/'game/DigitalPartner.exe').is_file() or not (inputs/'assets/device.mesh').is_file():
    raise SystemExit('Run tools/fetch_assets.py to obtain the pinned distribution inputs.')
shutil.copytree(inputs/'game',stage/'game')
shutil.copytree(inputs/'assets',stage/'assets/device')
shutil.copy2(root/'ASSET_CREDITS.md',stage/'ASSET_CREDITS.md')
shutil.copytree(root/'assets/input',stage/'assets/input')
for name in ('LICENSE','OPENVR-LICENSE.txt'):shutil.copy2(root/name,stage/name)
(stage/'module.json').write_text(json.dumps(meta,indent=2)+'\n')
subprocess.run([sys.executable,str(sdk/'tools/package_creation.py'),str(stage),'--output',str(root/'dist'),'--repository',repository],check=True)
