#!/usr/bin/env python3
"""Own the shell and backend as one creation; shutdown also cleans external runtimes."""
import json,os,pathlib,shutil,signal,subprocess,sys,time
package=pathlib.Path(__file__).resolve().parents[1]
meta=json.loads((package/'module.json').read_text())
data=pathlib.Path(os.environ['MAGICSHELF_DATA_DIR']);data.mkdir(parents=True,exist_ok=True)
phone=meta['id']=='org.magicshelf.android-phone'
inputs=pathlib.Path.home()/'.local/share/magicshelf/creation-inputs'/meta['id']
assets=data/'assets';assets.mkdir(exist_ok=True)
# Assets are read-only package/dependency content, never mutable save data.
source=package/'assets' if phone else inputs/'assets'
if not (source/('android-phone/device.mesh' if phone else 'device.mesh')).is_file():
    raise SystemExit('Model missing. Run tools/import_inputs.py from this creation repository first.')
for path in source.iterdir():
    if path.name=='input': continue
    target=assets/path.name
    if target.is_symlink(): target.unlink()
    if not target.exists(): target.symlink_to(path, target_is_directory=path.is_dir())
target=assets/'input'
if target.is_symlink(): target.unlink()
if not target.exists(): target.symlink_to(package/'assets/input',target_is_directory=True)
settings=data/('android-settings.json' if phone else 'settings.json')
if not settings.exists(): settings.write_text(json.dumps({'docked':True,'visible':True}))
for name in ('android-source.json','private-source.json'):(data/name).unlink(missing_ok=True)
if not phone:
    game=inputs/'game/DigitalPartner.exe'
    if not game.is_file(): raise SystemExit('Digital Partner game missing. Run tools/import_inputs.py first.')
    os.environ['MAGICSHELF_GAME_DIR']=str(game.parent)
else:
    subprocess.run([sys.executable,str(package/'backend/prepare_runtime.py')],check=True)
children=[];stopping=False
# Own process groups allow orderly cleanup before systemd's final cgroup sweep.
def spawn(args,env=None):
    p=subprocess.Popen(args,env=env,start_new_session=True);children.append(p);return p
def stop(*_):
    global stopping
    stopping=True
signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
try:
    env=dict(os.environ)
    for key in ('DISPLAY','WAYLAND_DISPLAY','GAMESCOPE_WAYLAND_DISPLAY'):env.pop(key,None)
    width,height=(720,1600) if phone else (480,1046)
    backend=spawn(['/usr/bin/gamescope','--backend','headless','-W',str(width),'-H',str(height),'-w',str(width),'-h',str(height),'-r','30','--force-windows-fullscreen','--', '/bin/bash',str(package/'backend/run-session.sh')],env)
    shell=spawn([str(package/'bin/digitalpartner-vr')])
    if phone: spawn([sys.executable,str(package/'backend/phone_bridge.py')])
    while not stopping:
        if any(p.poll() is not None for p in children):
            raise RuntimeError('A creation process exited; stopping its remaining processes')
        time.sleep(.2)
finally:
    # Stop producers and consumers together; no orphaned music/container remains.
    for p in reversed(children):
        if p.poll() is None:
            try:os.killpg(p.pid,signal.SIGTERM)
            except ProcessLookupError:pass
    subprocess.run([str(package/'bin/cleanup')],check=False,timeout=25)
    for p in children:
        try:p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            try:os.killpg(p.pid,signal.SIGKILL)
            except ProcessLookupError:pass
    for name in ('android-source.json','private-source.json'):(data/name).unlink(missing_ok=True)
