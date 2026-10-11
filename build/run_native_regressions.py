"""Run isolated native acceptance handlers sequentially, retaining each output."""
from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
dest = root / 'build/dolphin-game-replay-final'
pid = sys.argv[1]
assert int(pid) != 9084
stages = [
    ('game-devopts', ['menu_features_smoke.py', pid, str(dest / 'native-game-devopts.json')]),
    ('spawn-pop', ['live_gc_smoke.py', pid, '--reopen', '--spawn-pop']),
    ('cameras', ['live_gc_smoke.py', pid, '--reopen', '--cameras']),
    ('progression', ['live_gc_smoke.py', pid, '--reopen', '--progression', '--progression-action']),
    ('diagnostics', ['live_gc_smoke.py', pid, '--reopen', '--diagnostics', '--play-animation', '--export-ai']),
]
for name, args in stages:
    if args[0] == 'live_gc_smoke.py':
        args.append('--output-dir=' + str(dest))
    result = subprocess.run([sys.executable, str(root / 'build' / args[0]), *args[1:]],
                            cwd=root, capture_output=True, text=True)
    (dest / (name + '-acceptance.log')).write_text(result.stdout + result.stderr)
    print(json.dumps({'stage': name, 'exit_code': result.returncode}), flush=True)
    if result.returncode:
        print(result.stdout + result.stderr, flush=True)
        sys.exit(result.returncode)
