#!/usr/bin/env python3
import os,pathlib,signal,sys
if len(sys.argv)!=2 or sys.argv[1] not in ('--show','--dock','--hide'):raise SystemExit('Unknown control')
data=pathlib.Path(os.environ['MAGICSHELF_DATA_DIR'])
lock=data/'instance.lock'
pid=int(lock.read_text())
if pid<=1:raise SystemExit('Creation is not running')
# Confirm the lock identifies this creation's executable, not a reused PID.
exe=pathlib.Path('/proc')/str(pid)/'exe'
if exe.resolve().name!='digitalpartner-vr':raise SystemExit('Creation is not running')
os.kill(pid,{'--show':signal.SIGUSR1,'--dock':signal.SIGUSR2,'--hide':signal.SIGRTMIN}[sys.argv[1]])
