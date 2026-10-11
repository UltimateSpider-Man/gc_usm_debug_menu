from pathlib import Path
import subprocess, sys, json
root=Path(__file__).resolve().parents[1]
dest=root/'build/dolphin-game-replay-final'
pid=sys.argv[1]
assert int(pid)!=9084
for name, args in (
 ('startup',['reach_gc_menu.py',pid,str(dest/'menu-startup.json'),'--no-bypass']),
 ('spawn',['spawn_lifecycle_probe.py',pid,str(dest/'native-spawn-lifecycle.json')]),
 ('replay',['replay_native_probe.py',pid,str(dest/'native-replay.json')])):
    completed=subprocess.run([sys.executable,str(root/'build'/args[0]),*args[1:]],cwd=root,capture_output=True,text=True)
    (dest/(name+'-probe.log')).write_text(completed.stdout+completed.stderr)
    print(json.dumps({'stage':name,'exit_code':completed.returncode}),flush=True)
    if completed.returncode:
        print(completed.stdout+completed.stderr,flush=True)
        sys.exit(completed.returncode)
