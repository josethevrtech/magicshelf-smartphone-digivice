#!/usr/bin/env python3
import os,json,pathlib,subprocess,time
base=pathlib.Path(os.environ['MAGICSHELF_DATA_DIR'])
display=os.environ['DISPLAY']
for _ in range(40):
 data=json.loads(subprocess.check_output(['pw-dump'],timeout=10))
 clients={x['id'] for x in data if x['type']=='PipeWire:Interface:Client' and x.get('info',{}).get('props',{}).get('window.x11.display')==display and x.get('info',{}).get('props',{}).get('application.process.binary')=='gamescope'}
 nodes=[x for x in data if x['type']=='PipeWire:Interface:Node' and x.get('info',{}).get('props',{}).get('client.id') in clients and x.get('info',{}).get('props',{}).get('media.class')=='Video/Source']
 if len(nodes)==1:
  folder=base/'proton-data/pfx/drive_c/users/steamuser/AppData/Local/DigitalPartnerBeta'
  # Runs before the game starts, so the game cannot overwrite this edit.
  settings=folder/'some_file.json'
  preferences=base/'care-mode.json'
  music_locked=preferences.exists() and json.loads(preferences.read_text()).get('music')=='always-off'
  if settings.exists() and music_locked:
   saved=json.loads(settings.read_text())
   saved['music']=False
   saved['sfx']=True
   temp=settings.with_name('some_file.json.audio.tmp')
   temp.write_text(json.dumps(saved))
   temp.replace(settings)
  candidates=[]
  for slot in range(1,10):
   save=folder/f'digimemory{slot:02d}.json'
   try:
    if json.loads(save.read_text()).get('alive',False): candidates.append((save.stat().st_mtime,slot))
   except (OSError,ValueError): pass
  resume_slot=max(candidates)[1] if candidates else 0
  resume=bool(resume_slot)
  tmp=base/'private-source.json.tmp';tmp.write_text(json.dumps({'display':display,'node':nodes[0]['id'],'serial':int(nodes[0]['info']['props']['object.serial']),'started':time.time(),'autoresume':resume,'resume_slot':resume_slot}));tmp.replace(base/'private-source.json');break
 time.sleep(.25)
else:raise RuntimeError('Cannot identify private game display')
