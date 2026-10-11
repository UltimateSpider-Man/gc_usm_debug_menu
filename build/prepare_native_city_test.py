from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from game_ini import update_scene
from debug_disc import write_disc_ini
dest = root / 'build/dolphin-game-replay-final'
ini = dest / 'game.ini'
replacement = update_scene(ini.read_bytes(), 'characterb')
ini.write_bytes(replacement)
write_disc_ini(root / 'build/debug-disc/usm-game-replay-test.iso', replacement)
(dest / 'ini-before-request.ini').write_bytes(replacement)
print('Prepared workspace-only Characterb INI; loaded City remains active until native selection.')
